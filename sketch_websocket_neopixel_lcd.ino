#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

#define DATA_PIN 16
#define NUM_LEDS 13

CRGB leds[NUM_LEDS];

const char* ssid     = "CR1500";
const char* password = "28643111";

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ── Webpage ──────────────────────────────────────────────────────
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/p5.js/1.9.0/p5.min.js"></script>
  <script src="https://unpkg.com/ml5@0.6.1/dist/ml5.min.js"></script>
</head>
<body style="margin:0">
<script>
  let video;
  let detector;
  let detections = [];
  let socket;
  let lastState = "";
  let lastSeenTime = 0;
  let timeout = 1000;

  function preload() {
    detector = ml5.objectDetector('cocossd');
  }

  function setup() {
    createCanvas(640, 480);
    video = createCapture(VIDEO);
    video.size(640, 480);
    video.hide();
    detector.detect(video, gotDetections);

    socket = new WebSocket("ws://192.168.10.44:81/");
    socket.onopen = () => console.log("Connected to ESP32!");
    socket.onerror = (e) => console.log("WebSocket error", e);
  }

  function gotDetections(error, results) {
    if (error) { console.error(error); return; }
    detections = results;
    detector.detect(video, gotDetections);
  }
  function draw() {
    image(video, 0, 0);
    let personDetectedNow = false;

    for (let i = 0; i < detections.length; i++) {
      let object = detections[i];
      stroke(0, 255, 0);
      strokeWeight(4);
      noFill();
      rect(object.x, object.y, object.width, object.height);
      noStroke();
      fill(255);
      textSize(24);
      text(object.label, object.x + 10, object.y + 24);
      if (object.label === "person") personDetectedNow = true;
    }

    if (personDetectedNow) {
      lastSeenTime = millis();
      updateTrafficLight(true);
    } else {
      if (millis() - lastSeenTime > timeout) {
        updateTrafficLight(false);
      }
    }

    noStroke();
    fill(0, 255, 0);
    textSize(18);
    text("State: " + lastState, 10, 20);
  }

  function sendState(state) {
    if (socket.readyState === WebSocket.OPEN && state !== lastState) {
      socket.send(state);
      lastState = state;
      console.log("Sent:", state);
    }
  }

  function updateTrafficLight(isPerson) {
    sendState(isPerson ? "green" : "red");
  }
</script>
</body>
</html>
  )rawhtml";
  server.send(200, "text/html", html);
}

// ── WebSocket handler ────────────────────────────────────────────
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[%u] Client connected\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Client disconnected\n", num);
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      msg.toLowerCase();
      Serial.printf("Received: %s\n", msg.c_str());
      if (msg == "green") {
        leds[1] = CRGB::Green;
        leds[2] = CRGB::Black;
      } else if (msg == "red") {
        leds[1] = CRGB::Black;
        leds[2] = CRGB::Red;
      } else if (msg == "off") {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
      }
      FastLED.show();
      break;
    }
  }
}

// ── Setup ────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot); // ← THIS was missing!
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ── Loop ─────────────────────────────────────────────────────────
void loop() {
  webSocket.loop();
  server.handleClient();
}