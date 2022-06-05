// Environment monitor with E-paper.
// 2022-05-29  T. Nakagawa

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <soc/rtc_cntl_reg.h>
#include "EPDClass.h"
#include "TIFFG4.h"

constexpr int PIN_CL = 2;
constexpr int PIN_LE = 4;
constexpr int PIN_ADC = 5;
constexpr int PIN_D0 = 12;
constexpr int PIN_SDA = 21;
constexpr int PIN_SCL = 22;
constexpr int PIN_OE = 23;
constexpr int PIN_STL = 25;
constexpr int PIN_MODE = 26;
constexpr int PIN_CKV = 27;
constexpr int PIN_SPV = 32;
constexpr int PIN_WAKEUP = 33;
constexpr int PIN_BAT = 34;
constexpr int PIN_CDS = 35;
constexpr int BUFFER_SIZE = 32 * 1024;
constexpr float LED_VF = 2.74f;
constexpr float BAT_MIN = 3.3f;
constexpr float BAT_MAX = 4.5f;
constexpr int HALL_NEUTRAL = 55;

Preferences preferences;
EPDClass epd(PIN_SPV, PIN_CKV, PIN_MODE, PIN_STL, PIN_OE, PIN_LE, PIN_CL, PIN_D0, PIN_SDA, PIN_SCL, PIN_WAKEUP);
uint8_t buffer[BUFFER_SIZE];
RTC_DATA_ATTR bool booted = false;

float getVoltage() {
  digitalWrite(PIN_ADC, LOW);
  pinMode(PIN_ADC, OUTPUT);
  pinMode(PIN_BAT, ANALOG);
  analogSetAttenuation(ADC_11db);
  delay(10);
  int adc = 0;
  for (int i = 0; i < 100; i++) adc += analogRead(PIN_BAT);
  const float result = adc * (1.0f / 100.0f / 4096.0f / 0.28184f) + LED_VF;
  pinMode(PIN_BAT, INPUT);
  pinMode(PIN_ADC, INPUT);
  return result;
}

float getLuminance() {
  digitalWrite(PIN_ADC, LOW);
  pinMode(PIN_ADC, OUTPUT);
  pinMode(PIN_CDS, ANALOG);
  analogSetAttenuation(ADC_6db);
  delay(10);
  int adc = 0;
  for (int i = 0; i < 100; i++) adc += analogRead(PIN_CDS);
  const float result = adc * (1.0f / 100.0f);
  pinMode(PIN_CDS, INPUT);
  pinMode(PIN_ADC, INPUT);
  return result;
}

bool downloadData(const String &url, uint8_t *data, int size) {
  HTTPClient client;
  Serial.println("Fetching the URL: " + url);
  client.begin(url);
  const int res = client.GET();
  Serial.println("Response: " + String(res));
  if (res != HTTP_CODE_OK) return false;
  const String &page = client.getString();
  Serial.println("Data size: " + String(page.length()));
  if (page.length() > size) return false;
  memcpy(data, page.c_str(), page.length());
  client.end();
  return true;
}

void eraseScreen() {
  static uint8_t buf[EPDClass::WIDTH];
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < EPDClass::WIDTH; j++) buf[j] = (i & 0x01);
    epd.begin();
    for (int j = 0; j < EPDClass::HEIGHT; j++) epd.transfer(buf, true);
    epd.end();
  }
}

void drawIcon(uint8_t *icon, float voltage) {
  const int v = (int)((voltage - BAT_MIN) / (BAT_MAX - BAT_MIN) * (27 - 2 + 1) + 0.5f);
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 32; x++) {
      uint8_t clr = 0;
      if ((y == 1 || y == 6) && (x >= 1 && x <= 28)) clr = 1;
      else if ((x == 1 || x ==  28) && (y >= 1 && y <= 6)) clr = 1;
      else if ((y == 2 || y == 5) && x == 29) clr = 1;
      else if (x == 30 && (y >= 2 && y <= 5)) clr = 1;
      else if ((y >= 2 && y <= 5) && (x >= 2 && x <= 27)) clr = (x - 2 < v) ? 1 : 0;
      icon[y * 32 + x] = clr;
    }
  }
}

void superimposeIcon(uint8_t *data, int row, const uint8_t *icon) {
  const int left = EPDClass::WIDTH - 32;
  const int top = EPDClass::HEIGHT - 8;
  if (row >= top) {
    data += left;
    icon += 32 * (row - top);
    for (int i = 0; i < 32; i++) *data++ = *icon++;
  }
}

void update() {
  preferences.begin("config", true);
  const float wakeup_luminance = preferences.getString("WAKEUP", "0.0").toFloat();
  String url = preferences.getString("URL");
  preferences.end();
  const float luminance = getLuminance();
  Serial.println("Luminance: " + String(luminance));
  if (luminance < wakeup_luminance) return;
  const float voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));

  // Enable WiFi.
  Serial.println("Connecting WiFi... " + String(millis()));
  WiFi.mode(WIFI_STA);
  if (!booted) {
    booted = true;
    Serial.println("Connecting with the SSID information.");
    preferences.begin("config", true);
    WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PASS").c_str(), preferences.getString("CHAN").toInt());
    preferences.end();
  } else {
    WiFi.begin();
  }
  while (WiFi.status() != WL_CONNECTED && millis() < 5000) delay(1);

  // Download the data.
  uint8_t *file = nullptr;
  if (WiFi.status() == WL_CONNECTED) {
    url += "?l=" + String(luminance, 0);
    url += "&b=" + String(voltage, 2);
    Serial.println("Getting the data... " + String(millis()));
    for (int retry = 0; retry < 2; retry++) {
      if (downloadData(url, buffer, BUFFER_SIZE)) {
        file = buffer;
        break;
      }
      delay(3000);
    }
  }

  // Disable WiFi.
  Serial.println("Disconnecting... " + String(millis()));
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Output to the EPD.
  if (file != nullptr) {
    static uint8_t icon[32 * 8];
    drawIcon(icon, voltage);
    Serial.println("Updating the screen... " + String(millis()));
    epd.enable();
    Serial.println("Erasing...");
    eraseScreen();
    Serial.println("Drawing...");
    static uint8_t buf[EPDClass::WIDTH];
    auto transfer = [&](const uint8_t *data, int row) {
      memcpy(buf, data, EPDClass::WIDTH);
      superimposeIcon(buf, row, icon);
      epd.transfer(buf);
    };
    for (int iter = 0; iter < 2; iter++) {
      epd.begin();
      tiffg4_decoder(file, EPDClass::WIDTH, EPDClass::HEIGHT, transfer);
      epd.end();
    }
    epd.disable();
    Serial.println("Update finished. " + String(millis()));
  }
}

void config() {
  Serial.println("Entering the configuration mode.");
  preferences.begin("config", false);
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32", "12345678");
  delay(100);
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));

  WiFiServer server(80);
  server.begin();
  while (true) {
    WiFiClient client = server.available();
    if (client) {
      const String line = client.readStringUntil('\n');
      Serial.println("Accessed: " + line);
      String message;
      if (line.startsWith("GET /?")) {
        String key;
        String val;
        String buf = line.substring(6);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          String tmp = param.substring(pos + 1);
          tmp.replace('+', ' ');
          for (int c = 0; c < 256; c++) {
            String hex = String("%") + ((c < 16) ? "0" : "") + String(c, HEX);
            hex.toUpperCase();
            tmp.replace(hex, String((char)c));
            hex.toLowerCase();
            tmp.replace(hex, String((char)c));
          }
          if (param.substring(0, pos) == "key") {
            key = tmp;
          }
          else if (param.substring(0, pos) == "val") {
            val = tmp;
          }
        }
        key.trim();
        val.trim();
        Serial.println("key=" + key + ", val=" + val);
        if (key == "QUIT") {
          Serial.println("Quit the configuration mode.");
          break;
        }
        if (key.length()) {
          preferences.putString(key.c_str(), val);
          if (preferences.getString(key.c_str()) == val) {
            message = "Succeeded to update: " + key;
          } else {
            message = "Failed to write: " + key;
          }
        } else {
          message = "Key was not specified.";
        }
      }

      const float voltage = getVoltage();
      message += "<br>Voltage=" + String(voltage, 2);
      const float luminance = getLuminance();
      message += "<br>luminance=" + String(luminance, 0);

      client.println("<!DOCTYPE html>");
      client.println("<head><title>Configuration</title></head>");
      client.println("<body>");
      client.println("<h1>Configuration</h1>");
      client.println("<form action=\"/\" method=\"get\">Key: <input type=\"text\" name=\"key\" size=\"10\"> Value: <input type=\"text\" name=\"val\" size=\"20\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
    }

    if (millis() > 5 * 60 * 1000) break;
    digitalWrite(PIN_ADC, LOW);
    pinMode(PIN_ADC, (millis() & (1 << 8)) ? OUTPUT : INPUT);  // LED blink at 1000/256/2Hz.
  }

  Serial.println("Disconnected.");
  server.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  preferences.end();
  Serial.println("Configuration finished.");
  digitalWrite(PIN_ADC, LOW);
  pinMode(PIN_ADC, INPUT);
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.
  digitalWrite(PIN_ADC, LOW);
  pinMode(PIN_ADC, INPUT);
  digitalWrite(PIN_BAT, LOW);
  pinMode(PIN_BAT, INPUT);
  digitalWrite(PIN_CDS, LOW);
  pinMode(PIN_CDS, INPUT);
  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("E-Monitor");

  const int h = hallRead();
  Serial.println("Hall sensor: " + String(h));
  if (h < HALL_NEUTRAL - 40 || h > HALL_NEUTRAL + 40) {
    config();
  } else {
    update();
  }

  // Deep sleep.
  preferences.begin("config", true);
  unsigned long long sleep = preferences.getString("SLEEP", "570").toInt();
  preferences.end();
  Serial.println("Sleeping: " + String((unsigned long)sleep) + "sec.");
  esp_sleep_enable_timer_wakeup(sleep * 1000000);
  esp_deep_sleep_start();
}

void loop() {
}
