#[macro_use]
extern crate log;

mod model;
mod view;

use color_eyre::Result;
use common::bus::BusReceiver;
use common::DashMessage;
use common::param::*;
use common::DASH_PORT;
use crossterm::event::{Event, KeyCode, KeyEvent};
use model::Model;
use ratatui::layout::Constraint;
use ratatui::layout::Direction;
use ratatui::layout::Layout;
use ratatui::Frame;
use std::sync::{Arc, Mutex};
use std::{
    error::Error,
    sync::atomic::{AtomicBool, Ordering},
    time::Duration,
};

const FRAMERATE: u64 = 30;

fn render(frame: &mut Frame, model: &Arc<Mutex<Model>>, pc: &mut ParameterController) {
    let root_layout = Layout::new(Direction::Vertical, [
        Constraint::Ratio(1, 2),
        Constraint::Ratio(1, 2)
    ]).split(frame.area());

    let btm_layout = Layout::new(Direction::Horizontal, [
        Constraint::Max(25),
        Constraint::Fill(1)
    ]).split(root_layout[1]);

    view::limb_chart::render(frame, root_layout[0], model, pc);
    view::paramctl::render(frame, btm_layout[0], pc);
    view::fft_chart::render(frame, btm_layout[1], model, pc);
}

fn main() -> Result<(), Box<dyn Error>> {
    let mut term = ratatui::init();
    let stop = Arc::new(AtomicBool::new(false));
    let thr_stop = stop.clone();
    let mut pc = ParameterController::new()?;

    let mut rx = BusReceiver::<DashMessage>::new(DASH_PORT)?;
    let model = Arc::new(Mutex::new(Model::new()));
    let thr_model = model.clone();

    let thr = std::thread::spawn(move || {
        loop {
            if thr_stop.load(Ordering::Relaxed) {
                break;
            }

            if let Err(e) = rx.timeout(100) {
                warn!("Failed setting receiver timeout: {e}");
            }

            match rx.recv() {
                Ok(m) => match m {
                    Some(DashMessage::LimbHistory(frame)) => {
                        thr_model.lock().unwrap().submit_limb_data(model::LimbDataset::PinOut, frame)
                    },
                    Some(DashMessage::RmsHistory(frame)) => {
                        thr_model.lock().unwrap().submit_limb_data(model::LimbDataset::Rms, frame);
                    },
                    Some(DashMessage::FFTData(frame)) => {
                        thr_model.lock().unwrap().submit_fft(frame);
                    }
                    None => (),
                },
                Err(e) => {
                    warn!("Failed receiving message: {e}");
                }
            }
        }
    });

    loop {
        term.draw(|t| render(t, &model, &mut pc))?;

        if !crossterm::event::poll(Duration::from_millis(1000 / FRAMERATE))? {
            continue;
        }

        match crossterm::event::read()? {
            Event::Key(KeyEvent {
                code: KeyCode::Char('q'),
                ..
            }) => break,
            _ => (),
        }
    }

    ratatui::restore();

    info!("Waiting for network thread.");
    stop.store(true, Ordering::Relaxed);
    thr.join().unwrap();

    info!("Bye!");
    Ok(())
}
