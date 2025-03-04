#[macro_use]
extern crate log;

mod signal_processing;
use std::{
    error::Error,
    time::{self, Instant},
};

use signal_processing::*;

const SPIN_LOCK_TIMEOUT_MS: u64 = 1000;
fn main() -> Result<(), Box<dyn Error>> {
    unsafe {
        let cur = std::env::var("RUST_LOG").unwrap_or_else(|_| "debug".to_string());
        std::env::set_var("RUST_LOG", cur);
    }

    pretty_env_logger::init();

    let mut sig_processor = AudioNode::new("BBB");

    let mut log_timer = Instant::now();
    let log_time = 5;
    let mut samples = 0;

    let mut micros = 0;
    let mut spin_loop_timeout;
    loop {
        spin_loop_timeout = Instant::now();
        while micros == 0 {
            if spin_loop_timeout.elapsed() > time::Duration::from_millis(SPIN_LOCK_TIMEOUT_MS) {
                std::thread::sleep(time::Duration::from_millis(SPIN_LOCK_TIMEOUT_MS / 10));
            }
            micros = sig_processor.update()?;
        }
        std::thread::sleep(time::Duration::from_micros(micros / 2));

        samples += micros * 44100 / 1_000_000;
        micros = 0;

        if log_timer.elapsed().as_secs() >= log_time && micros != 1000 {
            debug!("Samples a second: {}", samples / log_time,);
            samples = 0;
            log_timer = Instant::now();
        }
    }
}
