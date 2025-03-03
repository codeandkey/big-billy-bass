use std::{net::{SocketAddr, UdpSocket}, time::Duration};
use serde::{Serialize, Deserialize};
use std::error::Error;

pub const DASH_PORT: u16 = 2534;
pub const MAXSIZE: u32 = 32768;

#[derive(Serialize, Deserialize)]
pub enum DashMessage {
    LimbHistory(Vec<(u128, f32, f32)>),
    RmsHistory(Vec<(u128, f32, f32)>),
    NotReady,
}

pub struct DashMessageSender {
    sock: UdpSocket,
    addr: SocketAddr,
}

impl DashMessageSender {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        Ok(Self {
            sock: UdpSocket::bind("0.0.0.0:0")?,
            addr: ([127, 0, 0, 1], DASH_PORT).into(),
        })
    }

    pub fn send(&self, message: DashMessage) -> Result<(), Box<dyn Error>> {
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

pub struct DashMessageReceiver {
    sock: UdpSocket,
}

impl DashMessageReceiver {
    pub fn new() -> Result<Self, Box<dyn Error>> {
        let sock = UdpSocket::bind(format!("127.0.0.1:{DASH_PORT}"))?;
        sock.set_read_timeout(Some(Duration::from_millis(100)))?;
        Ok(Self { sock })
    }

    pub fn recv(&mut self) -> Result<DashMessage, Box<dyn Error>> {
        let mut payload = [0u8; 4];
        let (n_bytes, _source) = self.sock.peek_from(&mut payload)?;

        if n_bytes != payload.len() {
            warn!("Didn't receive enough bytes for size ({n_bytes}/4), skipping");
            return Ok(DashMessage::NotReady);
        }

        let size = u32::from_le_bytes(payload);

        if size > MAXSIZE {
            warn!("Payload size {size} exceeds max {MAXSIZE}, skipping");
            return Ok(DashMessage::NotReady);
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
