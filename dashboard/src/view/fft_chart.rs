use std::sync::{Arc, Mutex};

use common::param::*;
use ratatui::{
    Frame,
    layout::{Constraint, Rect},
    style::{Style, Stylize},
    symbols,
    widgets::{Axis, Block, Chart, Dataset, GraphType},
};

use crate::model::Model;

fn compute_frequency_bins(sample_rate: f32, fft_size: usize) -> Vec<f32> {
    (0..fft_size)
        .map(|k| k as f32 * sample_rate / fft_size as f32)
        .collect()
}

pub fn render(
    frame: &mut Frame,
    rect: Rect,
    model: &Arc<Mutex<Model>>,
    pc: &mut ParameterController,
) {
    let fft_logmin = (20f64).log(10.0);
    let fft_logmax = (20000f64).log(10.0);
    let fft_n_xticks = 4;

    let fft_xticks: Vec<f64> = (0..fft_n_xticks + 1)
        .map(|i| fft_logmin + i as f64 * (fft_logmax - fft_logmin) / (fft_n_xticks as f64))
        .collect();

    let s_rate = pc.get::<u32>(&PARAM_SAMPLE_RATE);
    let fft_yraw = dynamic_moving_average(&model.lock().unwrap().fft_data().clone(), 1000);
    let fft_xraw = compute_frequency_bins(s_rate as f32, fft_yraw.len());
    let fft_data: Vec<(f64, f64)> = fft_xraw
        .iter()
        .zip(fft_yraw)
        .map(|(a, b)| ((a + 1.0).log(10.0) as f64, b as f64))
        .collect();

    let lpf_cutoff = pc.get::<f64>(&PARAM_LPF_CUTOFF);
    let hpf_cutoff = pc.get::<f64>(&PARAM_HPF_CUTOFF);
    let lpf_points = [(lpf_cutoff.log(10.0), -50.0), (lpf_cutoff.log(10.0), 0.0)];
    let hpf_points = [(hpf_cutoff.log(10.0), -50.0), (hpf_cutoff.log(10.0), 0.0)];

    let fft_datasets = vec![
        Dataset::default()
            .name("SIG")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().green())
            .data(&fft_data[..]),
        Dataset::default()
            .name("LPFC")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().red())
            .data(&lpf_points),
        Dataset::default()
            .name("HPFC")
            .marker(symbols::Marker::Braille)
            .graph_type(GraphType::Line)
            .style(Style::default().cyan())
            .data(&hpf_points),
    ];

    // Create the X axis and define its properties
    let fft_x_axis = Axis::default()
        .title("HZ".red())
        .style(Style::default().white())
        .bounds([*fft_xticks.first().unwrap(), *fft_xticks.last().unwrap()])
        .labels(
            fft_xticks
                .iter()
                .map(|x| ((10.0_f64.powf(*x) + 1.0) as u32).to_string()),
        );

    // Create the Y axis and define its properties
    let fft_y_axis = Axis::default()
        .title("DB".red())
        .style(Style::default().white())
        .bounds([-50.0, 10.0])
        .labels(["-50", "-40", "-30", "-20", "-10", "0", "10"]);

    let constraints = (Constraint::Min(0), Constraint::Ratio(1, 4));

    // Create the chart and link all the parts together
    let fft_chart = Chart::new(fft_datasets)
        .block(Block::new().title("FFT"))
        .x_axis(fft_x_axis)
        .y_axis(fft_y_axis)
        .hidden_legend_constraints(constraints)
        .legend_position(Some(ratatui::widgets::LegendPosition::TopRight));

    frame.render_widget(fft_chart, rect);
}

fn dynamic_moving_average(vec: &Vec<f32>, k: usize) -> Vec<f32> {
    let total_size = vec.len();

    (0..total_size) // Iterate over indices (1-based)
        .map(|n| {
            let window_size = (n * n / vec.len() / vec.len() * k).max(1).min(n); // Compute dynamic window size

            let end_index = n;
            let start_index = end_index - window_size;
            let sum = vec[start_index..end_index].iter().sum::<f32>();
            Some(sum / (end_index - start_index) as f32) // Compute average
        })
        .take_while(|x| x.is_some()) // Stop iteration when window size is too large
        .flatten() // Remove None values
        .collect()
}
