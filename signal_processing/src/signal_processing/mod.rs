mod audio_node;
mod biquad_filter;
mod moving_rms;

use std::path::Path;

use audio_node::{PaSource, ReadResult};
use biquad_filter::BiquadFilter;
use common::param::*;
use common::*;
use moving_rms::MovingRms;

const FILTER_Q: f32 = 0.707;

pub struct AudioNode {
    _hpf: BiquadFilter,
    _lpf: BiquadFilter,
    // second stage lpfs
    _lpf_rms: MovingRms,
    _hpf_rms: MovingRms,
    // pulse
    _source: PaSource,

    // gpio
    _gpio_handle: GpioMessageSender,

    // config
    _pc: ParameterController,
}

impl AudioNode {
    pub fn new(app_name: &str) -> Self {
        let pc = ParameterController::new(&Path::new(PARAM_ROOT)).unwrap();
        Self {
            _hpf: BiquadFilter::new_hpf(FILTER_Q, pc.get(PARAM_HPF_CUTOFF), 44100),
            _lpf: BiquadFilter::new_lpf(FILTER_Q, pc.get(PARAM_LPF_CUTOFF), 44100),

            // moving RMS
            _lpf_rms: MovingRms::new(),
            _hpf_rms: MovingRms::new(),

            _source: PaSource::new(app_name).unwrap(),

            _gpio_handle: GpioMessageSender::new().unwrap(),

            _pc: pc,
        }
    }

    pub fn update(&mut self) -> Result<u64, &'static str> {
        // update filters
        self._hpf.set_cutoff(self._pc.get(PARAM_HPF_CUTOFF));
        self._lpf.set_cutoff(self._pc.get(PARAM_LPF_CUTOFF));
        let rms_window_ms = self._pc.get::<f32>(PARAM_RMS_WINDOW_SIZE_MS);
        let window_size = (44100.0 * rms_window_ms / 1000.0) as usize;
        self._hpf_rms.set_size(window_size);
        self._lpf_rms.set_size(window_size);

        // read data
        let buff = match self._source.read()? {
            ReadResult::NotReady => return Ok(0),
            ReadResult::Data(dat) => dat,
        };

        if buff.len() == 0 {
            return Ok(0);
        }

        // apply filtering
        let mut hpf: Vec<Sample> = Vec::with_capacity(buff.len() / 4);
        let mut lpf: Vec<Sample> = Vec::with_capacity(buff.len() / 4);

        for chunk in buff.chunks_exact(4) {
            let s_1 = i16::from_le_bytes([chunk[0], chunk[1]]) as f32;
            let s_2 = i16::from_le_bytes([chunk[2], chunk[3]]) as f32;
            let s = (s_1 + s_2) / 2.0;

            hpf.push(self._hpf_rms.update(self._hpf.update(s)) as Sample);
            lpf.push(self._lpf_rms.update(self._lpf.update(s)) as Sample);
        }

        // frames * (uS/S) / (frames / S)
        let sleep_time_us: u64 = lpf.len() as u64 * 1_000_000 / 44100;

        // send to gpio
        self._gpio_handle
            .send(GpioMessage::NextFrame(lpf, hpf))
            .unwrap();

        self._source.drop()?;
        Ok(sleep_time_us)
    }
}
