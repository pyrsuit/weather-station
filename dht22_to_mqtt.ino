#include <DHT22.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
const int mqtt_port = 1883;
const char* mqtt_temperature_topic = "weather/indoor-sensor/temperature";
const char* mqtt_humidity_topic = "weather/indoor-sensor/humidity";

#define pinDATA SDA

DHT22 dht22(pinDATA);

const char* ssid = "ssid";
const char* password = "password";
const char* mqtt_server = "mqtt_server";
 
WiFiClient espClient;
PubSubClient client(espClient);

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC

// Connect to Wi-Fi
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi ...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: ");
  Serial.println(WiFi.localIP());
}

// Reconnect to the MQTT server if disconnected
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection ...");
    if (client.connect("NodeMCUClient")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  timeClient.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Handle incoming messages

  timeClient.update();

  // Read sensor values
  float humidity = dht22.getHumidity();
  float temperature = dht22.getTemperature();

  if (dht22.getLastError() != dht22.OK) {
    Serial.print("last error :");
    Serial.println(dht22.getLastError());
  }

  // Check if readings are valid
  if (!isnan(humidity) && !isnan(temperature)) {
    unsigned long timestamp = timeClient.getEpochTime();

    // Prepare temperature payload as JSON
    String temperature_payload = "{\"metric\": \"temperature\", \"value\": " + String(temperature) + ", \"unit\": \"℃\", \"timestamp\": " + String((uint64_t)timestamp) + "}";
  
    // Prepare humidity payload as JSON
    String humidity_payload = "{\"metric\": \"humidity\", \"value\": " + String(humidity) + ", \"unit\": \"%\", \"timestamp\": " + String((uint64_t)timestamp) + "}";

    // Publish temperature
    client.publish(mqtt_temperature_topic, temperature_payload.c_str());
    Serial.println("Published temperature: " + temperature_payload);

    // Publish humidity
    client.publish(mqtt_humidity_topic, humidity_payload.c_str());
    Serial.println("Published humidity: " + humidity_payload);

  } else {
    Serial.println("Failed to read from DHT sensor");
  }

  delay(600000); // Wait 10 minutes before the next reading
}
