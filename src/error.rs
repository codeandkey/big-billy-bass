use std::sync::mpsc::TryRecvError;

#[derive(Debug)]
pub enum Error {
    SQLError(rusqlite::Error),
    TryRecvError(TryRecvError),
}

impl From<rusqlite::Error> for Error {
    fn from(err: rusqlite::Error) -> Self {
        Error::SQLError(err)
    }
}

impl From<TryRecvError> for Error {
    fn from(err: TryRecvError) -> Self {
        Error::TryRecvError(err)
    }
}

impl Into<String> for Error {
    fn into(self) -> String {
        match self {
            Error::SQLError(e) => format!("SQL error: {e}"),
            Error::TryRecvError(e) => format!("IPC error: {e}"),
        }
    }
}

impl std::fmt::Display for Error {
    fn fmt(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(formatter, "{}", self.to_string())
    }
}

impl std::error::Error for Error {}
