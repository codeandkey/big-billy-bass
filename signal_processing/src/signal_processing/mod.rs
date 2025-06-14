pub mod pa_node;

use circular_buffer::CircularBuffer;
use rustfft::num_complex::Complex;
use std::{f32::consts::PI, sync::Arc};

const FF: usize = 3;
const FB: usize = 2;

pub enum FilterType {
    LPF,
    HPF,
}

/// A structure for implementing a biquad filter.
///
/// Biquad Filter implimenation based on https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
pub struct BiquadFilter {
    q: f32,
    cutoff: f32,
    sample_rate: i32,
    r#type: FilterType,

    // Data buffers
    x: CircularBuffer<FF, f32>,
    y: CircularBuffer<FB, f32>,

    // filter coefs
    a: [f32; FB],
    b: [f32; FF],
}

impl BiquadFilter {
    /// Creates a new instance of `BiquadFilter`.
    ///
    /// # Arguments
    ///
    /// * `q` - The Q factor of the filter.
    /// * `cutoff` - The cutoff frequency of the filter.
    /// * `sample_rate` - The sample rate of the audio signal.
    /// * `filter_type` - The type of the filter (low-pass or high-pass).
    ///
    /// # Returns
    ///
    /// A new `BiquadFilter` instance.
    ///
    pub fn new(q: f32, cutoff: f32, sample_rate: i32, filter_type: FilterType) -> Self {
        let mut temp = Self {
            q,
            cutoff,
            sample_rate,
            r#type: filter_type,
            x: CircularBuffer::<FF, f32>::new(),
            y: CircularBuffer::<FB, f32>::new(),
            a: [0.0; FB],
            b: [0.0; FF],
        };

        temp.update_coefs();
        return temp;
    }

    /// Creates a new low-pass filter.
    ///
    /// # Arguments
    ///
    /// * `q` - The Q factor of the filter.
    /// * `cutoff` - The cutoff frequency of the filter.
    /// * `sample_rate` - The sample rate of the audio signal.
    ///
    /// # Returns
    ///
    /// A new `BiquadFilter` instance configured as a low-pass filter.
    pub fn new_lpf(q: f32, cutoff: f32, sample_rate: i32) -> Self {
        Self::new(q, cutoff, sample_rate, FilterType::LPF)
    }

    /// Creates a new high-pass filter.
    ///
    /// # Arguments
    ///
    /// * `q` - The Q factor of the filter.
    /// * `cutoff` - The cutoff frequency of the filter.
    /// * `sample_rate` - The sample rate of the audio signal.
    ///
    /// # Returns
    ///
    /// A new `BiquadFilter` instance configured as a high-pass filter.
    pub fn new_hpf(q: f32, cutoff: f32, sample_rate: i32) -> Self {
        Self::new(q, cutoff, sample_rate, FilterType::HPF)
    }

    /// updated the filter
    ///
    /// # Arguments
    /// * `sample` - New sample to push into the filter buffers
    ///
    /// # Returns
    ///
    /// the output of the filter given the sample input
    ///
    pub fn update(&mut self, sample: f32) -> f32 {
        self.x.push_front(sample);

        let mut y = 0.0;
        y += self.x.iter().zip(self.b).map(|(x, b)| x * b).sum::<f32>();
        y -= self.y.iter().zip(self.a).map(|(y, a)| y * a).sum::<f32>();

        self.y.push_front(y);
        y
    }

    /// updates the filter cutoff frequency, in hz
    pub fn set_cutoff(&mut self, cutoff: f32) {
        self.cutoff = cutoff;
        self.update_coefs();
    }

    /// biquad filter implimenation based on https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html
    ///
    /// Takes self, and updates feed forward and feed back coeficients.
    /// Only called internally after any filter parameters are ajdusted via setter methods
    fn update_coefs(&mut self) {
        let w0 = 2. * PI * self.cutoff / (self.sample_rate as f32);
        let alpha = f32::sin(w0) / (2.0 * self.q);
        let mut a = [0.0; 3];
        let mut b = [0.0; 3];
        match self.r#type {
            FilterType::LPF => {
                b[0] = (1.0 - f32::cos(w0)) / 2.0;
                b[1] = b[0] * 2.0;
                b[2] = b[0];
                a[0] = 1.0 + alpha;
                a[1] = -2.0 * f32::cos(w0);
                a[2] = 1.0 - alpha;
            }
            FilterType::HPF => {
                b[0] = (1.0 + f32::cos(w0)) / 2.0;
                b[1] = -b[0] * 2.0;
                b[2] = b[0];
                a[0] = 1.0 + alpha;
                a[1] = -2.0 * f32::cos(w0);
                a[2] = 1.0 - alpha;
            }
        }
        self.b[0] = b[0] / a[0];
        self.b[1] = b[1] / a[0];
        self.b[2] = b[2] / a[0];
        self.a[0] = a[1] / a[0];
        self.a[1] = a[2] / a[0];
    }
}


/// Performs an FFT on the given data and applies a window function.
///
/// # Arguments
///
/// * `_fft` - An `Arc` containing the FFT processor.
/// * `data` - A vector of input data samples.
///
/// # Returns
///
/// A vector of the FFT result in decibels.
///
pub fn do_fft(_fft: Arc<dyn rustfft::Fft<f32>>, mut data: Vec<f32>) -> Vec<f32> {
    // ham the window
    let len = data.len();
    let mut ham_sum = 0.0;
    for (i, sample) in data.iter_mut().enumerate() {
        let window = 0.5 * (1.0 - (2.0 * std::f32::consts::PI * i as f32 / (len - 1) as f32).cos());
        *sample *= window;
        ham_sum += window;
    }

    let mut buffer: Vec<Complex<f32>> = data.iter().map(|&s| Complex::new(s, 0.0)).collect();

    // do fft
    _fft.process(&mut buffer);

    // normalize fft

    buffer
        .iter()
        .map(|a| (20.0 * (a.norm() * 2.0 / ham_sum / ((i16::MAX / 4 * 3) as f32)).log10()))
        .collect()
}
