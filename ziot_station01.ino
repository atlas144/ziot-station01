#include <ESP8266WiFi.h>
// It's necessary to change MQTT_MAX_PACKET_SIZE value in 'PubSubClient.h', it should be (payload byte size + topic byte size + some bytes (0 - 15)) <220 here> (change MQTT_KEEPALIVE is also good idea <150 here>)
#include <PubSubClient.h>
#include <SSD1306Wire.h>
#include <DHT.h>
#include "font.h"

// SETUP
// Wi-Fi
const char* ssid = "Wifi-Svarc";
const char* password = "xp3x275x";
// MQTT
const char* broker = "192.168.0.10";
const int port = 1883;
const String stationID = "94c127f8-6143-42fd-bd88-4fb9939e2cd4";
const String stationType = "station01";

// Support variables
const String topic = stationType + "/" + stationID;
boolean lastConnected = false;
int saveCounter = 1;
char* save;

// Necessary function for MQTT
void callback(char* topic, byte* payload, unsigned int length) {}

// Setup Wi-Fi and MQTT client
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// DHT11 - inside
#define DHT1PIN D2
#define DHT1TYPE DHT11

// DHT22 - outside
#define DHT2PIN D1
#define DHT2TYPE DHT22

DHT dht1(DHT1PIN, DHT1TYPE);
DHT dht2(DHT2PIN, DHT2TYPE);

// OLED
SSD1306Wire  display(0x3c, D6, D5);

// Interruption
const byte interruptPin = 13;
volatile byte interruptCounter = 0;

boolean brokerConnect() {
  Serial.println("MQTT - Attempting connection...");
  if (client.connect((char*) stationID.c_str())) {
      Serial.println("MQTT - OK");
      client.loop();
      if (!lastConnected) {
        Serial.println(client.publish((char*) (topic + "/connect").c_str(), "Connected"));
      } 
      lastConnected = true;
      return true;
    } else {
      Serial.println("MQTT - Failed");
      lastConnected = false;
      return false;
  }
}

void handleInterrupt() { 
  if(interruptCounter == 0) {
    interruptCounter++;
    display.displayOn();
    Serial.println("OLED - ON");
  } else {
    interruptCounter--;
    display.displayOff();
    Serial.println("OLED - OFF");
  }
}

void displayValues(String place, float temp, float hum) {
  display.clear();
  display.setFont(Roboto_Slab_Bold_14);
  display.drawString(0, 0, place);
  display.setFont(Roboto_Slab_Bold_16);
  display.drawString(0, 18, "Teplota: " + String(temp) + "°C");
  display.drawString(0, 40, "Vlhkost: " + String(hum) + " %");
  display.display();
  Serial.println("OLED - OK (write)");
  delay(3000);
}

void setup() {
  // Start serial console
  Serial.begin(115200);
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi - OK");
  Serial.println(WiFi.localIP());
  // Connect to MQTT broker
  client.setServer(broker, port);
  client.setCallback(callback);
  // Start DHT sensors
  dht1.begin();
  Serial.println("DHT1 - OK");
  dht2.begin();
  Serial.println("DHT2 - OK");
  // Set up OLED display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.displayOff();
  Serial.println("OLED - OK");
  // Set up OLED display ON/OFF feature
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, CHANGE);
  Serial.println("OLED - OK (Interruption)");
}

void loop() {
  float tempIn = 0.0;
  float humIn = 0.0;
  while (isnan(tempIn) || isnan(humIn) || !tempIn || !humIn) {
    tempIn = dht1.readTemperature();
    humIn = dht1.readHumidity();
    Serial.printf("%f --- %f\n", tempIn, humIn);
    delay(3000);
  }
  displayValues("Doma", tempIn, humIn);
  float tempOut = 0.0;
  float humOut = 0.0;
  while (isnan(tempOut) || isnan(humOut) || !tempOut || !humOut) {
    tempOut = dht2.readTemperature();
    humOut = dht2.readHumidity();
    Serial.printf("%f --- %f\n", tempOut, humOut);
    delay(3000);
  }
  displayValues("Venku", tempOut, humOut);
  if (brokerConnect()) {
    if (saveCounter < 10) {
      save = "false";
      saveCounter++;
    } else {
      save = "true";
      saveCounter = 1;    
    }
    char payload[149];
    snprintf(payload, 149, "{\"save\": %s, \"data\": {\"inside\": {\"temperature\": %f, \"humidity\": %f}, \"outside\": {\"temperature\": %f, \"humidity\": %f}}}", save, tempIn, humIn, tempOut, humOut);
    Serial.println(payload);
    Serial.println(client.publish((char*) topic.c_str(), payload));
  }
}
