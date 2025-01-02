use crate::error::Error;
use crate::gpio::{LogicPin, PwmPin};

#[cfg(test)]
use crate::gpio::{MockLogicPin, MockPwmPin};

use std::time::Instant;

#[derive(Copy, Clone)]
pub enum Direction {
    Forward,
    Backward,
    None,
}

pub struct Limb<L, P> {
    forward_pin: Box<L>,
    backward_pin: Box<L>,
    speed_pin: Box<P>,
    move_speed: f32,
    forward_hold_t: Instant,
    backward_hold_t: Instant,
    forward_hold: Option<f32>,
    backward_hold: Option<f32>,
}

impl<L: LogicPin + Send, P: PwmPin + Send> Limb<L, P> {
    /// Initialize a new limb. Limbs are controlled by 3 pins:
    /// 2 logic pins determine the diretcion of motion. When the limb is to be
    /// moved, one of <forward> or <backward> will be set to a HIGH state and
    /// the other to a LOW state.
    ///
    /// The speed pin determines the power sent to the actuating motor. A value
    /// of 1.0 will send the maximum power to the motor, and a value of 0.0 will
    /// not send any power to the motor.
    pub fn new(forward_pin: Box<L>, backward_pin: Box<L>, speed_pin: Box<P>) -> Self {
        Self {
            forward_pin,
            backward_pin,
            speed_pin,
            move_speed: 1.0,
            forward_hold: None,
            backward_hold: None,
            forward_hold_t: Instant::now(),
            backward_hold_t: Instant::now(),
        }
    }

    /// Assign the limb actuation speed (0 to 1).
    pub fn move_speed(&mut self, value: f32) -> &mut Self {
        self.move_speed = value;
        self
    }

    /// Assign or remove the forward hold setting.
    pub fn forward_hold(&mut self, value: Option<f32>) -> &mut Self {
        self.forward_hold = value;
        self
    }

    /// Assign or remove the backward hold setting.
    pub fn backward_hold(&mut self, value: Option<f32>) -> &mut Self {
        self.backward_hold = value;
        self
    }

    /// Move the limb forward, backward, or cease movement entirely. If the limb
    /// is held in a non-none direction for time exceeding the direction's
    /// _hold_ time, the motor will be released until either None or the opposite
    /// direction is applied.
    pub fn apply(&mut self, direction: Direction) {
        let mut states: Vec<Result<(), Error>> = vec![];

        let mut tp = &mut self.forward_hold_t;
        let mut backtp = &mut self.backward_hold_t;
        let mut hold = &self.forward_hold;
        let mut pin = &mut self.forward_pin;
        let mut backpin = &mut self.backward_pin;

        if let Direction::Backward = direction {
            tp = &mut self.backward_hold_t;
            backtp = &mut self.forward_hold_t;
            hold = &self.backward_hold;
            pin = &mut self.backward_pin;
            backpin = &mut self.forward_pin;
        }

        match direction {
            Direction::Forward | Direction::Backward => {
                let mut speed = self.move_speed;

                if let Some(hold) = hold {
                    if tp.elapsed().as_secs_f32() >= *hold {
                        speed = 0.0;
                    }
                }

                states.push(backpin.set_low());

                if speed > 0.0 {
                    states.push(pin.set_high());
                } else {
                    states.push(pin.set_low());
                }

                states.push(self.speed_pin.set(speed));

                *backtp = Instant::now();
            }
            Direction::None => {
                states.push(self.forward_pin.set_low());
                states.push(self.backward_pin.set_low());
                states.push(self.speed_pin.set(0.0));

                self.forward_hold_t = Instant::now();
                self.backward_hold_t = Instant::now();
            }
        }

        for s in states {
            if let Err(e) = s {
                error!("GPIO write failure: {e}");
            }
        }
    }

    /// Retrieve a reference to the forward direction pin (logic).
    #[cfg(test)]
    pub fn fwd_pin(&mut self) -> &mut Box<L> {
        &mut self.forward_pin
    }

    /// Retrieve a reference to the backward direction pin (logic).
    #[cfg(test)]
    pub fn bwd_pin(&mut self) -> &mut Box<L> {
        &mut self.backward_pin
    }

    /// Retrieve a reference to the speed control pin (pwm).
    #[cfg(test)]
    pub fn spd_pin(&mut self) -> &mut Box<P> {
        &mut self.speed_pin
    }
}

/// Tests the GPIO pin states are all set to LOW or 0% work when moving the limb
/// in the 'None' direction.
#[test]
pub fn test_limb_initial_pins() {
    let fwd = MockLogicPin::new(10);
    let bwd = MockLogicPin::new(10);
    let spd = MockPwmPin::new(10);

    let mut limb = Limb::new(Box::new(fwd), Box::new(bwd), Box::new(spd));

    limb.apply(Direction::None);

    assert_eq!(limb.fwd_pin().get(), false);
    assert_eq!(limb.bwd_pin().get(), false);
    assert_eq!(limb.spd_pin().get(), 0.0);
}

/// Tests the GPIO pin states are set to FWD HIGH, BWD LOW, and correct work
/// when moving in the forward direction.
#[test]
pub fn test_limb_forward_pins() {
    let fwd = MockLogicPin::new(10);
    let bwd = MockLogicPin::new(10);
    let spd = MockPwmPin::new(10);

    let mut limb = Limb::new(Box::new(fwd), Box::new(bwd), Box::new(spd));

    limb.move_speed(0.5);
    limb.apply(Direction::Forward);

    assert_eq!(limb.fwd_pin().get(), true);
    assert_eq!(limb.bwd_pin().get(), false);
    assert_eq!(limb.spd_pin().get(), 0.5);
}

/// Tests the GPIO pin states are set to FWD LOW, BWD HIGH, and correct work
/// when moving in the backward direction.
#[test]
pub fn test_limb_backward_pins() {
    let fwd = MockLogicPin::new(10);
    let bwd = MockLogicPin::new(10);
    let spd = MockPwmPin::new(10);

    let mut limb = Limb::new(Box::new(fwd), Box::new(bwd), Box::new(spd));

    limb.move_speed(0.5);
    limb.apply(Direction::Backward);

    assert_eq!(limb.fwd_pin().get(), false);
    assert_eq!(limb.bwd_pin().get(), true);
    assert_eq!(limb.spd_pin().get(), 0.5);
}

/// Tests the forward hold frames release the motors after a given time.
#[test]
pub fn test_limb_forward_hold() {
    let fwd = MockLogicPin::new(10);
    let bwd = MockLogicPin::new(10);
    let spd = MockPwmPin::new(10);

    let mut limb = Limb::new(Box::new(fwd), Box::new(bwd), Box::new(spd));

    limb.forward_hold(Some(0.5));
    limb.apply(Direction::Forward);

    assert_eq!(limb.fwd_pin().get(), true);
    assert_eq!(limb.bwd_pin().get(), false);
    assert_eq!(limb.spd_pin().get(), 1.0);

    std::thread::sleep(std::time::Duration::from_secs_f32(0.6));

    limb.apply(Direction::Forward);

    assert_eq!(limb.fwd_pin().get(), false);
    assert_eq!(limb.bwd_pin().get(), false);
    assert_eq!(limb.spd_pin().get(), 0.0);
}

/// Tests the backward hold frames release the motors after a given time.
#[test]
pub fn test_limb_backward_hold() {
    let fwd = MockLogicPin::new(10);
    let bwd = MockLogicPin::new(10);
    let spd = MockPwmPin::new(10);

    let mut limb = Limb::new(Box::new(fwd), Box::new(bwd), Box::new(spd));

    limb.backward_hold(Some(0.5));
    limb.apply(Direction::Backward);

    assert_eq!(limb.fwd_pin().get(), false);
    assert_eq!(limb.bwd_pin().get(), true);
    assert_eq!(limb.spd_pin().get(), 1.0);

    std::thread::sleep(std::time::Duration::from_secs_f32(0.6));

    limb.apply(Direction::Backward);

    assert_eq!(limb.fwd_pin().get(), false);
    assert_eq!(limb.bwd_pin().get(), false);
    assert_eq!(limb.spd_pin().get(), 0.0);
}
