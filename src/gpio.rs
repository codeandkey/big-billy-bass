use crate::error::Error;

pub trait LogicPin: Send {
    fn set(&mut self, high: bool) -> Result<(), Error>;

    fn set_high(&mut self) -> Result<(), Error> {
        self.set(true)
    }

    fn set_low(&mut self) -> Result<(), Error> {
        self.set(false)
    }
}

pub trait PwmPin: Send {
    fn set(&mut self, v: f32) -> Result<(), Error>;
}

pub struct MockLogicPin {
    v: bool,
}
pub struct MockPwmPin {
    v: f32,
}

impl MockLogicPin {
    pub fn new(pn: u16) -> Self {
        warn!("Mocking logic pin on PN {pn}");
        MockLogicPin { v: false }
    }

    #[cfg(test)]
    pub fn get(&self) -> bool {
        self.v
    }
}

impl LogicPin for MockLogicPin {
    fn set(&mut self, v: bool) -> Result<(), Error> {
        self.v = v;
        Ok(())
    }
}

impl MockPwmPin {
    pub fn new(pn: u16) -> Self {
        warn!("Mocking PWM pin on PN {pn}");
        MockPwmPin { v: 0.0 }
    }

    #[cfg(test)]
    pub fn get(&self) -> f32 {
        self.v
    }
}

impl PwmPin for MockPwmPin {
    fn set(&mut self, v: f32) -> Result<(), Error> {
        self.v = v;
        Ok(())
    }
}
