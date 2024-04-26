# live-room-sensor
Embedded code to read sensor information about occupation in rooms

## Description
This code is meant to run on a Pico W and reads radar information from either a MicRadar R60AMP1 or a Minewsemi MS72SF1 radar via UART on GPIO 4-5.
In addition to the radar information, the code reads the state of a PIR sensor(or any digital sensor) on GPIO 23.

Then once every minute it will report the state of the PIR sensor and the radar information to a central server via HTTPS.
The certificate for the reporting server is hardcoded to be a Let's Encrypt R3 certificate.

An example of the JSON payload that is sent to the reporting server is:
```json
{
  "firmwareVersion": "0.2.2-Minew",
  "sensorId":"AB:CD:EF:12:34:56",
  "occupants": 3,
  "radarState": 3,
  "pirState": 1
}
```

The code also has a debug console that can be accessed via Bluetooth SPP.
The debug console is password protected and the password is set via the BLUETOOTH_AUTH_TOKEN environment variable.
To connect use a Bluetooth SPP terminal and after connecting send the password followed by a newline and carriage return (often added by the terminal automatically).
After the password is accepted you will see a "Authenticated" message and then you can start sending commands and receiving debug information.

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




 
