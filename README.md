# live-room-sensor
Embedded code to read sensor information about occupation in rooms

## Description
This code is ment to run on a Pico W and reads radar information from ether a MicRadar R60AMP1 or a Minewsemi MS72SF1 radar via UART on GPIO 4-5.
In addition to the radar information, the code reads the state of a PIR sensor(or any digital sensor) on GPIO 23.

Then once every minute it will report the state of the PIR sensor and the radar information to a central server via HTTPS.
The certificate for the reporting server is hardcoded to be a Let's Encrypt R3 certificate.

An example of the JSON payload that is sent to the reporing server is:
```json
{
  "firmwareVersion": "0.2.2-Minew",
  "sensorId":"AB:CD:EF:12:34:56",
  "occupants": 3,
  "radarState": 3,
  "pirState": 1
}
```

## Building
To build the code CMAKE expects a few environment variables to be set:

| Variable             | Description                                     | Example value       |
|----------------------|-------------------------------------------------|---------------------|
| WIFI_SSID            | The wifi SSID to connect to                     | MySSID              |
| WIFI_PASS            | The wifi password                               | MyPassword          |
| REPORT_API_KEY       | The API key to send in the Authorization header | SuperSecretToken    |
| REPORTING_SERVER     | The server/FQDN to send the reports to          | example.com         |
| REPORTING_PATH       | The path on the server to send the report to    | /api/sensors/report |
| BLUETOOTH_AUTH_TOKEN | The password to use for the SPP debug console   | Password123         |
| USE_NEW_MINEW_RADAR  | If defined, will use the new Minew radar        | anything            |

The version of the firmware is set in the CMakeLists.txt file.
When making a new release, the version should be updated in the CMakeLists.txt file.




 
