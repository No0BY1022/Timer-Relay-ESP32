#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "Cutiekatz";        // Change to your WiFi SSID
const char* password = "DaisyMilky@143"; // Change to your WiFi password

WebServer server(80);
const int RELAY_PIN = 5;

unsigned long relayEndTime = 0;  // Store when the relay should turn off

// HTML + JavaScript UI
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Relay Timer Control</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background-color: black; color: white; }
        .timer { font-size: 60px; margin-top: 20px; }
        .picker { font-size: 40px; }
        button { font-size: 20px; padding: 10px; margin-top: 20px; }
    </style>
    <script>
        let countdownInterval;

        function startTimer() {
            let hours = document.getElementById('hours').value;
            let minutes = document.getElementById('minutes').value;
            let seconds = document.getElementById('seconds').value;
            let totalSeconds = (hours * 3600) + (minutes * 60) + parseInt(seconds);

            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/setTimer?time=" + totalSeconds, true);
            xhr.send();

            alert("Relay set for " + hours + "h " + minutes + "m " + seconds + "s");
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

        // Fetch relay status when the page loads
        window.onload = updateRelayStatus;
    </script>
</head>
<body>
    <h2>ESP32 Relay Timer Control</h2>
    <div class="picker">
        <input type="number" id="hours" min="0" max="23" value="0"> h
        <input type="number" id="minutes" min="0" max="59" value="1"> m
        <input type="number" id="seconds" min="0" max="59" value="0"> s
    </div>
    <button onclick="startTimer()">Set Timer</button>
    <button onclick="stopTimer()">Stop Timer</button>
    <div class="timer" id="countdown">00h 01m 00s</div>
</body>
</html>
)rawliteral";

// Start Timer Handler
void handleSetTimer() {
    if (server.hasArg("time")) {
        int duration = server.arg("time").toInt();
        relayEndTime = millis() + (duration * 1000);  // Set relay end time
        digitalWrite(RELAY_PIN, HIGH); // Turn on relay
        server.send(200, "text/plain", "Timer Set");
    }
}

// Stop Timer Handler
void handleStopTimer() {
    relayEndTime = 0;
    digitalWrite(RELAY_PIN, LOW);  // Turn off relay
    server.send(200, "text/plain", "Timer Stopped");
}

// Send Relay Status
void handleRelayStatus() {
    unsigned long now = millis();
    int remaining = (relayEndTime > now) ? (relayEndTime - now) / 1000 : 0;

    if (remaining == 0) {
        digitalWrite(RELAY_PIN, LOW);  // Ensure relay is off
    }

    String response = "{\"state\":\"" + String((remaining > 0) ? "ON" : "OFF") +
                      "\", \"remaining\":" + String(remaining) + "}";
    server.send(200, "application/json", response);
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi Connected!");
    Serial.print("ESP32 Web Server IP: ");
    Serial.println(WiFi.localIP());

    server.on("/", []() { server.send_P(200, "text/html", index_html); });
    server.on("/setTimer", handleSetTimer);
    server.on("/stopTimer", handleStopTimer);
    server.on("/relayStatus", handleRelayStatus);

    server.begin();
}

void loop() {
    server.handleClient();

    // Check if the timer has expired and turn off relay
    if (relayEndTime > 0 && millis() >= relayEndTime) {
        relayEndTime = 0;
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Relay turned OFF (Timer ended)");
    }
}
