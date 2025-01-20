use std::{sync::atomic::AtomicU64, time::Instant};

use serialport::{SerialPortType, UsbPortInfo};

mod fbi;
mod gui;
mod core;
mod sensor;
mod config;

pub struct ToPiOwned {
    pub tsy_timestamp: u64,
    pub tsy_pktid: u64,

    pub samples: Vec<Sample>,

    pub acks: Vec<u64>,
}

struct ToTsyOwned {
}

fn main() {
    println!("Hello, world!");
}

struct Pipe {
    //https://docs.rs/buffer/latest/buffer/trait.Buffer.html
    // stuff
    // hm
    // for stuff in here (and implementing the methods on pipe)
    // I can do that later, it depends on what physical layer
    // transport we decide to use is
    //okay
}

enum SendError {
    Disconnected,
    BufferFull,
}

impl Pipe {
    /// If there are `count` bytes available, return those bytes
    /// Otherwise, return `None`
    pub fn try_read_bytes(&mut self, count: usize) -> Option<Vec<u8>> {
        if self.available_bytes() >= count {
            let bytes = self.read_bytes(count);
            Some(bytes)
        } else {
            None
        }
    }

    /// waits for `count` bytes to have been received before
    /// returning at all
    pub fn read_bytes(&mut self, count: usize) -> Vec<u8> {
        todo!()
    }

    /// Returns the number of bytes that have been received
    /// but not yet consumed
    pub fn available_bytes(&self) -> usize {
        todo!()
    }

    /// Send all of the bytes, and if necessary wait for all of them to send
    pub fn send(&mut self, bytes: &[u8]) -> Result<(), SendError> {
        todo!()
    }
}

pub struct RxMessage {
    /// This is provided by the teensy
    send_timestamp: u64,

    /// This is also provided by the teensy
    teensy_messageid: u64,

    /// This isn't read from the pipe, but is actually just stamped onto the message as soon as
    /// we start reading it from the pipe
    recv_timestamp: Instant,

    /// This is also assigned by the pi, and isn't actually transmitted,
    /// but is a monotonically increasing counter
    pi_messageid: u64,

    data: RxMessagePayload,
}

pub enum RxMessagePayload {
    Samples(Vec<Sample>),
}

pub struct Sample {
    sensor: u32,
    value: u32,
    timestamp: u64,
}

pub struct TxMessage {
    pi_messageid: u64,

    data: TxMessagePayload,
}

pub enum TxMessagePayload {
    MidiCommand(Vec<MidiCommand>),
}

pub struct MidiCommand {
    channel: u8,
    //
}

const MESSAGE_ID_GEN: AtomicU64 = AtomicU64::new(0);
pub fn next_message_id() -> u64 {
    MESSAGE_ID_GEN.fetch_add(1, std::sync::atomic::Ordering::AcqRel)
}

pub fn read_message(pipe: &mut Pipe) -> RxMessage {
    // akshita's function

    // each receive message is this structure:
    /*
       // all integers are little endian (le)
       8 bytes: u64 of teensy timestamp when this message was transmitted
       8 bytes: u64 of the message id of this message on the teensy
       1 byte: which type of data this message contains ("dtype")
       switch(dtype):
           0: this message contains samples
               4 bytes: u32 of the count of samples contained in this message
               array of messages, each with structure:
                   4 bytes: u32 of sensor id
                   4 bytes: measured value of sensor, as u32
                   8 bytes: teensy timestamp that this was captured at, as u64
    */

    //https://users.rust-lang.org/t/from-bytes-to-u32-in-c-vs-rust/62713

    let timestamp_bytes = pipe.read_bytes(8);
    let timestamp = u64::from_le_bytes(timestamp_bytes.try_into().unwrap());

    let message_id_bytes = pipe.read_bytes(8);
    let message_id = u64::from_le_bytes(message_id_bytes.try_into().unwrap());

    let recv_timestamp = Instant::now();

    let dtype = pipe.read_bytes(1)[0];

    let data = match dtype {
        0 => {
            let count_of_samples_bytes = pipe.read_bytes(4);
            let count_of_samples = u32::from_le_bytes(count_of_samples_bytes.try_into().unwrap());

            // make an empty vector here ===========
            let mut samples = Vec::new();

            // https://users.rust-lang.org/t/initializing-an-array-of-structs/36586
            // https://doc.rust-lang.org/std/vec/struct.Vec.html
            // https://doc.rust-lang.org/std/vec/struct.Vec.html#method.with_capacity

            let mut count_sample: Vec<Sample> = Vec::with_capacity(count_of_samples as usize);

            for i in 0..count_of_samples {
                //
                let sensor_id_bytes = pipe.read_bytes(4);
                let sensor_id = u32::from_le_bytes(sensor_id_bytes.try_into().unwrap());

                let sensor_value_bytes = pipe.read_bytes(4);
                let sensor_value = u32::from_le_bytes(sensor_value_bytes.try_into().unwrap());

                let teensy_timestamp_bytes = pipe.read_bytes(8);
                let teensy_timestamp =
                    u64::from_le_bytes(teensy_timestamp_bytes.try_into().unwrap());

                let sample = Sample {
                    sensor: sensor_id,
                    value: sensor_value,
                    timestamp: teensy_timestamp,
                };

                samples.push(sample);

                // put things into vector here =======
            }

            RxMessagePayload::Samples(samples)

        }

        _ => {
            panic!("dtype wasn't a message type we know of?")
        }
    };

    RxMessage {
        send_timestamp: timestamp,
        teensy_messageid: message_id,
        data,
        recv_timestamp: recv_timestamp,
        pi_messageid: next_message_id(),
    }
}

pub fn f() {
    let ports = serialport::available_ports().expect("couldn't open serial ports?");

    /*let teensy = ports.into_iter().find(|p| {
        match p.port_type {
            SerialPortType::UsbPort(u) => {
                match u.product {
                    Some(s) => s.contains("MIDI") && s.contains("Teensy"),
                    None => false,
                }
            }
            _ => false,
        }
    }).expect("couldn't find teensy usb serial!");*/

    // baud rate shouldn't matter here, since this is over usb
    /*let teensy = serialport::new(teensy.port_name, 14400).open_native().expect("couldn't open serial");*/

    // both this and the C++ side agree that the maximum individual packet size should
    // always be well below 4096 bytes, (inexact, but ...eh), so here we scan for the
    // first 0 and then attempt COBS decode until the next delimiter

}
