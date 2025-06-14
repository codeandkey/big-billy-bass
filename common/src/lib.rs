#[macro_use]
extern crate log;

use serde::{Deserialize, Serialize};

pub mod bus;
pub mod param;
pub mod sp;

pub type Sample = i16;

pub const GPIO_PORT: u16 = 2134;
pub const DASH_PORT: u16 = 2144;
pub const MAXSIZE: u32 = 32768;

#[derive(Serialize, Deserialize)]
pub enum GpioMessage {
    SetSampleRate(u32),
    NextFrame(Vec<Sample>, Vec<Sample>),
}

#[derive(Serialize, Deserialize)]
pub enum DashMessage {
    LimbHistory(Vec<(u128, f32, f32, f32, f32)>),
    RmsHistory(Vec<(u128, f32, f32, f32, f32)>),
    FFTData(Vec<f32>),
}
