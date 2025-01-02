/// This file contains a list of all monitored parameters.

/// The Parameter defines a configurable property which can differ between
/// tracks. Each parameter is templated with an internal type which must
/// correspond to a SQL field type (see rusqlite for details).
///
/// Parameters are defined by a string Key and a default value.
#[derive(Clone, Copy)]
pub struct Parameter<T>(pub &'static str, pub T);

// Width of the RMS window in seconds
pub const PARAM_RMS_WINDOW: Parameter<f32> = Parameter::<f32>("RmsWindow", 0.3);

// Milliseconds between attempted head/tail movement swaps
pub const PARAM_FLIP_INTERVAL: Parameter<u32> = Parameter::<u32>("FlipInterval", 500);

// Effective sample rate of the current track
pub const PARAM_SAMPLE_RATE: Parameter<u32> = Parameter::<u32>("SampleRate", 44100);

// RMS threshold at which to actuate the body
pub const PARAM_BODY_THRESHOLD: Parameter<u32> = Parameter::<u32>("BodyThreshold", 1000);

// PWM work factor for body motion (0-1)
pub const PARAM_BODY_SPEED: Parameter<f32> = Parameter::<f32>("BodySpeed", 1.0);

// RMS threshold at which to actuate the mouth
pub const PARAM_MOUTH_THRESHOLD: Parameter<u32> = Parameter::<u32>("MouthThreshold", 1000);

// PWM work factor for mouth motion (0-1)
pub const PARAM_MOUTH_SPEED: Parameter<f32> = Parameter::<f32>("MouthSpeed", 1.0);

// Time in seconds to hold the mouth open or closed before releasing the motor
pub const PARAM_MOUTH_FWD_HOLD: Parameter<f32> = Parameter::<f32>("MouthFwdHold", 0.1);
pub const PARAM_MOUTH_BWD_HOLD: Parameter<f32> = Parameter::<f32>("MouthBwdHold", 0.1);

/// All integer parameters
pub const I_PARAMS: &[Parameter<u32>] = &[
    PARAM_MOUTH_THRESHOLD,
    PARAM_BODY_THRESHOLD,
    PARAM_SAMPLE_RATE,
];

/// All float parameters
pub const F_PARAMS: &[Parameter<f32>] = &[
    PARAM_RMS_WINDOW,
    PARAM_BODY_SPEED,
    PARAM_MOUTH_SPEED,
    PARAM_MOUTH_FWD_HOLD,
    PARAM_MOUTH_BWD_HOLD,
];
