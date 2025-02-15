#[macro_use]
extern crate log;

use core::time;
use gpio::limb::Direction;
use gpio::pin::{LogicPin, PwmPin};
use param::ParameterController;
use spin_sleep;
use std::error::Error;
use std::path::{Path, PathBuf};
use std::time::Instant;

mod error;
mod gpio;
mod param;
mod signal_processing;
mod types;

use crate::gpio::GpioThread;
use crate::gpio::limb::Limb;

// TODO: move saved params to .local config and just use tmp for current link
const PARAM_ROOT: &str = "/tmp/billy";

fn set_track(track: &str) {
    // TODO: Move this behavior to the dbus watcher
    let root = PathBuf::from(PARAM_ROOT);

    std::fs::remove_file(root.join("current")).ok();

    if let Err(e) = std::os::unix::fs::symlink(root.join(track), root.join("current")) {
        warn!("Failed creating current track link: {:?}", e);
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    unsafe {
        let cur = std::env::var("RUST_LOG").unwrap_or_else(|_| "debug".to_string());
        std::env::set_var("RUST_LOG", cur);
    }

    pretty_env_logger::init();

    let watcher = ParameterController::new(Path::new(PARAM_ROOT))?;
    set_track("defaulttestABC");

    let limb_body = Limb::new(
        LogicPin::new(17)?,
        LogicPin::new(27)?,
        PwmPin::new(0)?,
    );

    let limb_mouth = Limb::new(
        LogicPin::new(10)?,
        LogicPin::new(10)?,
        PwmPin::new(10)?,
    );

    let handle = GpioThread::new(watcher, limb_body, limb_mouth)?.run()?;

    let mut sig_processor = signal_processing::AudioNode::new(
        ParameterController::new(Path::new(PARAM_ROOT))?,
        "BBB",
        handle,
    );

    let mut log_timer = Instant::now();
    let log_time = 5;
    let mut samples = 0;
    let mut update_interval_actual = 0;
    let mut loop_count = 0;
    let mut overhead_adder: f32 = 0.0;
    let mut total_err = 0.0;
    let overhead_adder_scalar: f32 = 0.01;

    loop {
        // call updated
        let start = Instant::now();
        let micros = sig_processor.update()?;

        // timing nonsense
        samples += micros * 44100 / 1_000_000;
        update_interval_actual += micros;

        if overhead_adder > 0.0 {
            spin_sleep::sleep(
                time::Duration::from_micros(micros)
                    .saturating_add(time::Duration::from_micros(overhead_adder as u64)),
            );
        } else {
            spin_sleep::sleep(
                time::Duration::from_micros(micros)
                    .saturating_sub(time::Duration::from_micros((-overhead_adder) as u64)),
            );
        }
        loop_count += 1;
        let err = micros as f32 - start.elapsed().as_micros() as f32;
        total_err += err;
        overhead_adder += err * overhead_adder_scalar;

        if log_timer.elapsed().as_secs() >= log_time && micros != 1000 {
            debug!(
                "Samples a second: {}, uS / interval {}, err {} , adder {}",
                samples / log_time,
                update_interval_actual / loop_count,
                total_err / loop_count as f32,
                overhead_adder as i64
            );
            total_err = 0.0;
            samples = 0;
            loop_count = 0;
            update_interval_actual = 0;
            log_timer = Instant::now();
        }
    }
}
