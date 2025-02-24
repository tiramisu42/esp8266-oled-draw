#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

const char* ssid = "OLED_DRAW";
const char* password = "12345678";

IPAddress apIP(172, 0, 0, 1);

DNSServer dnsServer;

ESP8266WebServer server(80);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void handleRoot();
void handleDraw();
void handleClear();
void configWebServer();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");


  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("STARTING!!!");
  display.display();
  delay(2000);
  display.clearDisplay();
  delay(100);

  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  bool apSuccess = WiFi.softAP(ssid, password);
  if (apSuccess) {
    Serial.print("AP started with SSID: ");
    Serial.println(ssid);
  } else {
    Serial.println("AP failed to start");
    while (1);
  }

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  dnsServer.start(53, "*", apIP);

  configWebServer();
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Connect to http://");
  Serial.println(myIP);

  Serial.println("Setup complete!");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}

void configWebServer() {
  server.on("/", handleRoot);
  server.on("/draw", HTTP_POST, handleDraw);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound([]() {
    Serial.println("404: Not Found");
    server.send(404, "text/plain", "404: Not Found");
  });
}

void handleRoot() {
  String html = "<!DOCTYPE html>\
  <html>\
  <head>\
    <title>esp8266 draw</title>\
  </head>\
  <body>\
    <h1>tiramisu draw @m5stick_loni2</h1>\
    <canvas id='drawCanvas' width='128' height='64' style='border:1px solid #000000;'></canvas>\
    <br>\
    <button onclick='clearCanvas()'>Clear</button>\
    <script>\
      const canvas = document.getElementById('drawCanvas');\
      const ctx = canvas.getContext('2d');\
      let drawing = false;\
      canvas.addEventListener('mousedown', startDrawing);\
      canvas.addEventListener('mouseup', stopDrawing);\
      canvas.addEventListener('mouseout', stopDrawing);\
      canvas.addEventListener('mousemove', draw);\
\
      function startDrawing(e) {\
        drawing = true;\
        draw(e);\
      }\
\
      function stopDrawing() {\
        drawing = false;\
        ctx.beginPath();\
      }\
\
      function draw(e) {\
        if (!drawing) return;\
        let rect = canvas.getBoundingClientRect();\
        let x = e.clientX - rect.left;\
        let y = e.clientY - rect.top;\
        ctx.lineWidth = 1;\
        ctx.lineCap = 'round';\
        ctx.lineTo(x, y);\
        ctx.stroke();\
        sendPoint(x, y);\
        ctx.beginPath();\
        ctx.moveTo(x, y);\
      }\
\
      async function sendPoint(x, y) {\
        const data = { x: x, y: y };\
        try {\
          const response = await fetch('/draw', {\
            method: 'POST',\
            headers: {\
              'Content-Type': 'application/json'\
            },\
            body: JSON.stringify(data)\
          });\
          if (!response.ok) {\
            console.error('Error sending point:', response.status);\
          }\
        } catch (error) {\
          console.error('Error sending point:', error);\
        }\
      }\
\
      async function clearCanvas() {\
        ctx.clearRect(0, 0, canvas.width, canvas.height);\
        try {\
          const response = await fetch('/clear', {\
            method: 'POST'\
          });\
          if (!response.ok) {\
            console.error('Error clearing canvas:', response.status);\
          }\
        } catch (error) {\
          console.error('Error clearing canvas:', error);\
        }\
      }\
    </script>\
  </body>\
  </html>";
  server.send(200, "text/html", html);
}

void handleDraw() {
  Serial.println("handleDraw called");
  if (!server.hasArg("plain")) {
    Serial.println("No data received");
    server.send(400, "text/plain", "No data received");
    return;
  }

  String jsonString = server.arg("plain");
  Serial.print("Received JSON: ");
  Serial.println(jsonString);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  if (!doc.containsKey("x") || !doc.containsKey("y")) {
    Serial.println("Missing 'x' or 'y' in JSON");
    server.send(400, "text/plain", "Missing 'x' or 'y' in JSON");
    return;
  }

  int x = doc["x"];
  int y = doc["y"];

  Serial.print("X: "); Serial.println(x);
  Serial.print("Y: "); Serial.println(y);

  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
    Serial.println("Coordinates out of bounds");
    server.send(400, "text/plain", "Coordinates out of bounds");
    return;
  }

  display.drawPixel(x, y, WHITE);
  display.display();

  Serial.println("Pixel drawn");
  server.send(200, "text/plain", "Point received");
}

void handleClear() {
  Serial.println("Clearing OLED display");
  display.clearDisplay();
  display.display();
  server.send(200, "text/plain", "OLED cleared");
}
