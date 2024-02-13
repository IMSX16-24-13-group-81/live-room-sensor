use serde::Serialize;

const VERSION: Option<&str> = option_env!("CARGO_PKG_VERSION");

#[derive(Serialize)]
pub struct Heartbeat<'a> {
    pub id: [char; 12],
    pub version: &'a str,
}

impl Heartbeat<'_> {
    pub fn new(id: [char; 12]) -> Heartbeat<'static> {
        let version = VERSION.unwrap_or("unknown");
        Heartbeat { id, version }
    }
}
