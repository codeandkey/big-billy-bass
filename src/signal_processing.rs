mod audio_node;
mod biquad_filter;

use crate::{gpio::GpioThreadHandle, param::*, types::Sample};
use audio_node::{PaSource, ReadResult};
use biquad_filter::BiquadFilter;

const FILTER_Q: f32 = 0.707;

pub struct AudioNode {
    _hpf: BiquadFilter,
    _lpf: BiquadFilter,
    // second stage lpfs
    _lpf2lpf: BiquadFilter,
    _lpf2hpf: BiquadFilter,
    // pulse
    _source: PaSource,

    // gpio
    _gpio_handle: GpioThreadHandle,

    // config
    _pc: ParameterController,
}

impl Drop for AudioNode {
    fn drop(&mut self) {
        self._gpio_handle.join();
    }
}

impl AudioNode {
    pub fn new(pc: ParameterController, app_name: &str, handle: GpioThreadHandle) -> Self {
        Self {
            _hpf: BiquadFilter::new_hpf(FILTER_Q, pc.get(PARAM_HPF_CUTOFF), 44100),
            _lpf: BiquadFilter::new_lpf(FILTER_Q, pc.get(PARAM_LPF_CUTOFF), 44100),

            // second stage lpfs
            _lpf2lpf: BiquadFilter::new_lpf(FILTER_Q, pc.get(PARAM_LPF_CUTOFF), 44100),
            _lpf2hpf: BiquadFilter::new_lpf(FILTER_Q, pc.get(PARAM_LPF_CUTOFF), 44100),

            _source: PaSource::new(app_name).unwrap(),

            _gpio_handle: handle,

            _pc: pc,
        }
    }

    pub fn update(&mut self) -> Result<u64, &'static str> {
        // update filters
        self._hpf.set_cutoff(self._pc.get(PARAM_HPF_CUTOFF));
        self._lpf.set_cutoff(self._pc.get(PARAM_LPF_CUTOFF));

        self._lpf2hpf
            .set_cutoff(self._pc.get(PARAM_SECOND_STAGE_CUTOFF));
        self._lpf2lpf
            .set_cutoff(self._pc.get(PARAM_SECOND_STAGE_CUTOFF));

        // read data
        let buff = match self._source.read()? {
            ReadResult::NotReady => return Ok(1000),
            ReadResult::Data(dat) => dat,
        };

        if buff.len() == 0 {
            return Ok(5);
        }

        // apply filtering
        // let mut pcm_dat: Vec<Sample> = Vec::with_capacity(buff.len() / 2);
        let mut hpf: Vec<Sample> = Vec::with_capacity(buff.len() / 4);
        let mut lpf: Vec<Sample> = Vec::with_capacity(buff.len() / 4);

        for chunk in buff.chunks_exact(4) {
            let s_1 = i16::from_le_bytes([chunk[0], chunk[1]]) as f32;
            let s_2 = i16::from_le_bytes([chunk[2], chunk[3]]) as f32;
            let s = (s_1 + s_2) / 2.0;

            // pcm_dat.push(s_1 as Sample);
            // pcm_dat.push(s_2 as Sample);
            hpf.push(self._lpf2hpf.update(self._hpf.update(s).abs()) as Sample);
            lpf.push(self._lpf2lpf.update(self._lpf.update(s).abs()) as Sample);
        }

        // send to gpio
        let sleep_time_us: u64 = lpf.len() as u64 * 1_000_000 / 44100;

        self._gpio_handle.send_frame(lpf, hpf);
        // frames * (uS/S) / (frames / S)

        Ok(sleep_time_us)
    }
}
