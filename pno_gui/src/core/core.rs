use tokio::sync::mpsc as tk_mpsc;

use super::{
    gui::{GUIEvent, GUI},
    ToPiOwned, ToTsyOwned,
};

struct Midi {}

struct Core<'gui> {
    from_tsy: tk_mpsc::Receiver<ToPiOwned>,
    to_tsy: tk_mpsc::Sender<ToTsyOwned>,
    midi: tk_mpsc::Sender<Midi>,
    gui: &'gui GUI,
}

enum Event {
    GUIEvent(GUIEvent),
    TsyEvent(ToPiOwned),
    None,
}

impl<'gui> Core<'gui> {
    pub async fn run_core(mut self) {
        let mut gui_sub = self.gui.subscribe();
        loop {
            tokio::select! {
                e = self.from_tsy.recv() => {
                    if let Some(e) = e {
                        self.handle_tsy_pkt(e).await;
                    }
                }
                e = gui_sub.next_event() => {
                    self.handle_gui_evt(e).await;
                }
            }
        }
    }

    async fn handle_tsy_pkt(&mut self, p: ToPiOwned) {
        let ToPiOwned {
            tsy_timestamp,
            tsy_pktid,
            samples,
            acks,
        } = p;
    }

    async fn handle_gui_evt(&mut self, e: GUIEvent) {}
}
