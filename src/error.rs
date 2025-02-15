use std::sync::mpsc::TryRecvError;

#[derive(Debug)]
pub enum Error {
    TryRecvError(TryRecvError),
    IOError(std::io::Error),
    NotifyError(notify::Error),

    #[cfg(feature="gpio")]
    GPIOError(rppal::gpio::Error),

    #[cfg(feature="gpio")]
    PWMError(rppal::pwm::Error),
}

impl From<TryRecvError> for Error {
    fn from(err: TryRecvError) -> Self {
        Error::TryRecvError(err)
    }
}

#[cfg(feature="gpio")]
impl From<rppal::gpio::Error> for Error {
    fn from(err: rppal::gpio::Error) -> Self {
        Error::GPIOError(err)
    }
}

#[cfg(feature="gpio")]
impl From<rppal::pwm::Error> for Error {
    fn from(err: rppal::pwm::Error) -> Self {
        Error::PWMError(err)
    }
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Error::IOError(err)
    }
}

impl From<notify::Error> for Error {
    fn from(err: notify::Error) -> Self {
        Error::NotifyError(err)
    }
}

impl Into<String> for Error {
    fn into(self) -> String {
        match self {
            Error::TryRecvError(e) => format!("IPC error: {e}"),
            Error::IOError(e) => format!("PWM error: {e}"),
            Error::NotifyError(e) => format!("Notify error: {e}"),

            #[cfg(feature="gpio")]
            Error::GPIOError(e) => format!("GPIO error: {e}"),
            #[cfg(feature="gpio")]
            Error::PWMError(e) => format!("PWM error: {e}"),
        }
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(formatter, "{}", self.to_string())
    }
}

impl std::error::Error for Error {}
