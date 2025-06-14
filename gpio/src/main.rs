#[macro_use]
extern crate log;

pub mod gpio;
use common::{
    bus::{BusReceiver, BusSender},
    sp::MovingRms,
};
use gpio::limb::{Direction, Limb};
use gpio::pin::{LogicPin, PwmPin};

use std::time::{Duration, Instant};

use common::param::*;
use common::*;
use std::error::Error;

const REPORT_TIME_S: f32 = 5.0;

// Actual rate will be less due to overhead. Set this a bit above the target rate
pub struct GpioProc {
    pc: ParameterController,
    rx: BusReceiver<GpioMessage>,
    limb_body: Limb,
    limb_mouth: Limb,
    body_direction: Direction,
    body_direction_last_flip: Instant,
    last_report_time: Instant,
    start_time: Instant,
    write_count: u64,
    dash_writer: BusSender<DashMessage>,
    am_body_rms: MovingRms,
    am_mouth_rms: MovingRms,
    mouth_thresh: f32,
    body_thresh: f32,
}

impl GpioProc {
    pub fn new(limb_body: Limb, limb_mouth: Limb) -> Result<Self, Box<dyn Error>> {
        Ok(Self {
            pc: ParameterController::new()?,
            rx: BusReceiver::new(GPIO_PORT)?,
            limb_body,
            limb_mouth,
            body_direction: Direction::Forward,
            body_direction_last_flip: Instant::now(),
            write_count: 0,
            last_report_time: Instant::now(),
            start_time: Instant::now(),
            dash_writer: BusSender::new(DASH_PORT)?,
            am_body_rms: MovingRms::new_w_size(44100 / 200),
            am_mouth_rms: MovingRms::new_w_size(44100 / 200),
            mouth_thresh: 0.0,
            body_thresh: 0.0,
        })
    }

    fn handle_frame(&mut self, lpf: &Vec<i16>, hpf: &Vec<i16>) -> Result<(), Box<dyn Error>> {
        assert_eq!(lpf.len(), hpf.len());

        let f_start = Instant::now();
        let mut ctr = 0;

        let s_rate = 44100;
        let f_rate = self.pc.get::<u32>(&PARAM_GPIO_RATE);
        let report_elapsed = self.last_report_time.elapsed().as_secs_f32();

        let mut dash_pin_points = Vec::<(u128, f32, f32, f32, f32)>::with_capacity(
            (lpf.len() as u32 * f_rate / s_rate) as usize,
        );

        let mut dash_rms_points = Vec::<(u128, f32, f32, f32, f32)>::with_capacity(
            (lpf.len() as u32 * f_rate / s_rate) as usize,
        );

        if report_elapsed > REPORT_TIME_S {
            let rate = self.write_count as f32 / report_elapsed;
            info!(
                "GPIO: {:.1} w/s ({}%)",
                rate,
                (rate * 100.0 / s_rate as f32) as u8,
            );

            self.last_report_time = Instant::now();
            self.write_count = 0;
        }

        loop {
            let elapsed_us = f_start.elapsed().as_micros() as u32;
            let ind = ((s_rate * elapsed_us) / 1_000_000) as usize;

            if ind >= lpf.len() {
                // Send processed frame over to dash

                self.dash_writer
                    .send(DashMessage::LimbHistory(dash_pin_points))?;
                self.dash_writer
                    .send(DashMessage::RmsHistory(dash_rms_points))?;

                return Ok(());
            }

            self.handle_sample(&lpf[ind], &hpf[ind], ctr)?;
            ctr += 1;

            dash_pin_points.push((
                self.start_time.elapsed().as_millis(),
                self.limb_body.get_actuation(),
                self.limb_mouth.get_actuation(),
                0.0,
                0.0,
            ));

            dash_rms_points.push((
                self.start_time.elapsed().as_millis(),
                lpf[ind] as f32,
                hpf[ind] as f32,
                self.body_thresh,
                self.mouth_thresh,
            ));

            self.pc.set(PARAM_BODY_THRESHOLD, self.body_thresh)?;
            self.pc.set(PARAM_MOUTH_THRESHOLD, self.mouth_thresh)?;

            spin_sleep::sleep(Duration::from_micros((1_000_000 / f_rate.max(1)).into()));
        }
    }

    /// Advances the GPIO thread by one step. The current RMS state is computed
    /// and the limbs are updated accordingly.
    fn handle_sample(&mut self, l: &Sample, h: &Sample, c: u32) -> Result<u64, Box<dyn Error>> {
        let start = Instant::now();
        let rms = (*l as f32, *h as f32);
        self.write_count += 1;

        let body_speed = self.pc.get(PARAM_BODY_SPEED);

        if self.pc.get(PARAM_AUTO_MODE) {
            let thresh: f32 = self.pc.get(PARAM_AUTO_MODE_THRESH);
            self.body_thresh = (self.am_mouth_rms.update(rms.0) * thresh / 100.0).max(100.0);
            self.mouth_thresh = (self.am_body_rms.update(rms.1) * thresh / 100.0).max(100.0);
        } else {
            self.mouth_thresh = self.pc.get(PARAM_MOUTH_THRESHOLD);
            self.body_thresh = self.pc.get(PARAM_BODY_THRESHOLD);
        }

        self.limb_body.move_speed(body_speed);

        if rms.0 >= self.body_thresh {
            self.limb_body.apply(self.body_direction);
            if c % 200 == 0 {
                trace!(
                    "body lpf {:6} >= {:6}, apply spd {:3} dir {:?}",
                    rms.0, self.body_thresh, body_speed, self.body_direction
                );
            }
        } else {
            self.limb_body.apply(Direction::None);
            if c % 200 == 0 {
                trace!(
                    "body lpf {:6} <  {:6}, apply spd {:3} dir None",
                    rms.0, self.body_thresh, body_speed
                );
            }

            if self.body_direction_last_flip.elapsed().as_millis()
                > self.pc.get(PARAM_FLIP_INTERVAL)
            {
                self.body_direction = match self.body_direction {
                    Direction::Forward => Direction::Backward,
                    Direction::Backward => Direction::Forward,
                    Direction::None => unreachable!(),
                };

                self.body_direction_last_flip = Instant::now();
            }
        }

        self.limb_mouth
            .forward_hold(Some(self.pc.get(PARAM_MOUTH_FWD_HOLD)));
        self.limb_mouth
            .backward_hold(Some(self.pc.get(PARAM_MOUTH_BWD_HOLD)));
        self.limb_mouth.move_speed(self.pc.get(PARAM_MOUTH_SPEED));

        if rms.1 > self.mouth_thresh {
            self.limb_mouth.apply(Direction::Forward);
        } else {
            self.limb_mouth.apply(Direction::Backward);
        }

        Ok(start.elapsed().as_micros() as u64)
    }

    fn main(&mut self) -> Result<(), Box<dyn Error>> {
        debug!("Waiting for messages.");
        loop {
            match self.rx.recv()? {
                Some(GpioMessage::NextFrame(lpf, hpf)) => self.handle_frame(&lpf, &hpf)?,
                Some(GpioMessage::SetSampleRate(_rate)) => warn!("SetSampleRate not implemented"),
                None => {}
            }
        }
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    color_eyre::install()?;

    unsafe {
        let cur = std::env::var("RUST_LOG").unwrap_or_else(|_| "debug".into());
        std::env::set_var("RUST_LOG", cur);
    }

    pretty_env_logger::init();
    rust_pigpio::initialize()?;

    let limb_body = Limb::new(LogicPin::new(17)?, LogicPin::new(27)?, PwmPin::new(12)?);

    let limb_mouth = Limb::new(LogicPin::new(24)?, LogicPin::new(25)?, PwmPin::new(13)?);

    Ok(GpioProc::new(limb_body, limb_mouth)?.main()?)
}
