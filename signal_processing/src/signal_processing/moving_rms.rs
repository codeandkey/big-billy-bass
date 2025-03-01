use std::collections::VecDeque;

pub struct MovingRms {
    running_sum: f32,
    samples: VecDeque<f32>,
    target_size: usize,
}

impl MovingRms {
    pub fn new() -> Self {
        Self {
            running_sum: 0.0,
            samples: VecDeque::new(),
            target_size: 0,
        }
    }

    pub fn update(&mut self, sample: f32) -> f32 {
        self.samples.push_front(sample.powf(2.0));
        self.set_size(self.target_size);
        f32::sqrt(self.running_sum / self.target_size as f32)
    }

    pub fn set_size(&mut self, target_size: usize) {
        while target_size < self.samples.len() {
            if let Some(sample) = self.samples.pop_back() {
                self.running_sum -= sample;
            }
        }
        self.target_size = target_size;
    }
}
