#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include "DHT.h"

#include <string>
#include <sstream>
// GCC 4.8.2 workaround
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) \
  || !defined(MQTT_USER) || !defined(MQTT_PASSWORD)
#error "WIFI_SSID, WIFI_PASSWORD, MQTT_USER, MQTT_PASSWORD defines expected"
#endif
#define Q(x) #x
#define QUOTE(x) Q(x)

template <typename T>
std::string ToString(T val)
{
    std::ostringstream stream;
    stream << val;
    return stream.str();
}

#define FLIP_BIT(from,to,n) ((((n >> (31-from)) & 1) << (31-to)) | (((n >> (31-to)) & 1) << (31-from)))
// The library that I'm using expects a different order bits in a bytes than the one that I caputured.
// Example: my capture: 0x02A008F7, what the library wants: 0x400510EF
#define CONV(n) (\
  FLIP_BIT(0, 7, n) | FLIP_BIT(1, 6, n) | FLIP_BIT(2, 5, n) | FLIP_BIT(3, 4, n) | \
  FLIP_BIT(8, 15, n) | FLIP_BIT(9, 14, n) | FLIP_BIT(10, 13, n) | FLIP_BIT(11, 12, n) | \
  FLIP_BIT(16, 23, n) | FLIP_BIT(17, 22, n) | FLIP_BIT(18, 21, n) | FLIP_BIT(19, 20, n) | \
  FLIP_BIT(24, 31, n) | FLIP_BIT(25, 30, n) | FLIP_BIT(26, 29, n) | FLIP_BIT(27, 28, n))

const char* ssid = QUOTE(WIFI_SSID);
static_assert(sizeof(QUOTE(WIFI_SSID)) > 1, "empty WIFI_SSID");
const char* password =  QUOTE(WIFI_PASSWORD);
static_assert(sizeof(QUOTE(WIFI_PASSWORD)) > 1, "empty WIFI_PASSWORD");
const char* mqttServer = "192.168.0.100";
const int mqttPort = 1883;
const char* mqttUser = QUOTE(MQTT_USER);
static_assert(sizeof(QUOTE(MQTT_USER)) > 1, "empty MQTT_USER");
const char* mqttPassword = QUOTE(MQTT_PASSWORD);
static_assert(sizeof(QUOTE(MQTT_PASSWORD)) > 1, "empty MQTT_PASSWORD");
const uint16_t kInternalLed = 2;
const uint16_t kIrLed = 4;  // ESP8266 GPIO pin to use. Recommended: 4 (D2).
const uint16_t kDHT = D1;

using namespace std;

WiFiClient espClient;
PubSubClient client(espClient);
IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.
DHT dht;
unsigned long dhtSampling;

void c_mqtt_callback(char* c_topic, byte* c_payload, unsigned int length);

void setup() {
  pinMode(kInternalLed, OUTPUT);
  irsend.begin();
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  dht.setup(kDHT);
  dhtSampling = dht.getMinimumSamplingPeriod();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  client.setServer(mqttServer, mqttPort);
  client.setCallback(c_mqtt_callback);

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("espRemote", mqttUser, mqttPassword )) {

      Serial.println("connected");

    } else {

      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

  client.subscribe("logitech-remote/led", 0);
  client.subscribe("logitech-remote/power", 0);
  client.subscribe("logitech-remote/input", 0);
  client.subscribe("logitech-remote/mute", 0);
  client.subscribe("logitech-remote/level", 0);
  client.subscribe("logitech-remote/minus", 0);
  client.subscribe("logitech-remote/plus", 0);
  client.subscribe("logitech-remote/effect", 0);
}

void mqtt_callback(string topic, string payload) {
  if (topic == "logitech-remote/led") {
    if (payload == "on") {
      digitalWrite(kInternalLed, LOW);
    } else if (payload == "off") {
      digitalWrite(kInternalLed, HIGH);
    }
  } else if (topic == "logitech-remote/power") {
    irsend.sendNEC(CONV(0x02A0807F), 32);
  } else if (topic == "logitech-remote/input") {
    irsend.sendNEC(CONV(0x02A008F7), 32);
  } else if (topic == "logitech-remote/mute") {
    irsend.sendNEC(CONV(0x02A0EA15), 32);
  } else if (topic == "logitech-remote/level") {
    irsend.sendNEC(CONV(0x02A00AF5), 32);
  } else if (topic == "logitech-remote/minus") {
    irsend.sendNEC(CONV(0x02A06A95), 32);
  } else if (topic == "logitech-remote/plus") {
    irsend.sendNEC(CONV(0x02A0AA55), 32);
  } else if (topic == "logitech-remote/effect") {
    irsend.sendNEC(CONV(0x02A00EF1), 32);
  }

}

void c_mqtt_callback(char* c_topic, byte* c_payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(c_topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)c_payload[i]);
  }

  Serial.println();
  Serial.println("-----------------------");
  std::string topic(c_topic);
  std::string payload(reinterpret_cast<char*>(c_payload), length);
  mqtt_callback(topic, payload);
}

unsigned long lastDHTSampling = 0;

void loop() {
  unsigned long t = millis();
  client.loop();
  if (t - lastDHTSampling >= dhtSampling) {
    int humidity = dht.getHumidity();
    int temperature = dht.getTemperature();
    std::string status(dht.getStatusString());
    if (status == "OK") {
      Serial.print(humidity);
      Serial.print("%RH | ");
      Serial.print(temperature);
      Serial.println("*C");
      client.publish("living-room/humidity", ToString(humidity).c_str(), true);
      client.publish("living-room/temperature", ToString(temperature).c_str(),
        true);
    }
    lastDHTSampling = t;
  }
}
