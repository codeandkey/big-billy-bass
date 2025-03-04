use std::{marker::PhantomData, net::{SocketAddr, UdpSocket}};

use serde::{de::DeserializeOwned, Serialize};

const MAXSIZE: usize = 32768;

pub struct BusReceiver<T> {
    sock: UdpSocket,
    _phantom: PhantomData<T>,
}

impl <T> BusReceiver<T> {
    /// Create a new BusReceiver
    /// 
    /// ## Arguments
    /// port - The communication port
    pub fn new(port: u16) -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            sock: UdpSocket::bind(format!("0.0.0.0:{port}"))?,
            _phantom: PhantomData,
        })
    }

    /// Set the timeout for the receiver
    /// 
    /// ## Arguments
    /// ms - The timeout in milliseconds
    pub fn timeout(&self, ms: u64) -> Result<(), Box<dyn std::error::Error>> {
        self.sock.set_read_timeout(Some(std::time::Duration::from_millis(ms)))?;
        Ok(())
    }

    /// Receive a message from the bus
    /// 
    /// # Returns
    /// Some(T) - The message received
    /// None - If no message was received (timeout or incomplete)
    pub fn recv(&mut self) -> Result<Option<T>, Box<dyn std::error::Error>> where T: DeserializeOwned {
        let mut payload = [0u8; 4];
        let (n_bytes, _source) = self.sock.peek_from(&mut payload)?;

        if n_bytes != 4 {
            warn!("Didn't receive enough bytes for size ({n_bytes}/4), skipping");
            return Ok(None);
        }

        let size = u32::from_le_bytes(payload) as usize;

        if size > MAXSIZE {
            warn!("Payload size {size} exceeds max {MAXSIZE}, skipping");
            return Ok(None);
        }

        let mut payload = vec![0u8; size + 4];
        let (n_bytes, _source) = self.sock.recv_from(&mut payload)?;

        if n_bytes != payload.len() {
            warn!("Didn't receive enough bytes for size ({n_bytes}/{}), skipping", payload.len());
            return Ok(None);
        }

        Ok(Some(bincode::deserialize(&payload[4..])?))
    }
}

pub struct BusSender<T> {
    sock: UdpSocket,
    addr: SocketAddr,
    _phantom: PhantomData<T>,
}

impl <T> BusSender<T> {
    /// Create a new BusSender
    /// 
    /// ## Arguments
    /// port - The communication port
    pub fn new(port: u16) -> Result<Self, Box<dyn std::error::Error>> {
        Ok(Self {
            sock: UdpSocket::bind("0.0.0.0:0")?,
            addr: ([127, 0, 0, 1], port).into(),
            _phantom: PhantomData,
        })
    }

    /// Send a message over the bus
    /// 
    /// ## Arguments
    /// message - The message to send
    pub fn send(&self, message: T) -> Result<(), Box<dyn std::error::Error>> where T: Serialize {
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