let dns_client = embassy_net::dns::DnsSocket::new(stack);
let tcp_client_state: TcpClientState<2, 4096, 4096> = TcpClientState::new();
let mut tcp_client = TcpClient::new(stack, &tcp_client_state);
let mut http_client = HttpClient::new(&mut tcp_client, &dns_client);

let mut rx = [0; 4096];

loop {

    control.gpio_set(0, false).await;
    
    Timer::after(Duration::from_secs(1)).await;

    let mut request = 
    http_client.request(reqwless::request::Method::GET, "http://google.com").await
    .unwrap();
    let response = request.send(&mut rx).await;

    match response {
        Ok(response) => {
            let body = response.body().body_buf;
            info!("response: {}", body);
        }
        Err(err) => {
            info!("request failed: {:?}", err);
        }
        
    }

    
    control.gpio_set(0, true).await;

}