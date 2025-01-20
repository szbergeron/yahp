pub struct GUI {
}

pub enum GUIEvent {
}

pub struct GUIEventSubscriber {
}

impl GUIEventSubscriber {
    pub async fn next_event(&mut self) -> GUIEvent {
        todo!()
    }
}

/// GUI is an interior mutable...thing
impl GUI {
    pub fn subscribe(&self) -> GUIEventSubscriber {
        todo!()
    }
}
