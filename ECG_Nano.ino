#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
const char* ssid = "ECG_ESP32";
const char* password = "12345678";
#define RX_PIN 16
#define TX_PIN 17
HardwareSerial ECGSerial(2);
WebServer server(80);
WebSocketsServer webSocket(81);
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>ECG Monitor</title>
<style>
body {
    background: black;
    color: white;
    text-align: center;
    font-family: Arial;
}

h2 {
    color: white;
}

#status {
    color: lime;
    font-size: 20px;
}

canvas {
    width: 95%;
    height: 400px;
    border: 2px solid lime;
    background: black;
}

</style>
</head>

<body>

<h2>Portable ECG Monitor</h2>

<p id="status">Connecting...</p>

<canvas id="ecg"></canvas>

<script>

let socket;

let canvas = document.getElementById("ecg");
let ctx = canvas.getContext("2d");

canvas.width = 800;
canvas.height = 400;

let x = 0;
let previousY = 200;

function connect()
{
    socket = new WebSocket(
        "ws://" + location.hostname + ":81/"
    );

    socket.onopen = function()
    {
        document.getElementById("status").innerHTML =
            "ECG CONNECTED";
    };

    socket.onclose = function()
    {
        document.getElementById("status").innerHTML =
            "DISCONNECTED";

        setTimeout(connect, 2000);
    };

    socket.onmessage = function(event)
    {
        let value = event.data;

        // Lead-off detection
        if (value == "!")
        {
            document.getElementById("status").innerHTML =
                "CHECK ELECTRODES";

            return;
        }

        document.getElementById("status").innerHTML =
            "ECG MONITORING";

        value = parseInt(value);

        // Convert ADC value to screen
        let y = canvas.height -
                (value / 1023.0) *
                canvas.height;

        // Restart waveform
        if (x >= canvas.width)
        {
            ctx.clearRect(
                0,
                0,
                canvas.width,
                canvas.height
            );

            x = 0;
        }

        // Draw ECG
        ctx.beginPath();

        ctx.moveTo(x - 1, previousY);
        ctx.lineTo(x, y);

        ctx.strokeStyle = "lime";
        ctx.lineWidth = 2;

        ctx.stroke();

        previousY = y;

        x++;
    };
}

connect();

</script>

</body>
</html>
)rawliteral";


void webSocketEvent(
    uint8_t num,
    WStype_t type,
    uint8_t *payload,
    size_t length)
{
    if (type == WStype_CONNECTED)
    {
        Serial.println("Mobile connected");
    }

    if (type == WStype_DISCONNECTED)
    {
        Serial.println("Mobile disconnected");
    }
}


void setup()
{
    Serial.begin(115200);

    // ESP32 UART2
    ECGSerial.begin(
        115200,
        SERIAL_8N1,
        RX_PIN,
        TX_PIN
    );

    // Create Wi-Fi network
    WiFi.softAP(
        ssid,
        password
    );

    Serial.println("ECG ESP32 Started");

    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Webpage
    server.on("/", []()
    {
        server.send(
            200,
            "text/html",
            webpage
        );
    });

    server.begin();
    // WebSocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}
void loop()
{
    server.handleClient();

    webSocket.loop();

    // Receive ECG data from Nano
    if (ECGSerial.available())
    {
        String data =
            ECGSerial.readStringUntil('\n');

        data.trim();

        if (data.length() > 0)
        {
            // Send ECG to mobile
            webSocket.broadcastTXT(data);
        }
    }
}
