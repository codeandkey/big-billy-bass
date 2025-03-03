#[macro_use]
extern crate log;

use serde::{Deserialize, Serialize};
use std::{
    error::Error,
    net::{SocketAddr, UdpSocket},
};

pub mod param;
pub mod dash;

pub const COMM_PORT: u16 = 2134;
pub const MAXSIZE: u32 = 32768;
pub type Sample = i16;

#[derive(Serialize, Deserialize)]
pub enum GpioMessage {
    SetSampleRate(u32),
    NextFrame(Vec<Sample>, Vec<Sample>),
    RequestQuit,
    NotReady,
}

pub struct GpioMessageSender {
    sock: UdpSocket,
    addr: SocketAddr,
}

impl GpioMessageSender {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        Ok(Self {
            sock: UdpSocket::bind("0.0.0.0:0")?,
            addr: ([127, 0, 0, 1], COMM_PORT).into(),
        })
    }

    pub fn send(&self, message: GpioMessage) -> Result<(), Box<dyn Error>> {
        let payload = bincode::serialize(&message)?;
        let size_payload = u32::to_le_bytes(payload.len() as u32);
        let message = [size_payload.to_vec(), payload].concat();

        let n_bytes = self.sock.send_to(&message, self.addr)?;

        if n_bytes != message.len() {
            return Err(format!(
                "Send payload size operation returned bad byte count {n_bytes}/{}",
                message.len()
            )
            .into());
        }

        Ok(())
    }
}

pub struct GpioMessageReceiver {
    sock: UdpSocket,
}

impl GpioMessageReceiver {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        Ok(Self {
            sock: UdpSocket::bind(format!("127.0.0.1:{COMM_PORT}"))?,
        })
    }

    pub fn recv(&mut self) -> Result<GpioMessage, Box<dyn Error>> {
        let mut payload = [0u8; 4];
        let (n_bytes, _source) = self.sock.peek_from(&mut payload)?;

        if n_bytes != payload.len() {
            warn!("Didn't receive enough bytes for size ({n_bytes}/4), skipping");
            return Ok(GpioMessage::NotReady);
        }

        let size = u32::from_le_bytes(payload);

        if size > MAXSIZE {
            warn!("Payload size {size} exceeds max {MAXSIZE}, skipping");
            return Ok(GpioMessage::NotReady);
        }

        let mut payload = vec![0u8; size as usize + 4];
        let (n_bytes, _source) = self.sock.recv_from(&mut payload)?;

        if n_bytes != payload.len() {
            return Err(format!(
                "Didn't receive enough payload bytes {n_bytes}/{}",
                payload.len()
            )
            .into());
        }

        Ok(bincode::deserialize(&payload[4..])?)
    }
}
