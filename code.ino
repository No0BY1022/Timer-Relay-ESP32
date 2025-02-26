#include <WiFi.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "Cutiekatz";
const char* password = "DaisyMilky@143";

const int relayPin = 5;  
bool relayState = false;
bool countdownRunning = false;  
unsigned long relayEndTime = 0;

AsyncWebServer server(80);

// HTML + CSS + JavaScript
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Relay Timer</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background-color: black;
            color: white;
            margin: 0;
            padding: 20px;
        }
        h2 {
            font-size: 24px;
            margin-bottom: 20px;
        }
        .picker {
            font-size: 40px;
            margin-bottom: 20px;
        }
        .picker input {
            font-size: 30px;
            width: 80px;
            text-align: center;
            margin: 5px;
        }
        .timer {
            font-size: 60px;
            font-weight: bold;
            margin: 20px 0;
        }
        button {
            font-size: 22px;
            padding: 12px 20px;
            margin: 10px;
            border: none;
            cursor: pointer;
            width: 150px;
        }
        .start-btn { background-color: green; color: white; }
        .stop-btn { background-color: red; color: white; }
        button:hover { opacity: 0.8; }
    </style>
    <script>
        let countdownInterval;

        function startTimer() {
            let hours = parseInt(document.getElementById('hours').value) || 0;
            let minutes = parseInt(document.getElementById('minutes').value) || 0;
            let seconds = parseInt(document.getElementById('seconds').value) || 0;
            let totalSeconds = (hours * 3600) + (minutes * 60) + seconds;

            if (totalSeconds <= 0) {
                alert("Please enter a valid time.");
                return;
            }

            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/setTimer?time=" + totalSeconds, true);
            xhr.send();

            startCountdown(totalSeconds);
        }

        function stopTimer() {
            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/stopTimer", true);
            xhr.send();

            clearInterval(countdownInterval);
            document.getElementById('countdown').innerText = "00h 00m 00s";
        }

        function startCountdown(duration) {
            clearInterval(countdownInterval);
            let endTime = Date.now() + duration * 1000;

            countdownInterval = setInterval(function () {
                let remaining = Math.max(0, Math.floor((endTime - Date.now()) / 1000));
                let h = Math.floor(remaining / 3600);
                let m = Math.floor((remaining % 3600) / 60);
                let s = remaining % 60;

                document.getElementById('countdown').innerText = 
                    (h < 10 ? "0" : "") + h + "h " + 
                    (m < 10 ? "0" : "") + m + "m " + 
                    (s < 10 ? "0" : "") + s + "s";

                if (remaining <= 0) {
                    clearInterval(countdownInterval);
                    document.getElementById('countdown').innerText = "00h 00m 00s";
                }
            }, 1000);
        }

        function updateRelayStatus() {
            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/relayStatus", true);
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 4 && xhr.status == 200) {
                    let response = JSON.parse(xhr.responseText);
                    if (response.state === "ON") {
                        startCountdown(response.remaining);
                    } else {
                        clearInterval(countdownInterval);
                        document.getElementById('countdown').innerText = "00h 00m 00s";
                    }
                }
            };
            xhr.send();
        }

        setInterval(updateRelayStatus, 2000);
    </script>
</head>
<body>
    <h2>ESP32 Relay Timer Control</h2>
    <div class="picker">
        <input type="number" id="hours" min="0" max="23" value="0"> h
        <input type="number" id="minutes" min="0" max="59" value="1"> m
        <input type="number" id="seconds" min="0" max="59" value="0"> s
    </div>
    <button class="start-btn" onclick="startTimer()">Start Timer</button>
    <button class="stop-btn" onclick="stopTimer()">Stop Timer</button>
    <div class="timer" id="countdown">00h 01m 00s</div>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20) {
        delay(1000);
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi!");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.on("/setTimer", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("time")) {
            int duration = request->getParam("time")->value().toInt();
            relayState = true;
            countdownRunning = true;
            relayEndTime = millis() + (duration * 60000);
            digitalWrite(relayPin, HIGH);
        }
        request->send(200, "text/plain", "Timer Set");
    });

    server.on("/stopTimer", HTTP_GET, [](AsyncWebServerRequest *request){
        digitalWrite(relayPin, LOW);
        relayState = false;
        countdownRunning = false;
        relayEndTime = 0;
        request->send(200, "text/plain", "Timer Stopped");
    });

    server.on("/getTimeRemaining", HTTP_GET, [](AsyncWebServerRequest *request){
        int remaining = (countdownRunning && relayEndTime > millis()) ? (relayEndTime - millis()) / 1000 : 0;
        request->send(200, "text/plain", String(remaining));
    });

    server.begin();
    Serial.println("Web Server Started!");
}

void loop() {
    if (countdownRunning && millis() >= relayEndTime) {
        digitalWrite(relayPin, LOW);
        relayState = false;
        countdownRunning = false;
        Serial.println("Relay OFF");
    }
}
