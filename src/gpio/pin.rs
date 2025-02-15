use crate::error::Error;

pub enum LogicLevel {
    HIGH,
    LOW
}

#[cfg(feature="gpio")]
pub struct LogicPin {
    pin: rppal::gpio::OutputPin,
}

#[cfg(feature="gpio")]
impl LogicPin {
    pub fn new(pn: u8) -> Result<Self, Error> {
        Ok(Self {
            pin: rppal::gpio::Gpio::new()?.get(pn)?.into_output_low(),
        })
    }

    pub fn set(&mut self, level: LogicLevel) -> Result<(), Error> {
        match level {
            LogicLevel::HIGH => self.pin.set_high()?,
            LogicLevel::LOW => self.pin.set_high()?,
        }

        Ok(())
    }
}

#[cfg(feature="gpio")]
impl Drop for LogicPin {
    pub fn drop(&mut self) {
        self.pin.set_low().ok();
    }
}

#[cfg(not(feature="gpio"))]
pub struct LogicPin {
    v: bool
}

#[cfg(not(feature="gpio"))]
impl LogicPin {
    pub fn new(pn: u8) -> Result<Self, Error> {
        warn!("Mocking logic pin on PN {pn}");
        Ok(LogicPin { v: false })
    }

    pub fn set(&mut self, level: LogicLevel) -> Result<(), Error> {
        self.v = match level {
            LogicLevel::HIGH => true,
            LogicLevel::LOW => false,
        };

        Ok(())
    }

    #[cfg(test)]
    pub fn get(&self) -> bool {
        self.v
    }
}

#[cfg(feature="gpio")]
pub struct PwmPin {
    pin: rppal::pwm::Pwm,
}

#[cfg(feature="gpio")]
impl PwmPin {
    pub fn new(channel: u8) -> Result<Self, Error> {
        let chan = match channel {
            0 => rppal::pwm::Channel::PWM0,
            1 => rppal::pwm::Channel::PWM1,
            2 => rppal::pwm::Channel::PWM2,
            3 => rppal::pwm::Channel::PWM3,
        };

        Ok(Self {
            pin: rppal::pwm::Pwm::new(chan)?,
        })
    }

    fn set(&mut self, v: f32) -> Result<(), Error> {
        self.pin.set_duty_cycle(v as f64)?;
        Ok(())
    }
}

#[cfg(not(feature="gpio"))]
pub struct PwmPin;

#[cfg(not(feature="gpio"))]
impl PwmPin {
    pub fn new(channel: u8) -> Result<Self, Error> {
        warn!("Mocking PWM pin on channel {channel}");
        Ok(PwmPin {})
    }

    pub fn set(&mut self, _: f32) -> Result<(), Error> { Ok(()) }
}
