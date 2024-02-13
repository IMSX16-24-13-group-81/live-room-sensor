use serde::Serialize;

#[derive(Serialize)]
pub struct SensorStatus {
    pub id: [char; 12],
    pub count: u32,
}

impl SensorStatus {
    pub fn new(id: [char; 12], count: u32) -> SensorStatus {
        SensorStatus { id, count }
    }
}
