#[macro_use]
extern crate log;

mod signal_processing;
use std::{cell::RefCell, error::Error, rc::Rc, sync::Arc};

use circular_buffer::CircularBuffer;
use common::{bus::*, param::*, *};
use rustfft::FftPlanner;
use signal_processing::{BiquadFilter, MovingRms, pa_node::PaNode};

const FILTER_Q: f32 = 0.707;
const MIN_FFT_SIZE: usize = 44100 / 20 * 4 / 3;



fn main() -> Result<(), Box<dyn Error>> {
    unsafe {
        let cur = std::env::var("RUST_LOG").unwrap_or_else(|_| "debug".to_string());
        std::env::set_var("RUST_LOG", cur);
    }

    pretty_env_logger::init();


    // set filters and whatnot

    // params
    let pc = ParameterController::new()?;

    // filters
    let mut hpf = BiquadFilter::new_hpf(FILTER_Q, pc.get(PARAM_HPF_CUTOFF), 44100);
    let mut lpf = BiquadFilter::new_lpf(FILTER_Q, pc.get(PARAM_LPF_CUTOFF), 44100);

    // moving RMS
    let mut lpf_rms = MovingRms::new();
    let mut hpf_rms = MovingRms::new();

    // ipc
    let gpio_handle: BusSender<GpioMessage> = BusSender::new(GPIO_PORT).unwrap();
    let dash_handle: BusSender<DashMessage> = BusSender::new(DASH_PORT).unwrap();

    // fft nonsense
    let mut planner: FftPlanner<f32> = FftPlanner::<f32>::new();
    let fft = Arc::new(planner.plan_fft_forward(MIN_FFT_SIZE));
    let fft_buff = Rc::new(RefCell::new(CircularBuffer::<MIN_FFT_SIZE, f32>::new()));
    
    // set up pulse audio
    let audio_node = PaNode::new_ref()?;

    // set read callback
    PaNode::set_read_callback(
        &audio_node,
        Rc::new(RefCell::new(move |pcm_data: Vec<i16>| {
            // update params
            lpf.set_cutoff(pc.get(PARAM_LPF_CUTOFF));
            hpf.set_cutoff(pc.get(PARAM_HPF_CUTOFF));
            let rms_window_ms = pc.get::<f32>(PARAM_RMS_WINDOW_SIZE_MS);
            let window_size = (44100.0 * rms_window_ms / 1000.0) as usize;
            hpf_rms.set_size(window_size);
            lpf_rms.set_size(window_size);

            // create buffers
            let mut hpf_buf: Vec<i16> = Vec::with_capacity(pcm_data.len() / 2);
            let mut lpf_buf: Vec<i16> = Vec::with_capacity(pcm_data.len() / 2);

            // apply filtering
            for sample in pcm_data.chunks(2) {
                let mono = sample[0] / 2 + sample[1] / 2;

                fft_buff.borrow_mut().push_back(mono as f32);
                lpf_buf.push(lpf_rms.update(lpf.update(mono as f32)) as i16);
                hpf_buf.push(hpf_rms.update(hpf.update(mono as f32)) as i16);
            }
            // send to gpio
            gpio_handle
                .send(GpioMessage::NextFrame(lpf_buf, hpf_buf))
                .unwrap();

            // send fft message to dash
            if fft_buff.borrow().len() == MIN_FFT_SIZE {
                let fft_buff_borrow = fft_buff.borrow();
                let (left, right) = fft_buff_borrow.as_slices();
                dash_handle
                    .send(DashMessage::FFTData(signal_processing::do_fft(
                        Arc::clone(&fft),
                        [left, right].concat(),
                    )))
                    .unwrap();
            }
        })),
    )?;


    // kick off mainloop and let it rip
    audio_node.borrow_mut().run_mainloop()?;

    Ok(())
}
