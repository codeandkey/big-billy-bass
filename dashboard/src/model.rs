use std::collections::LinkedList;

pub struct Model {
    limb_hist: LinkedList<Vec<(u128, f32, f32)>>,
    rms_hist: LinkedList<Vec<(u128, f32, f32)>>,
}

pub enum Limb {
    Body,
    Mouth,
}

pub enum Dataset {
    Rms,
    PinOut,
}

const MAXFRAMES: usize = 40;

impl Model {
    pub fn new() -> Self {
        Self {
            limb_hist: LinkedList::new(),
            rms_hist: LinkedList::new(),
        }
    }

    pub fn submit_dataset(&mut self, set: Dataset, data: Vec<(u128, f32, f32)>) {
        let buf = match set {
            Dataset::PinOut => &mut self.limb_hist,
            Dataset::Rms => &mut self.rms_hist,
        };

        buf.push_front(data);

        while buf.len() > MAXFRAMES {
            buf.pop_back();
        }
    }

    pub fn dataset(&self, set: Dataset, limb: Limb) -> (Vec<(f64, f64)>, f64, f64) {
        let buf = match set {
            Dataset::PinOut => &self.limb_hist,
            Dataset::Rms => &self.rms_hist,
        };

        let fsize = match buf.front() {
            Some(f) => f.len(),
            None => return (vec![], 0.0, 0.0),
        };

        let mut out = Vec::<(u128, f32)>::with_capacity(buf.len() * fsize);
        let mut mintime = u128::max_value();
        let mut maxtime = u128::min_value();

        for f in buf {
            for (t, b, m) in f {
                mintime = mintime.min(*t);
                maxtime = maxtime.max(*t);

                let i = match limb {
                    Limb::Body => b,
                    Limb::Mouth => m
                };

                out.push((*t, *i));
            }
        }

        (
            out.into_iter().map(|(t, x)| (t as f64, x as f64)).collect(),
            mintime as f64,
            maxtime as f64,
        )
    }
}
