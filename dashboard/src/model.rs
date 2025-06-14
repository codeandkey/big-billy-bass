use core::f32;

use circular_buffer::CircularBuffer;
use common::sp::*;
const HIST_SAMPLES: usize = 750;
const DEFAULT_Y_MAX: f32 = 7000.0;
pub struct Model {
    limb_hist: CircularBuffer<HIST_SAMPLES, (u128, f32, f32, f32, f32)>,
    rms_hist: CircularBuffer<HIST_SAMPLES, (u128, f32, f32, f32, f32)>,
    y_values: CircularBuffer<HIST_SAMPLES, f32>,
    fft_frames: Vec<Vec<f32>>,
    y_adsr: SustainDecay,
}

pub enum Limb {
    Body,
    Mouth,
}

pub enum LimbDataset {
    Rms,
    PinOut,
}

impl Model {
    pub fn new() -> Self {
        Self {
            limb_hist: CircularBuffer::new(),
            rms_hist: CircularBuffer::new(),
            fft_frames: Vec::new(),
            y_values: CircularBuffer::new(),
            y_adsr: SustainDecay::new_w_decay(DEFAULT_Y_MAX),
        }
    }

    pub fn submit_limb_data(&mut self, set: LimbDataset, data: Vec<(u128, f32, f32, f32, f32)>) {
        let dt = data.len() as f32 / 44100.0; // Assuming 44100Hz sample rate
        let buf = match set {
            LimbDataset::PinOut => &mut self.limb_hist,
            LimbDataset::Rms => {
                let y_vs: Vec<f32> = data
                    .iter()
                    .map(|(_, b, m, _, _)| self.y_adsr.update(b.max(*m), dt))
                    .collect();
                self.y_values.extend(y_vs);
                &mut self.rms_hist
            }
        };

        buf.extend(data);
    }

    pub fn submit_fft(&mut self, data: Vec<f32>) {
        self.fft_frames.push(data);
    }

    pub fn fft_data(&mut self) -> Vec<f32> {
        if self.fft_frames.is_empty() {
            return vec![];
        }

        let mut out = vec![0f32; self.fft_frames[0].len()];
        let n = self.fft_frames.len();

        for v in self.fft_frames.iter() {
            for i in 0..v.len() {
                out[i] += v[i] / (n as f32);
            }
        }

        self.fft_frames.clear();
        out
    }

    pub fn dataset(&self, set: LimbDataset, limb: Limb) -> (Vec<(f64, f64)>, f64, f64, f32, f32) {
        let buf = match set {
            LimbDataset::PinOut => &self.limb_hist,
            LimbDataset::Rms => &self.rms_hist,
        };

        let (left, right) = buf.as_slices();
        let data = [left, right].concat();

        if data.len() == 0 {
            return (vec![], 0.0, 1.0, DEFAULT_Y_MAX, 0.0);
        }

        let limb_data: Vec<(f64, f64)> = match limb {
            Limb::Body => data
                .iter()
                .map(|(t, b, _m, _, _)| (*t as f64, *b as f64))
                .collect(),
            Limb::Mouth => data
                .iter()
                .map(|(t, _b, m, _, _)| (*t as f64, *m as f64))
                .collect(),
        };
        let thresh = match limb {
            Limb::Body => data.iter().map(|(_, _, _, bt, _)| *bt).sum::<f32>() / data.len() as f32,
            Limb::Mouth => data.iter().map(|(_, _, _, _, mt)| *mt).sum::<f32>() / data.len() as f32,
        };

        let mintime = limb_data.first().unwrap().0;
        let maxtime = limb_data.last().unwrap().0;

        let ymax = f32::max(
            self.y_values.iter().fold(0.0, |acc, &v| acc.max(v)),
            DEFAULT_Y_MAX,
        );

        (limb_data, mintime, maxtime, ymax, thresh)
    }
}
