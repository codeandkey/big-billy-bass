/// This file contains a list of all monitored parameters.
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
    str::FromStr,
    sync::{Arc, Mutex, RwLock},
};

use notify::{Event, EventKind, RecommendedWatcher, RecursiveMode, Watcher};
use std::error::Error;

#[derive(Clone, Copy)]
pub struct Parameter(pub &'static str, pub &'static str,pub f32, pub f32,pub f32); // name, def, min, max, incr

// Width of the RMS window in seconds
pub const PARAM_RMS_WINDOW_SIZE_MS: &Parameter =
    &Parameter("WindowSizeMs", "100", 5.0, 200.0, 25.0);

// Milliseconds between attempted head/tail movement swaps
pub const PARAM_FLIP_INTERVAL: &Parameter =
    &Parameter("FlipInterval", "1000", 100.0, 100.0, 2000.0);

// latency fudge factor
pub const PARAM_AUDIO_LATENCY: &Parameter = &Parameter("AudioLatencyMs", "250", 0.0, 1000.0, 50.0);

// RMS thresholds at which to actuate the motors
pub const PARAM_BODY_THRESHOLD: &Parameter =
    &Parameter("BodyThreshold", "5000", 500.0, 20000.0, 100.0);
pub const PARAM_MOUTH_THRESHOLD: &Parameter =
    &Parameter("MouthThreshold", "5000", 500.0, 20000.0, 100.0);

// PWM work factors for motor speed (0-1)
pub const PARAM_BODY_SPEED: &Parameter = &Parameter("BodySpeed", "1.0", 0.0, 1.0, 0.05);
pub const PARAM_MOUTH_SPEED: &Parameter = &Parameter("MouthSpeed", "1.0", 0.0, 1.0, 0.05);

// LPF, HPF cutoff settings
pub const PARAM_HPF_CUTOFF: &Parameter = &Parameter("HpfCutoff", "5000.0", 20.0, 20000.0, 100.0);
pub const PARAM_LPF_CUTOFF: &Parameter = &Parameter("LpfCutoff", "1000.0", 20.0, 20000.0, 100.0);

// Time in seconds to hold the mouth open or closed before releasing the motor
pub const PARAM_MOUTH_FWD_HOLD: &Parameter = &Parameter("MouthFwdHold", "0.1", 0.0, 0.3, 0.05);
pub const PARAM_MOUTH_BWD_HOLD: &Parameter = &Parameter("MouthBwdHold", "0.1", 0.0, 0.3, 0.05);

// Maximum GPIO / dash processing rate
pub const PARAM_GPIO_RATE: &Parameter = &Parameter("GpioRate", "250", 100.0, 500.0, 25.0);

pub const ALL_PARAMS: &[&Parameter] = &[
    PARAM_BODY_SPEED,
    PARAM_BODY_THRESHOLD,
    PARAM_FLIP_INTERVAL,
    PARAM_GPIO_RATE,
    PARAM_HPF_CUTOFF,
    PARAM_LPF_CUTOFF,
    PARAM_MOUTH_BWD_HOLD,
    PARAM_MOUTH_FWD_HOLD,
    PARAM_MOUTH_SPEED,
    PARAM_MOUTH_THRESHOLD,
    PARAM_RMS_WINDOW_SIZE_MS,
    PARAM_AUDIO_LATENCY,
];

pub struct ParameterController {
    cache: Arc<RwLock<HashMap<String, String>>>,
    _active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
    _root_watcher: RecommendedWatcher,
}

impl ParameterController {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        init_param_dir()?;

        let root = param_dir()?;

        let cache = Arc::new(RwLock::new(HashMap::new()));
        let active_watcher = Arc::new(Mutex::new(None));

        let w_cache = cache.clone();
        let w_active_watcher = active_watcher.clone();
        let w_link = root.join("current");

        ParameterController::trigger_track_update(active_watcher.clone(), cache.clone(), &w_link);

        let mut root_watcher =
            notify::recommended_watcher(move |r: Result<Event, notify::Error>| {
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

        root_watcher.watch(&root, RecursiveMode::NonRecursive)?;

        Ok(Self {
            cache,
            _active_watcher: active_watcher,
            _root_watcher: root_watcher,
        })
    }

    fn init_defaults(link: &Path) -> Result<(), Box<dyn Error>> {
        if link.exists() {
            return Ok(());
        }

        let defparams = link.parent().unwrap().join("default");

        debug!("Initializing default link");
        std::os::unix::fs::symlink(&defparams, link)?;

        if !defparams.is_dir() {
            debug!("Writing {} params to default store", ALL_PARAMS.len());
            std::fs::create_dir_all(&defparams)?;

            for Parameter(pn, pv, _, _, _) in ALL_PARAMS {
                std::fs::write(defparams.join(pn), pv)?;
            }
        }

        Ok(())
    }

    fn trigger_track_update(
        active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
        cache: Arc<RwLock<HashMap<String, String>>>,
        link: &Path,
    ) {
        if let Err(e) = ParameterController::init_defaults(link) {
            warn!("Failed initializing default store: {e}");
        }

        let mut track_root = std::fs::read_link(link).unwrap();

        if !track_root.starts_with(&PathBuf::from("/")) {
            track_root = link.parent().unwrap().join(track_root);
        }

        if let Err(e) =
            ParameterController::update_track(active_watcher.clone(), cache.clone(), &track_root)
        {
            warn!("Failed to update track: {:?}", e);
        }
    }

    fn update_track(
        active_watcher: Arc<Mutex<Option<RecommendedWatcher>>>,
        cache: Arc<RwLock<HashMap<String, String>>>,
        track_root: &Path,
    ) -> Result<(), Box<dyn Error>> {
        cache.write().unwrap().clear();

        if let Ok(dir) = std::fs::read_dir(track_root) {
            for entry in dir {
                if let Ok(entry) = entry {
                    let key = entry.file_name().into_string().unwrap();
                    let value = std::fs::read_to_string(entry.path())
                        .unwrap()
                        .trim()
                        .to_string();

                    debug!("Precached {} = \"{}\"", key, value);
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
                let fname = e
                    .paths
                    .iter()
                    .next()
                    .unwrap()
                    .file_name()
                    .unwrap()
                    .to_str()
                    .unwrap();
                match e.kind {
                    EventKind::Create(_) | EventKind::Modify(_) => 'skip: {
                        let value = std::fs::read_to_string(e.paths.iter().next().unwrap())
                            .unwrap()
                            .trim()
                            .to_string();

                        if value.is_empty() {
                            break 'skip;
                        }

                        let mut located = false;

                        for p in ALL_PARAMS {
                            if p.0 == fname {
                                located = true;
                                break;
                            }
                        }

                        if !located {
                            warn!(
                                "Detected update to unrecognized parameter: {fname}, caching anyway"
                            );
                        }

                        debug!("Updated \"{fname}\" = \"{value}\"");
                        w_cache.write().unwrap().insert(fname.to_string(), value);
                    }
                    EventKind::Remove(_) => {
                        debug!("Removed \"{fname}\"");
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

    pub fn get<T>(&self, Parameter(k, def, _, _, _): &Parameter) -> T
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
            match v.trim().parse() {
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

pub fn init_param_dir() -> Result<(), Box<dyn Error>> {
    let root = param_dir()?;
    let link = root.join("current");
    let defparams = root.join("default");

    if !link.exists() {
        std::os::unix::fs::symlink(root.join("default"), &link)?;
    }

    if !defparams.is_dir() {
        std::fs::create_dir_all(&defparams)?;

        for Parameter(pn, pv, _, _, _) in ALL_PARAMS {
            std::fs::write(defparams.join(pn), pv)?;
        }

        debug!("Wrote {} default parameters", ALL_PARAMS.len());
    }

    Ok(())
}

pub fn param_dir() -> Result<PathBuf, Box<dyn Error>> {
    let root = dirs::config_dir().unwrap().join("billy");

    if !root.is_dir() {
        std::fs::create_dir_all(&root)?;
    }

    Ok(root)
}

pub fn set_track(track: &str) -> Result<(), Box<dyn Error>> {
    // TODO: Move this behavior to the dbus watcher
    let root = param_dir()?;

    std::fs::remove_file(root.join("current")).ok();
    std::os::unix::fs::symlink(root.join(track), root.join("current"))?;

    Ok(())
}

pub fn write_param(param: &Parameter, value: String) -> Result<(), Box<dyn Error>> {
    let root = param_dir()?;
    std::fs::write(root.join("current").join(param.0), value)?;

    Ok(())
}
