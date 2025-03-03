#[macro_use]
extern crate log;

mod model;

use color_eyre::Result;
use common::dash::DashMessage;
use common::param::*;
use crossterm::event::{self, Event, KeyCode, KeyEvent};
use model::{Limb, Model};
use ratatui::{DefaultTerminal, Frame};
use std::os::unix::thread;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::{
    error::Error,
    sync::atomic::{AtomicBool, Ordering},
    time::Duration,
};

use ratatui::{
    style::{Style, Stylize},
    symbols,
    widgets::{Axis, Block, Chart, Dataset, GraphType},
};

const FRAMERATE: u64 = 30;

fn render(frame: &mut Frame, model: &Arc<Mutex<Model>>, pc: &mut ParameterController) {
    let (body, bmin, bmax) = model.lock().unwrap().dataset(model::Dataset::Rms, Limb::Body);
    let (mouth, mmin, mmax) = model.lock().unwrap().dataset(model::Dataset::Rms, Limb::Mouth);

    let bthresh = pc.get::<u32>(&PARAM_BODY_THRESHOLD);
    let mthresh = pc.get::<u32>(&PARAM_MOUTH_THRESHOLD);

    let min = bmin.min(mmin);
    let max = bmax.max(mmax);
    let mid = min + (max - min) / 2.0;

    let mthresh_ln = [(min, mthresh as f64), (max, mthresh as f64)];
    let bthresh_ln = [(min, bthresh as f64), (max, bthresh as f64)];

    // Create the datasets to fill the chart with
    let datasets = vec![
        Dataset::default()
            .name("BRMS")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().red())
            .data(&body[..]),
        Dataset::default()
            .name("MRMS")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().blue())
            .data(&mouth[..]),
        Dataset::default()
            .name("BTHR")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().cyan())
            .data(&bthresh_ln),
        Dataset::default()
            .name("MTHR")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().magenta())
            .data(&mthresh_ln)
    ];

    // Create the X axis and define its properties
    let x_axis = Axis::default()
        .title("Elapsed (ms)".red())
        .style(Style::default().white())
        .bounds([min, max])
        .labels([min.to_string(), max.to_string(), mid.to_string()]);

    // Create the Y axis and define its properties
    let y_axis = Axis::default()
        .title("RMS".red())
        .style(Style::default().white())
        .bounds([0.0, 16384.0])
        .labels(["0", "8192", "16384"]);

    // Create the chart and link all the parts together
    let chart = Chart::new(datasets)
        .block(Block::new().title("RMS"))
        .x_axis(x_axis)
        .y_axis(y_axis);

    frame.render_widget(chart, frame.area());
}

fn main() -> Result<(), Box<dyn Error>> {
    let mut term = ratatui::init();
    let thr_stop = AtomicBool::new(false);
    let mut pc = ParameterController::new(&Path::new(PARAM_ROOT))?;

    let mut rx = common::dash::DashMessageReceiver::new()?;
    let model = Arc::new(Mutex::new(Model::new()));
    let thr_model = model.clone();

    let thr = std::thread::spawn(move || {
        loop {
            match rx.recv() {
                Ok(m) => match m {
                    DashMessage::LimbHistory(frame) => {
                        thr_model.lock().unwrap().submit_dataset(model::Dataset::PinOut, frame)
                    },
                    DashMessage::RmsHistory(frame) => {
                        thr_model.lock().unwrap().submit_dataset(model::Dataset::Rms, frame);
                    },
                    _ => (),
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
    thr_stop.store(true, Ordering::Relaxed);
    thr.join().unwrap();

    info!("Bye!");
    Ok(())
}
