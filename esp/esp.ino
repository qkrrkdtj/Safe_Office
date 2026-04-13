#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

SoftwareSerial stmSerial(D5, D6);   // RX, TX

const char* ssid = "********";
const char* password = "********";
const char* serverUrl = "********";

void setup() {
  Serial.begin(115200);
  stmSerial.begin(115200);

  Serial.println();
  Serial.println("ESP8266 camera trigger test start");

  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi connected, IP = ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (stmSerial.available()) {
    String msg = stmSerial.readStringUntil('\n');
    msg.trim();

    Serial.print("RX from STM32: ");
    Serial.println(msg);

    if (msg == "HELLO" || msg == "CAPTURE") {
      if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;

        http.begin(client, serverUrl);
        int httpCode = http.GET();

        if (httpCode > 0) {
          String payload = http.getString();
          payload.trim();

          Serial.print("HTTP code: ");
          Serial.println(httpCode);
          Serial.print("Pi response(count): ");
          Serial.println(payload);

          // 받은 count 값을 그대로 STM32로 전달
          stmSerial.print(payload);
          stmSerial.print('\n');

          Serial.print("TX to STM32: ");
          Serial.println(payload);
        } else {
          Serial.print("HTTP request failed: ");
          Serial.println(httpCode);
          stmSerial.print("-1\n");
        }

        http.end();
      } else {
        Serial.println("WiFi disconnected");
        stmSerial.print("-1\n");
      }
    }
  }
}