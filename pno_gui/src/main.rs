use dioxus::prelude::*;
use dioxus_elements::{input, p};

mod core;

const FAVICON: Asset = asset!("/assets/favicon.ico");
const COLORS_CSS: Asset = asset!("/assets/colors.css");
const MAIN_CSS: Asset = asset!("/assets/main.css");
const HEADER_SVG: Asset = asset!("/assets/header.svg");
const TAILWIND_CSS: Asset = asset!("/assets/tailwind.css");

fn main() {
    dioxus::launch(App);
}

#[component]
fn App() -> Element {
    rsx! {
        document::Link { rel: "icon", href: FAVICON }
        document::Link { rel: "stylesheet", href: COLORS_CSS }
        document::Link { rel: "stylesheet", href: MAIN_CSS } document::Link { rel: "stylesheet", href: TAILWIND_CSS }
        //Slider { label: "slider label", low: 0.0, high: 1.0, default: 0.5 }
        SliderBatch {}

    }
}

/*#[derive(Props, PartialEq, Clone)]
struct SliderProps {
    limit_low: f64,
    limit_high: f64,
    label: String,
}*/

#[component]
pub fn Slider(label: String, low: f64, high: f64, default: f64) -> Element {
    let slider_label = label;
    rsx! {
        div {
            class: "slider_div_vertical",
            input {
                r#type: "range",
                class: "slider_vertical",
                min: low,
                max: high,
                value: default,
                step: "any",
                class: "slider",
            },
            p {
                class: "slider_label",
                "{slider_label}"
            },
        }
    }
}

#[component]
pub fn SliderBatch() -> Element {
    rsx! {
        div {
            class: "slider_batch",
            {(0..15).map(|i| rsx! {
                Slider { label: format!("label{i}"), low: 0.0, high: 1.0, default: 0.5 }
            })}
        }
    }
}

#[component]
pub fn Hero() -> Element {
    let mut count = use_signal(|| 0);

    rsx! {
        div {
            id: "hero",
            //img { src: HEADER_SVG, id: "header" }
            div { id: "links",
                a { href: "https://dioxuslabs.com/learn/0.6/", "📚 Learn Dioxus" }
            }
        }
    }
}

#[component]
pub fn Entry() -> Element {
    rsx! {
        div {
            p {
                "asdfjklfsdjalk"
            }
            p {
                "adsfhgiohedsiojf"
            }
        }
    }
}
