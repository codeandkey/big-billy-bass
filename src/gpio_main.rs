use std::sync::mpsc::{Receiver, Sender, TryRecvError, channel};

use std::collections::LinkedList;
use std::thread;
use std::time::Instant;

use crate::db::Database;
use crate::error::Error;
use crate::gpio::{LogicPin, PwmPin};
use crate::limb::{Direction, Limb};
use crate::monitor::Monitor;
use crate::params::*;
use crate::types::Sample;

const REPORT_TIME_S: f32 = 1.0;

pub enum GpioMessage {
    NextFrame(Vec<Sample>, Vec<Sample>),
    Stop,
}

pub struct GpioThreadHandle {
    tx: Sender<GpioMessage>,
    handle: thread::JoinHandle<()>,
}

impl GpioThreadHandle {
    pub fn send_frame(&mut self, lpf: Vec<Sample>, hpf: Vec<Sample>) {
        if let Err(_) = self.tx.send(GpioMessage::NextFrame(lpf, hpf)) {
            warn!("Failed writing frames to GPIO handle: already hung up");
        }
    }

    pub fn join(self) {
        if let Err(_) = self.tx.send(GpioMessage::Stop) {
            warn!("Failed writing stop message to GPIO handle: already hung up");
        }

        self.handle.join().expect("Failed to join GPIO thread");
    }
}

pub struct GpioThread<L, P> {
    db: Database,
    limb_body: Limb<L, P>,
    limb_mouth: Limb<L, P>,
    mon_body_thresh: Monitor<u32>,
    mon_mouth_thresh: Monitor<u32>,
    mon_rms_window: Monitor<f32>,
    mon_sample_rate: Monitor<u32>,
    mon_body_speed: Monitor<f32>,
    mon_mouth_speed: Monitor<f32>,
    mon_mouth_fwd_hold: Monitor<f32>,
    mon_mouth_bwd_hold: Monitor<f32>,
    mon_flip_interval: Monitor<u32>,
    frames: LinkedList<(Vec<Sample>, Vec<Sample>)>,
    body_direction: Direction,
    body_direction_last_flip: Instant,
    buffer_start_time: Instant, // The instant of the oldest chunk in the buffer
    last_report_time: Instant,
    write_count: u64,
}

impl<L: LogicPin + Send + 'static, P: PwmPin + Send + 'static> GpioThread<L, P> {
    pub fn new(db: Database, limb_body: Limb<L, P>, limb_mouth: Limb<L, P>) -> Result<Self, Error> {
        Ok(Self {
            db,
            limb_body,
            limb_mouth,
            mon_body_thresh: Monitor::new(PARAM_BODY_THRESHOLD),
            mon_mouth_thresh: Monitor::new(PARAM_MOUTH_THRESHOLD),
            mon_rms_window: Monitor::new(PARAM_RMS_WINDOW),
            mon_sample_rate: Monitor::new(PARAM_SAMPLE_RATE),
            mon_body_speed: Monitor::new(PARAM_BODY_SPEED),
            mon_mouth_speed: Monitor::new(PARAM_MOUTH_SPEED),
            mon_flip_interval: Monitor::new(PARAM_FLIP_INTERVAL),
            mon_mouth_fwd_hold: Monitor::new(PARAM_MOUTH_FWD_HOLD),
            mon_mouth_bwd_hold: Monitor::new(PARAM_MOUTH_BWD_HOLD),
            body_direction: Direction::Forward,
            body_direction_last_flip: Instant::now(),
            buffer_start_time: Instant::now(),
            frames: LinkedList::new(),
            write_count: 0,
            last_report_time: Instant::now(),
        })
    }

    fn msg(&mut self, msg: GpioMessage) -> Result<bool, Error> {
        match msg {
            GpioMessage::NextFrame(lpf, hpf) => {
                assert_eq!(lpf.len(), hpf.len());

                if self.frames.len() == 0 {
                    self.buffer_start_time = Instant::now();
                    info!("Stream started, first frame {} samples", lpf.len());
                }

                self.frames.push_back((lpf, hpf));
            }
            GpioMessage::Stop => {
                return Ok(true);
            }
        }

        Ok(false)
    }

    /// Advances the GPIO thread by one step. The current RMS state is computed
    /// and the limbs are updated accordingly.
    fn step(&mut self) -> Result<(), Error> {
        let report_elapsed = self.last_report_time.elapsed().as_secs_f32();
        if report_elapsed > REPORT_TIME_S {
            info!(
                "GPIO: {} buffered {:.1} writes/s",
                self.frames.len(),
                self.write_count as f32 / report_elapsed
            );

            self.last_report_time = Instant::now();
            self.write_count = 0;
        }

        if self.frames.is_empty() {
            return Ok(());
        }

        self.write_count += 1;

        let rms = self.rms()?;
        let body_thres = self.mon_body_thresh.sync(&self.db)?;
        let mouth_thres = self.mon_mouth_thresh.sync(&self.db)?;

        self.limb_body
            .move_speed(self.mon_body_speed.sync(&self.db)?);

        if rms.0 > body_thres as f32 {
            self.limb_body.apply(self.body_direction);
        } else {
            self.limb_body.apply(Direction::None);

            if self.body_direction_last_flip.elapsed().as_secs()
                > self.mon_flip_interval.sync(&self.db)? as u64
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
            .forward_hold(Some(self.mon_mouth_fwd_hold.sync(&self.db)?));
        self.limb_mouth
            .backward_hold(Some(self.mon_mouth_bwd_hold.sync(&self.db)?));
        self.limb_mouth
            .move_speed(self.mon_mouth_speed.sync(&self.db)?);

        if rms.1 > mouth_thres as f32 {
            self.limb_mouth.apply(Direction::Forward);
        } else {
            self.limb_mouth.apply(Direction::Backward);
        }

        Ok(())
    }

    /// Returns the most recent RMS value given the current chunk buffer and the
    /// current time.
    ///
    /// If the current time point lies outside of the buffered chunks, the RMS
    /// is considered to be 0 for both channels (no signal).
    fn rms(&mut self) -> Result<(f32, f32), Error> {
        // Find the time point to end the RMS window
        let sample_rate = self.mon_sample_rate.sync(&self.db)? as u128;

        let us_to_samples = |us: u128| -> u128 { us * sample_rate / 1_000_000 };

        // The length of the RMS window in microseconds
        let rms_window_us = (self.mon_rms_window.sync(&self.db)? * 1_000_000.0) as u128;
        let rms_window_samples = us_to_samples(rms_window_us);

        // The sample boundaries for the RMS window relative to the beginning of
        // the stream
        let rms_window_smp_end = us_to_samples(self.buffer_start_time.elapsed().as_micros());
        let rms_window_smp_start = rms_window_smp_end.saturating_sub(rms_window_samples);

        let mut cursor: u128 = 0;
        let mut rms = (0.0, 0.0);
        let mut count = 0;
        let mut unused = 0;

        for frame in self.frames.iter() {
            let current_frame_smp_start = cursor;
            let current_frame_smp_end = cursor + frame.0.len() as u128 - 1; // inclusive
            cursor = current_frame_smp_end;

            // Determine if this frame has expired
            if rms_window_smp_start >= current_frame_smp_end {
                unused += 1;
                continue;
            }

            // Calculate the window overlap and update the running calculation
            let subwindow_smp_start =
                rms_window_smp_start.clamp(current_frame_smp_start, current_frame_smp_end);

            let subwindow_smp_end =
                rms_window_smp_end.clamp(current_frame_smp_start, current_frame_smp_end);

            trace!(
                "Sampling RMS: {} frames, smp range [{} {}], subwin [{} {}]",
                self.frames.len(),
                current_frame_smp_start,
                current_frame_smp_end,
                subwindow_smp_start,
                subwindow_smp_end,
            );

            if subwindow_smp_start < subwindow_smp_end {
                // Reposition the subwindow in frame-local sample coordinates
                let subwindow_local_start = subwindow_smp_start - current_frame_smp_start;
                let subwindow_local_end = subwindow_smp_end - current_frame_smp_start;

                //assert!(subwindow_local_start >= 0);
                assert!(subwindow_local_end < frame.0.len() as u128);

                for i in subwindow_local_start..subwindow_local_end {
                    rms.0 += (frame.0[i as usize].abs() as u32).pow(2) as f32;
                    rms.1 += (frame.1[i as usize].abs() as u32).pow(2) as f32;
                    count += 1; // probably unroll
                }
            }
        }

        for _ in 0..unused {
            self.buffer_start_time += std::time::Duration::from_micros(
                (self.frames.front().unwrap().0.len() as u128 * 1_000_000 / sample_rate) as u64,
            );

            self.frames.pop_front();

            if self.frames.len() == 0 {
                info!("Stream interrupted")
            }
        }

        rms = (rms.0.sqrt() / count as f32, rms.1.sqrt() / count as f32);
        Ok(rms)
    }

    fn main(mut self, rx: Receiver<GpioMessage>) -> Result<(), Error> {
        loop {
            match rx.try_recv() {
                Ok(msg) => {
                    if self.msg(msg)? {
                        break;
                    }
                }
                Err(TryRecvError::Empty) => self.step()?,
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

        Ok(GpioThreadHandle { tx, handle })
    }
}
