
#[toml_cfg::toml_config]
pub struct Config {
    #[default("")]
    pub wifi_ssid: &'static str,
    #[default("")]
    pub wifi_password: &'static str,
    #[default("")]
    pub auth_token: &'static str,
    #[default(60)]
    pub pir_delay: u64,
}


pub fn validate_config() -> Result<(), &'static str> {
    let config = &CONFIG;
    if config.wifi_ssid.is_empty() {
        return Err("wifi_ssid is empty");
    }
    if config.wifi_password.is_empty() {
        return Err("wifi_password is empty");
    }
    if config.auth_token.is_empty() {
        return Err("auth_token is empty");
    }
    Ok(())
}
