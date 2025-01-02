use std::fmt::Debug;
use std::time::Instant;

use rusqlite::types::FromSql;

#[cfg(test)]
use rusqlite::{Connection, OpenFlags};

#[cfg(test)]
use std::path::PathBuf;

#[cfg(test)]
use std::time::Duration;

use super::db::Database;
use super::error::Error;
use super::params::*;

/// Time in seconds until a cached value must be refreshed from the parameter
/// database.
const CACHE_AGE: f32 = 1.0;

pub struct Monitor<T> {
    pub age: Instant,
    pub param: Parameter<T>,
    pub value: Option<T>,
}

impl<T: FromSql + Debug + Clone + PartialEq> Monitor<T> {
    pub fn expired(&self) -> bool {
        self.age.elapsed().as_secs_f32() > CACHE_AGE || self.value.is_none()
    }

    pub fn new(param: Parameter<T>) -> Self {
        Monitor::<T> {
            age: Instant::now(),
            value: None,
            param,
        }
    }

    pub fn sync(&mut self, db: &Database) -> Result<T, Error> {
        if !self.expired() {
            return Ok(self.value.clone().unwrap());
        }

        let old_value = self.value.clone();
        let Parameter(key, def) = &self.param;

        self.value = match db.query_param::<T>(&key)? {
            Some(v) => Some(v),
            None => {
                trace!("Pulling default for {}: {:?}", key, def);
                Some(def.clone())
            }
        };

        if self.value != old_value {
            info!("Update {}: {:?} to {:?}", key, old_value, self.value);
        }

        self.age = Instant::now();
        Ok(self.value.clone().unwrap())
    }
}

/// Tests that the Monitor returns the default parameter value if the
/// property is not defined in the database.
#[test]
fn test_monitor_default_value() -> Result<(), Error> {
    let dbp = PathBuf::from(&format!("/tmp/bbbtest.db.1"));
    if dbp.is_file() {
        std::fs::remove_file(&dbp).unwrap();
    }

    let d = Database::connect(&dbp)?;
    let mut mon = Monitor::new(PARAM_RMS_WINDOW);
    let Parameter(_, def) = PARAM_RMS_WINDOW;

    assert_eq!(mon.sync(&d)?, def);
    Ok(())
}

/// Tests that the monitor correctly syncs to values after initially opening
/// the database.
#[test]
fn test_monitor_read_inital() -> Result<(), Error> {
    let dbp = PathBuf::from(&format!("/tmp/bbbtest.db.2"));
    if dbp.is_file() {
        std::fs::remove_file(&dbp).unwrap();
    }

    let mut d = Database::connect(&dbp)?;

    let mut ip = String::new();
    let mut fp = String::new();

    for _ in I_PARAMS {
        ip += ", 0";
    }

    for Parameter(name, _) in F_PARAMS {
        match *name {
            "RmsWindow" => {
                fp += ", 123.0";
            }
            _ => fp += ", 0.0",
        }
    }

    let conn = Connection::open_with_flags(&dbp, OpenFlags::SQLITE_OPEN_READ_WRITE)?;

    d.set_state_value("track", "Test");

    conn.execute(
        &format!("INSERT INTO parameters VALUES ('Test' {ip} {fp})"),
        rusqlite::params![],
    )?;

    let mut mon = Monitor::new(PARAM_RMS_WINDOW);
    std::thread::sleep(Duration::from_secs_f32(CACHE_AGE + 0.1));
    assert_eq!(mon.sync(&d)?, 123.0);

    let mut mon = Monitor::new(PARAM_RMS_WINDOW);
    std::thread::sleep(Duration::from_millis(1000));
    assert_eq!(mon.sync(&d)?, 123.0);

    Ok(())
}

/// Tests that the monitor can detect an update to the parameter database in
/// real-time.
#[test]
fn test_monitor_detect_update() -> Result<(), Error> {
    let dbp = PathBuf::from(&format!("/tmp/bbbtest.db.3"));
    if dbp.is_file() {
        std::fs::remove_file(&dbp).unwrap();
    }

    let mut d = Database::connect(&dbp)?;

    let mut ip = String::new();
    let mut fp = String::new();

    let mut mon = Monitor::new(PARAM_RMS_WINDOW);
    let Parameter(_, def) = PARAM_RMS_WINDOW;

    assert_eq!(mon.sync(&d)?, def);

    for _ in I_PARAMS {
        ip += ", 0";
    }

    for Parameter(name, _) in F_PARAMS {
        match *name {
            "RmsWindow" => {
                fp += ", 123.0";
            }
            _ => fp += ", 0.0",
        }
    }

    let conn = Connection::open_with_flags(&dbp, OpenFlags::SQLITE_OPEN_READ_WRITE)?;

    d.set_state_value("track", "Test");

    conn.execute(
        &format!("INSERT INTO parameters VALUES ('Test' {ip} {fp})"),
        rusqlite::params![],
    )?;

    let mut mon = Monitor::new(PARAM_RMS_WINDOW);
    std::thread::sleep(Duration::from_secs_f32(CACHE_AGE + 0.1));
    assert_eq!(mon.sync(&d)?, 123.0);

    Ok(())
}
