use std::collections::VecDeque;

/// A structure for calculating the moving root mean square (RMS) of a sequence of samples.
///
/// # Fields
///
/// * `running_sum` - The running sum of the squared samples.
/// * `samples` - A deque containing the most recent samples.
/// * `target_size` - The desired size of the sample buffer.
///
pub struct MovingRms {
    running_sum: f32,
    samples: VecDeque<f32>,
    target_size: usize,
}

impl MovingRms {
    /// Creates a new instance of `MovingRms`.
    ///
    /// This function initializes a new `MovingRms` instance with an empty sample buffer,
    /// a running sum of zero, and a target size of zero.
    ///
    /// # Returns
    ///
    /// A new `MovingRms` instance.
    pub fn new() -> Self {
        Self {
            running_sum: 0.0,
            samples: VecDeque::new(),
            target_size: 0,
        }
    }

    pub fn new_w_size(target_size: usize) -> Self {
        let mut rms = Self::new();
        rms.set_size(target_size);
        rms
    }

    /// Updates the moving RMS calculation with a new sample.
    ///
    /// # Arguments
    ///
    /// * `sample` - The new sample to be added to the moving RMS calculation.
    ///
    /// # Returns
    ///
    /// The current RMS value after adding the new sample.
    pub fn update(&mut self, sample: f32) -> f32 {
        self.samples.push_front(sample.powf(2.0));
        self.running_sum += sample.powf(2.0);
        self.set_size(self.target_size);
        f32::sqrt(self.running_sum / self.target_size as f32)
    }

    /// Sets the target size for the moving RMS calculation.
    ///
    /// # Arguments
    ///
    /// * `target_size` - The desired size of the sample buffer.
    ///
    pub fn set_size(&mut self, target_size: usize) {
        while target_size < self.samples.len() {
            if let Some(sample) = self.samples.pop_back() {
                self.running_sum -= sample;
            }
        }
        self.target_size = target_size;
    }

    pub fn value(&self) -> f32 {
        if self.target_size == 0 {
            return 0.0;
        }
        f32::sqrt(self.running_sum / self.target_size as f32)
    }
}

pub struct SustainDecay {
    sustain: f32,
    decay: f32,

    max_value: f32,
    decay_value: f32,

    decay_slope: f32,

    dt_sustain: f32,
    dt_decay: f32,
}

impl SustainDecay {
    pub fn new_w_decay(decay_value: f32) -> Self {
        Self {
            sustain: 20.0,
            decay: 5.0,
            max_value: decay_value,
            decay_value: decay_value,

            decay_slope: 0.0,

            dt_sustain: 0.0,
            dt_decay: 0.0,
        }
    }

    pub fn update(&mut self, sample: f32, dt: f32) -> f32 {
        if sample > self.max_value {
            self.max_value = sample;
            self.decay_slope = (self.max_value - self.decay_value) / self.decay;
            self.dt_sustain = 0.0;
            self.dt_decay = 0.0;
        } else {
            self.dt_sustain += dt;
            if self.dt_sustain > self.sustain {
                self.dt_decay += dt;
                if self.dt_decay < self.decay {
                    // Decay phase
                    self.max_value -= self.decay_slope * dt;
                } else {
                    // Reset phase
                    self.max_value = self.decay_value;
                }
            }
        }
        self.max_value
    }
}
