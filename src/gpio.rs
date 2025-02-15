use std::sync::mpsc::{Receiver, Sender, TryRecvError, channel};

pub mod limb;
pub mod pin;

use std::thread;
use std::time::{Duration, Instant};

use crate::error::Error;
use crate::param::*;
use crate::types::Sample;

use limb::{Direction, Limb};
use pin::{LogicPin, PwmPin};

const REPORT_TIME_S: f32 = 5.0;
const INTERRUPT_WARN_S: f32 = 0.1;

pub enum GpioMessage {
    NextFrame(Vec<Sample>, Vec<Sample>),
    Stop,
}

pub struct GpioThreadHandle {
    tx: Sender<GpioMessage>,
    handle: Option<thread::JoinHandle<()>>,
}

impl GpioThreadHandle {
    pub fn send_frame(&mut self, lpf: Vec<Sample>, hpf: Vec<Sample>) {
        if let Err(_) = self.tx.send(GpioMessage::NextFrame(lpf, hpf)) {
            warn!("Failed writing frames to GPIO handle: already hung up");
        }
    }

    pub fn join(&mut self) {
        if let Err(_) = self.tx.send(GpioMessage::Stop) {
            warn!("Failed writing stop message to GPIO handle: already hung up");
        }

        self.handle
            .take()
            .unwrap()
            .join()
            .expect("Failed to join GPIO thread");
    }
}

pub struct GpioThread {
    pc: ParameterController,
    limb_body: Limb,
    limb_mouth: Limb,
    body_direction: Direction,
    body_direction_last_flip: Instant,
    last_report_time: Instant,
    write_count: u64,
}

impl GpioThread {
    pub fn new(
        pc: ParameterController,
        limb_body: Limb,
        limb_mouth: Limb,
    ) -> Result<Self, Error> {
        Ok(Self {
            pc,
            limb_body,
            limb_mouth,
            body_direction: Direction::Forward,
            body_direction_last_flip: Instant::now(),
            write_count: 0,
            last_report_time: Instant::now(),
        })
    }

    fn msg(&mut self, msg: GpioMessage) -> Result<bool, Error> {
        match msg {
            GpioMessage::NextFrame(lpf, hpf) => {
                assert_eq!(lpf.len(), hpf.len());

                let f_start = Instant::now();
                let mut last_written_i = 0;

                let s_rate = self.pc.get::<u32>(&PARAM_SAMPLE_RATE);
                let report_elapsed = self.last_report_time.elapsed().as_secs_f32();

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

                    if ind <= last_written_i {
                        spin_sleep::sleep(Duration::from_micros((500_000 / s_rate).into()));
                        continue;
                    }

                    last_written_i = ind;

                    if ind >= lpf.len() {
                        return Ok(false); // normal frame termination
                    }

                    self.handle_sample(&lpf[ind], &hpf[ind])?;
                }
            }
            GpioMessage::Stop => {
                return Ok(true);
            }
        }
    }

    /// Advances the GPIO thread by one step. The current RMS state is computed
    /// and the limbs are updated accordingly.
    fn handle_sample(&mut self, l: &Sample, h: &Sample) -> Result<u64, Error> {
        let start = Instant::now();
        let rms = (*l as f32, *h as f32);
        self.write_count += 1;

        self.limb_body.move_speed(self.pc.get(PARAM_BODY_SPEED));

        if rms.0 > self.pc.get(PARAM_BODY_THRESHOLD) {
            self.limb_body.apply(self.body_direction);
        } else {
            self.limb_body.apply(Direction::None);

            if self.body_direction_last_flip.elapsed().as_secs() > self.pc.get(PARAM_FLIP_INTERVAL)
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

        if rms.1 > self.pc.get(PARAM_MOUTH_THRESHOLD) {
            self.limb_mouth.apply(Direction::Forward);
        } else {
            self.limb_mouth.apply(Direction::Backward);
        }

        Ok(start.elapsed().as_micros() as u64)
    }

    fn main(mut self, rx: Receiver<GpioMessage>) -> Result<(), Error> {
        let mut interrupt_time = Instant::now();
        let mut interrupt_warn = false;

        loop {
            match rx.try_recv() {
                Ok(msg) => {
                    interrupt_warn = false;
                    interrupt_time = Instant::now();

                    if self.msg(msg)? {
                        break;
                    }
                }
                Err(TryRecvError::Empty) => {
                    if !interrupt_warn && interrupt_time.elapsed().as_secs_f32() > INTERRUPT_WARN_S
                    {
                        interrupt_warn = true;
                        warn!("GPIO stream interrupted");
                    }
                    spin_sleep::sleep(Duration::from_millis(1));
                }
                Err(e) => Err(Error::from(e))?,
            }
        }

        Ok(())
    }

    pub fn run(self) -> Result<GpioThreadHandle, Error> {
        let (tx, rx) = channel();
        let handle = thread::spawn(move || match self.main(rx) {
            Ok(_) => info!("GPIO thread terminating normally"),
            Err(e) => {
                error!("GPIO thread terminating with error: {e}");
            }
        });

        Ok(GpioThreadHandle {
            tx,
            handle: Some(handle),
        })
    }
}
