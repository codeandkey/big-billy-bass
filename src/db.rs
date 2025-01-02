use std::fmt::Debug;
use std::path::Path;

#[cfg(test)]
use std::path::PathBuf;

use rusqlite::{Connection, OpenFlags, params, types::FromSql};

use super::error::Error;
use super::params::*;

pub struct Database {
    conn: Connection,
}

impl Database {
    /// Initialize a new parameter connection. The database is created if it
    /// does not exist.
    pub fn connect(path: &Path) -> Result<Self, Error> {
        let flags = OpenFlags::SQLITE_OPEN_CREATE | OpenFlags::SQLITE_OPEN_READ_WRITE;
        let conn = Connection::open_with_flags(path, flags)?;

        info!("Connected to {:?}", path);

        let mut param_schema = "track STRING PRIMARY KEY\n".to_string();

        for Parameter(k, _) in I_PARAMS {
            param_schema += &format!(", {k} INTEGER\n");
            debug!("Including iparam {k}");
        }

        for Parameter(k, _) in F_PARAMS {
            param_schema += &format!(", {k} FLOAT\n");
            debug!("Including fparam {k}");
        }

        conn.execute(
            &format!("CREATE TABLE IF NOT EXISTS parameters(\n{param_schema})"),
            params![],
        )?;
        conn.execute(
            "CREATE TABLE IF NOT EXISTS state(key STRING PRIMARY KEY, value STRING)",
            params![],
        )?;
        info!("Initialized schema");

        Ok(Database { conn })
    }

    /// Read a named parameter fromm the db, coercing the database value to the
    /// expected type.
    ///
    /// Example:
    /// ```
    /// let volume = db.query_param("volume")?;
    ///
    /// if let Some(volume) = volume {
    ///    println!("Volume is {}", volume);
    /// } else {
    ///    println!("Volume is not set or track is not playing");
    /// }
    /// ```
    pub fn query_param<T>(&self, key: &str) -> Result<Option<T>, Error>
    where
        T: FromSql + Debug,
    {
        let Some(track) = self.get_track()? else {
            // No track is playing, so no parameters are set.
            return Ok(None);
        };

        let query = format!("SELECT {key} FROM parameters WHERE track='{track}'");
        debug!("Querying {track}/{key} : \"{query}\"");
        Ok(
            match self
                .conn
                .query_row::<T, _, _>(&query, params![], |row| row.get(0))
            {
                Ok(v) => Some(v),
                Err(rusqlite::Error::QueryReturnedNoRows) => None,
                Err(x) => Err(x)?,
            },
        )
    }

    /// Returns a string unique to the current playing track. The value lives
    /// in the 'state' table in the database and is used to associate parameters
    /// with the currently playing track.
    ///
    /// Example:
    /// ```
    /// let track = db.get_track()?;
    ///
    /// if let Some(track) = track {
    ///    println!("Track is {}", track);
    /// } else {
    ///    println!("No track is playing");
    /// }
    /// ```
    pub fn get_track(&self) -> Result<Option<String>, Error> {
        self.get_state_value("track")
    }

    pub fn get_state_value(&self, key: &str) -> Result<Option<String>, Error> {
        let query = "SELECT value FROM state WHERE key=?1";
        Ok(self
            .conn
            .query_row::<String, _, _>(query, params![key], |row| row.get(0))
            .ok())
    }

    pub fn set_state_value(&mut self, key: &str, value: &str) -> Result<Option<String>, Error> {
        let tx = self.conn.transaction()?;

        tx.execute("DELETE FROM state WHERE key=?1", params![key])?;
        tx.execute("INSERT INTO state VALUES (?1, ?2)", params![key, value])?;
        tx.commit()?;

        let query = "SELECT value FROM state WHERE key=?1";
        Ok(self
            .conn
            .query_row::<String, _, _>(query, params![key], |row| row.get(0))
            .ok())
    }
}

#[test]
fn test_db_state_management() -> Result<(), Error> {
    let dbp = PathBuf::from(&format!("/tmp/bbbtest_db.db.3"));
    if dbp.is_file() {
        std::fs::remove_file(&dbp).unwrap();
    }

    let mut d = Database::connect(&dbp)?;

    let test_keys = [
        ("key1", "value1"),
        ("key2", "value3"),
        ("key3", "value4"),
        ("key4", "value5"),
        ("key5", "value6"),
        ("key6", "value7"),
        ("key7", "value8"),
        ("key8", "value9"),
        ("key9", "value10"),
    ];

    for (k, v) in test_keys {
        d.set_state_value(k, v)?;
    }

    for (k, v) in test_keys {
        assert_eq!(d.get_state_value(k)?, Some(v.to_string()));
    }

    Ok(())
}
