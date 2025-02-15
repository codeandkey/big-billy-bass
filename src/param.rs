/// This file contains a list of all monitored parameters.
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    str::FromStr,
    sync::{Arc, Mutex, RwLock},
};

use notify::{Event, EventKind, RecommendedWatcher, RecursiveMode, Watcher};

use crate::error::Error;

#[derive(Clone, Copy)]
pub struct Parameter(&'static str, &'static str);

// Width of the RMS window in seconds
pub const PARAM_SECOND_STAGE_CUTOFF: &Parameter = &Parameter("RmsWindow", "0.3");

// Milliseconds between attempted head/tail movement swaps
pub const PARAM_FLIP_INTERVAL: &Parameter = &Parameter("FlipInterval", "500");

// Effective sample rate of the current track
// John: this will probably need to have to come from the pulseaudio libary and not the database
pub const PARAM_SAMPLE_RATE: &Parameter = &Parameter("SampleRate", "44100");

// RMS thresholds at which to actuate the motors
pub const PARAM_BODY_THRESHOLD: &Parameter = &Parameter("BodyThreshold", "1000");
pub const PARAM_MOUTH_THRESHOLD: &Parameter = &Parameter("MouthThreshold", "1000");

// PWM work factors for motor speed (0-1)
pub const PARAM_BODY_SPEED: &Parameter = &Parameter("BodySpeed", "1.0");
pub const PARAM_MOUTH_SPEED: &Parameter = &Parameter("MouthSpeed", "1.0");

// LPF, HPF cutoff settings
pub const PARAM_HPF_CUTOFF: &Parameter = &Parameter("HpfCutoff", "0.0");
pub const PARAM_LPF_CUTOFF: &Parameter = &Parameter("LpfCutoff", "20000.0");

// Time in seconds to hold the mouth open or closed before releasing the motor
pub const PARAM_MOUTH_FWD_HOLD: &Parameter = &Parameter("MouthFwdHold", "0.1");
pub const PARAM_MOUTH_BWD_HOLD: &Parameter = &Parameter("MouthBwdHold", "0.1");

pub struct ParameterController {
    cache: Arc<RwLock<HashMap<String, String>>>,
    _active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
    _root_watcher: RecommendedWatcher,
}

impl ParameterController {
    pub fn new(root: &Path) -> Result<Self, Error> {
        if !root.is_dir() {
            std::fs::create_dir_all(root).expect("Failed to create parameter root");
        }

        let cache = Arc::new(RwLock::new(HashMap::new()));
        let active_watcher = Arc::new(Mutex::new(None));

        let w_cache = cache.clone();
        let w_active_watcher = active_watcher.clone();
        let w_link = root.join("current");

        let mut root_watcher = notify::recommended_watcher(move |r: Result<Event, notify::Error>| {
            if let Ok(e) = r {
                match e.kind {
                    EventKind::Create(_) => {
                        if e.paths.iter().next().unwrap() == &w_link {
                            ParameterController::trigger_track_update(
                                w_active_watcher.clone(),
                                w_cache.clone(),
                                &w_link,
                            );
                        }
                    }
                    _ => (),
                }
            }
        })?;

        root_watcher.watch(root, RecursiveMode::NonRecursive)?;

        Ok(Self {
            cache,
            _active_watcher: active_watcher,
            _root_watcher: root_watcher,
        })
    }

    fn trigger_track_update(
        active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
        cache: Arc<RwLock<HashMap<String, String>>>,
        link: &Path,
    ) {
        let mut track_root = std::fs::read_link(link).unwrap();

        if !track_root.starts_with(&PathBuf::from("/")) {
            track_root = link.parent().unwrap().join(track_root);
        }

        if let Err(e) = ParameterController::update_track(
            active_watcher.clone(),
            cache.clone(),
            &track_root,
        ) {
            warn!("Failed to update track: {:?}", e);
        }
    }

    fn update_track(
        active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
        cache: Arc<RwLock<HashMap<String, String>>>,
        track_root: &Path,
    ) -> Result<(), Error> {
        cache.write().unwrap().clear();

        if let Ok(dir) = std::fs::read_dir(track_root) {
            for entry in dir {
                if let Ok(entry) = entry {
                    let key = entry.file_name().into_string().unwrap();
                    let value = std::fs::read_to_string(entry.path()).unwrap();

                    cache.write().unwrap().insert(key, value);
                }
            }
        } else {
            std::fs::create_dir_all(track_root)?;
            debug!("Created parameter directory {:?}", track_root);
        }

        let w_cache = cache.clone();

        let mut w = notify::recommended_watcher(move |r: Result<Event, notify::Error>| {
            if let Ok(e) = r {
                let fname = e.paths.iter().next().unwrap().file_name().unwrap().to_str().unwrap();
                match e.kind {
                    EventKind::Create(_) | EventKind::Modify(_) => {
                        let value = std::fs::read_to_string(e.paths.iter().next().unwrap())
                            .unwrap()
                            .trim()
                            .into();

                        debug!("Updated \"{fname}\" = \"{value}\"");
                        w_cache.write().unwrap().insert(fname.to_string(), value);
                    }
                    EventKind::Remove(_) => {
                        w_cache.write().unwrap().remove(fname);
                    }
                    _ => (),
                }
            }
        })?;

        w.watch(track_root, RecursiveMode::Recursive)?;
        active_watcher.lock().unwrap().replace(w);

        info!("Track changed to {:?}", track_root.file_name().unwrap());

        Ok(())
    }

    pub fn get<T>(&self, Parameter(k, def): &Parameter) -> T
    where
        T: FromStr,
        <T as FromStr>::Err: std::fmt::Debug,
    {
        let cached = self
            .cache
            .read()
            .expect("Parameter read lock acquisition failure")
            .get(*k)
            .cloned();

        if let Some(v) = cached {
            match v.parse() {
                Ok(r) => return r,
                Err(e) => {
                    warn!("{k} parsing \"{v}\" failed: {e:?}");
                }
            }
        }

        info!("Caching default: {k} = \"{def}\"");

        self.cache
            .write()
            .expect("Parameter write lock acquisition failure")
            .insert(k.to_string(), def.to_string());

        def.parse()
            .expect(&format!("Invalid default value {def} for {k}"))
    }
}
