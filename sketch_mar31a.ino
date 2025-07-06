#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include <PMS.h>

WiFiUDP udp;
Preferences preferences;

// ==== Wi-Fi Settings ====
const char* wifiList[][2] = {
  {"SSID", "password"} //this is wifi credentials
};
const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);
String lastSSID, lastPass;
bool wifiConnected = false;

// ==== DHT Settings ====
#define DHTPIN 13
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ==== Hardware Pins ====
#define LED 12
#define BUZZER 14

// ==== PMS5003 Settings ====
#define PMS_RX 27
#define PMS_TX 26
HardwareSerial pmsSerial(1);
PMS pms(pmsSerial);
PMS::DATA pmsData;

// ==== Server ====
WiFiServer server(80);

// ==== Connect to Wi-Fi ====
bool connectToWiFi(int timeoutPerNetwork = 7000) {
  preferences.begin("wifi", true);
  lastSSID = preferences.getString("ssid", "");
  lastPass = preferences.getString("pass", "");
  preferences.end();

  Serial.print("Trying saved WiFi: ");
  Serial.println(lastSSID);

  if (lastSSID != "") {
    WiFi.begin(lastSSID.c_str(), lastPass.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutPerNetwork) delay(250);
    if (WiFi.status() == WL_CONNECTED) return true;
  }

  for (int i = 0; i < wifiCount; i++) {
    Serial.print("Trying: ");
    Serial.println(wifiList[i][0]);

    WiFi.begin(wifiList[i][0], wifiList[i][1]);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutPerNetwork) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      preferences.begin("wifi", false);
      preferences.putString("ssid", wifiList[i][0]);
      preferences.putString("pass", wifiList[i][1]);
      preferences.end();
      return true;
    }
  }

  return false;
}


// ==== Air Quality Alert ====
void airQualityAlert() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED, HIGH);
    tone(BUZZER, 1000);
    delay(300);
    noTone(BUZZER);
    digitalWrite(LED, LOW);
    delay(300);
  }
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);

  wifiConnected = connectToWiFi();
  if (wifiConnected) {
    Serial.print("Connected to WiFi. IP: ");
    Serial.println(WiFi.localIP());
    server.begin();
  } else {
    Serial.println("WiFi connection failed.");
  }

  dht.begin();
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pmsSerial.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pms.passiveMode();
  pms.wakeUp();

  
}

// ==== Main Loop ====
void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");
    String request = client.readStringUntil('\r');
    client.flush();
    Serial.println(request);

    if (request.indexOf("GET /data") >= 0) {
      String json = String("{") +
        "\"temperature\":" + String(temperature, 2) + "," +
        "\"humidity\":" + String(humidity, 2) + "," +
        "\"pm1_0\":" + String(pmsData.PM_AE_UG_1_0) + "," +
        "\"pm2_5\":" + String(pmsData.PM_AE_UG_2_5) + "," +
        "\"pm10\":" + String(pmsData.PM_AE_UG_10_0) +
      "}";
  
// send json to the app
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      client.println(json);

      Serial.println("Sent JSON:");
      Serial.println(json);
    }

    delay(5);
    client.stop();
    Serial.println("Client disconnected");
  }

  

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("Temp: %.2f °C, Humidity: %.2f %%\n", temperature, humidity);
  } else {
    Serial.println("Failed to read DHT22.");
  }

  pms.requestRead();
  if (pms.readUntil(pmsData)) {
    Serial.printf("PM1.0: %d µg/m³, PM2.5: %d µg/m³, PM10: %d µg/m³\n",
                  pmsData.PM_AE_UG_1_0, pmsData.PM_AE_UG_2_5, pmsData.PM_AE_UG_10_0);

    if (pmsData.PM_AE_UG_2_5 > 100 || pmsData.PM_AE_UG_10_0 > 150) airQualityAlert();
  } else {
    Serial.println("No PMS5003 data...");
  }

  Serial.println("");
  delay(5000);  // Reduce delay for better responsiveness
}
