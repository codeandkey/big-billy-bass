#[derive(Command)]
pub struct Args {
    #[arg(short, long)]
    confdir: Option<PathBuf>,
}

impl Args {
    pub fn confdir(&self) -> PathBuf {
        let conf = match self.confdir {
            Some(ref p) => p.clone(),
            None => {
                match dirs::config_dir() {
                    Some(p) => p.join("billy"),
                    None => {
                        PathBuf::from("/tmp/billy")
                    }
                }
            }
        };

        if !conf.is_dir() {
            std::fs::create_dir_all(&conf).expect("Failed to create configuration directory");
        }

        debug!("Using configuration root: {:?}", conf);
    }
}