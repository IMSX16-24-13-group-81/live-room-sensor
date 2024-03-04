
#[toml_cfg::toml_config]
pub struct Config {
    #[default("")]
    pub wifi_ssid: &'static str,
    #[default("")]
    pub wifi_password: &'static str,
    #[default("")]
    pub auth_token: &'static str,
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

#[macro_export]
macro_rules! validate_config {
    () => {
        if CONFIG.wifi_ssid.is_empty() {
            compile_error!("wifi_network is empty");
        }
        if CONFIG.wifi_password.is_empty() {
            compile_error!("wifi_password is empty");
        }
        if CONFIG.auth_token.is_empty() {
            compile_error!("auth_token is empty");
        }
    };
}
