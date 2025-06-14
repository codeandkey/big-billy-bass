use circular_buffer::CircularBuffer;
use std::f32::consts::PI;

const FF: usize = 3;
const FB: usize = 2;

pub enum FilterType {
    LPF,
    HPF,
}

pub struct BiquadFilter {
    _q: f32,
    _cutoff: f32,
    _sample_rate: i32,
    _type: FilterType,

    // Data buffers
    _x: CircularBuffer<FF, f32>,
    _y: CircularBuffer<FB, f32>,

    // filter coefs
    _a: [f32; FB],
    _b: [f32; FF],
}

impl BiquadFilter {
    pub fn new(_q: f32, _cutoff: f32, _sample_rate: i32, filter_type: FilterType) -> Self {
        let mut temp = Self {
            _q,
            _cutoff,
            _sample_rate,
            _type: filter_type,
            _x: CircularBuffer::<FF, f32>::new(),
            _y: CircularBuffer::<FB, f32>::new(),
            _a: [0.0; FB],
            _b: [0.0; FF],
        };

        temp.update_coefs();
        return temp;
    }

    pub fn new_lpf(q: f32, cutoff: f32, sample_rate: i32) -> Self {
        Self::new(q, cutoff, sample_rate, FilterType::LPF)
    }
    pub fn new_hpf(q: f32, cutoff: f32, sample_rate: i32) -> Self {
        Self::new(q, cutoff, sample_rate, FilterType::HPF)
    }

    pub fn update(&mut self, sample: f32) -> f32 {
        self._x.push_front(sample);

        let mut y = 0.0;
        y += self._x.iter().zip(self._b).map(|(x, b)| x * b).sum::<f32>();
        y -= self._y.iter().zip(self._a).map(|(y, a)| y * a).sum::<f32>();

        self._y.push_front(y);
        y
    }

    #[allow(dead_code)]
    pub fn set_q(&mut self, q: f32) {
        self._q = q;
        self.update_coefs();
    }

    pub fn set_cutoff(&mut self, cutoff: f32) {
        self._cutoff = cutoff;
        self.update_coefs();
    }

    #[allow(dead_code)]
    pub fn set_sample_rate(&mut self, fs: f32) {
        self._sample_rate = fs as i32;
        self.update_coefs();
    }

    fn update_coefs(&mut self) {
        let w0 = 2. * PI * self._cutoff / (self._sample_rate as f32);
        let alpha = f32::sin(w0) / (2.0 * self._q);
        let mut a = [0.0; 3];
        let mut b = [0.0; 3];
        match self._type {
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
        self._b[0] = b[0] / a[0];
        self._b[1] = b[1] / a[0];
        self._b[2] = b[2] / a[0];
        self._a[0] = a[1] / a[0];
        self._a[1] = a[2] / a[0];
    }
}
