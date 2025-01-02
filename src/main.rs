#[macro_use]
extern crate log;

use std::error::Error;
use std::path::PathBuf;

mod db;
mod error;
mod gpio;
mod gpio_main;
mod limb;
mod monitor;
mod params;
mod types;

use crate::db::Database;
use crate::gpio::{MockLogicPin, MockPwmPin};
use crate::gpio_main::GpioThread;
use crate::limb::Limb;

fn main() -> Result<(), Box<dyn Error>> {
    unsafe {
        std::env::set_var("RUST_LOG", "debug");
    }

    pretty_env_logger::init();

    let watcher = Database::connect(&PathBuf::from("parameters.db"))?;

    info!("Starting GPIO service");

    let limb_body = Limb::new(
        Box::new(MockLogicPin::new(10)),
        Box::new(MockLogicPin::new(10)),
        Box::new(MockPwmPin::new(10)),
    );

    let limb_mouth = Limb::new(
        Box::new(MockLogicPin::new(10)),
        Box::new(MockLogicPin::new(10)),
        Box::new(MockPwmPin::new(10)),
    );

    let mut handle = GpioThread::new(watcher, limb_body, limb_mouth)?.run()?;

    let mut nulls = vec![];
    for _ in 0..44100 {
        nulls.push(0i16);
    }

    handle.send_frame(nulls.clone(), nulls.clone());

    for _ in 0..10 {
        handle.send_frame(nulls.clone(), nulls.clone());
        std::thread::sleep(std::time::Duration::from_millis(1000));
    }

    handle.join();
    Ok(())
}
