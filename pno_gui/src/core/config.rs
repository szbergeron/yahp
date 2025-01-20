use tokio::sync::mpsc::{Receiver, Sender};

use super::{ToPiOwned, ToTsyOwned};

pub struct YAHPConfig {
    keyboard: KeyboardConfig,
    controller: ControllerConfig,
    sampler: SamplerConfig,
}

pub struct KeyboardConfig {
    //
}

pub struct SamplerConfig {

}

pub struct ControllerConfig {
}

pub fn enroll(rx: Receiver<ToPiOwned>, tx: Sender<ToTsyOwned>) {
    //
}
