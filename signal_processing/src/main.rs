#[macro_use]
extern crate log;

mod signal_processing;
use std::{error::Error, time::{self, Instant}};

use signal_processing::*;

fn main() -> Result<(), Box<dyn Error>> {
    unsafe {
        let cur = std::env::var("RUST_LOG").unwrap_or_else(|_| "debug".to_string());
        std::env::set_var("RUST_LOG", cur);
    }

    pretty_env_logger::init();

    let mut sig_processor = AudioNode::new(
        "BBB"
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