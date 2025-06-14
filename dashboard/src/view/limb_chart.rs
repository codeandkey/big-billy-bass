use std::sync::{Arc, Mutex};

// use color_eyre::owo_colors::OwoColorize;
use common::param::*;
use ratatui::{
    Frame,
    layout::Rect,
    style::{Style, Stylize},
    symbols,
    widgets::{Axis, Block, Chart, Dataset, GraphType},
};

use crate::model::{self, Limb, Model};

pub fn render(
    frame: &mut Frame,
    rect: Rect,
    model: &Arc<Mutex<Model>>,
    pc: &mut ParameterController,
) {
    let (body, bmin, bmax, by, mut bthresh) = model
        .lock()
        .unwrap()
        .dataset(model::LimbDataset::Rms, Limb::Body);
    let (mouth, mmin, mmax, my, mut mthresh) = model
        .lock()
        .unwrap()
        .dataset(model::LimbDataset::Rms, Limb::Mouth);

    if !pc.get::<bool>(&PARAM_AUTO_MODE) {
        bthresh = pc.get(&PARAM_BODY_THRESHOLD);
        mthresh = pc.get(&PARAM_MOUTH_THRESHOLD);
    }

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
            .style(Style::new().light_red())
            .data(&body[..]),
        Dataset::default()
            .name("MRMS")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::new().light_cyan())
            .data(&mouth[..]),
        Dataset::default()
            .name("BTHR")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().red())
            .data(&bthresh_ln),
        Dataset::default()
            .name("MTHR")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().cyan())
            .data(&mthresh_ln),
    ];

    // Create the X axis and define its properties
    let x_axis = Axis::default()
        .title("MS".red())
        .style(Style::default().white())
        .bounds([min, max])
        .labels([min.to_string(), mid.to_string(), max.to_string()]);

    let n_ticks = 8;
    let ymax = my.max(by) as i16;
    let ticks = (0..(n_ticks + 1))
        .map(|i| i * (ymax / n_ticks))
        .map(|x| x.to_string());

    // Create the Y axis and define its properties
    let y_axis = Axis::default()
        .title("RMS".red())
        .style(Style::default().white())
        .bounds([0.0, ymax as f64])
        .labels(ticks);

    // Create the chart and link all the parts together
    let chart = Chart::new(datasets)
        .block(Block::new().title("RMS"))
        .x_axis(x_axis)
        .y_axis(y_axis);

    frame.render_widget(chart, rect);
}
