use rust_pigpio::*;
use std::error::Error;

#[derive(Debug)]
pub enum LogicLevel {
    HIGH,
    LOW,
}

#[cfg(feature = "gpio")]
pub struct LogicPin {
    pin: u32,
}

#[cfg(feature = "gpio")]
impl LogicPin {
    pub fn new(pn: u32) -> Result<Self, Box<dyn Error>> {
        set_mode(pn, OUTPUT)?;
        write(pn, ON)?;

        Ok(Self { pin: pn })
    }

    pub fn set(&mut self, level: LogicLevel) -> Result<(), Box<dyn Error>> {
        //trace!("logic {} {:?}", self.pin, level);
        match level {
            LogicLevel::HIGH => write(self.pin, ON)?,
            LogicLevel::LOW => write(self.pin, OFF)?,
        }

        Ok(())
    }
}

#[cfg(feature = "gpio")]
impl Drop for LogicPin {
    fn drop(&mut self) {
        self.set(LogicLevel::LOW).ok();
    }
}

#[cfg(not(feature = "gpio"))]
pub struct LogicPin {
    v: bool,
}

#[cfg(not(feature = "gpio"))]
impl LogicPin {
    pub fn new(pn: u32) -> Result<Self, Box<dyn Error>> {
        warn!("Mocking logic pin on PN {pn}");
        Ok(LogicPin { v: false })
    }

    pub fn set(&mut self, level: LogicLevel) -> Result<(), Box<dyn Error>> {
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

#[cfg(feature = "gpio")]
pub struct PwmPin {
    pin: u32,
}

#[cfg(feature = "gpio")]
impl PwmPin {
    pub fn new(pin: u32) -> Result<Self, Box<dyn Error>> {
        pwm::pwm(pin, 0)?;

        Ok(Self { pin })
    }

    pub fn set(&mut self, v: f32) -> Result<(), Box<dyn Error>> {
        let v = v.clamp(0.0, 1.0);
        //debug!("pwm {} {:?}", self.pin, (v * 255.0) as u32);
        pwm::pwm(self.pin, (v * 255.0) as u32)?;
        Ok(())
    }
}

#[cfg(feature = "gpio")]
impl Drop for PwmPin {
    fn drop(&mut self) {
        self.set(0.0).ok();
    }
}

#[cfg(not(feature = "gpio"))]
pub struct PwmPin;

#[cfg(not(feature = "gpio"))]
impl PwmPin {
    pub fn new(channel: u32) -> Result<Self, Box<dyn Error>> {
        warn!("Mocking PWM pin on channel {channel}");
        Ok(PwmPin {})
    }

    pub fn set(&mut self, _: f32) -> Result<(), Box<dyn Error>> {
        Ok(())
    }
}
