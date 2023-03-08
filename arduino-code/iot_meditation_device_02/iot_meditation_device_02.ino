#include "env.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <Seeed_Arduino_GroveAI.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define DEVICE 2

// Wifi
const char* ssid = env_ssid;
const char* password = env_password;
WiFiClient wioClient;
PubSubClient client(wioClient);

// MQTT
const char* mqtt_server = env_mqtt_server;
const char* mqtt_topic_1 = env_mqtt_topic_1;
const char* mqtt_topic_2 = env_mqtt_topic_2;
long lastMsg = 0;
char msg[50];
String msg_received;
String str;

// Display
TFT_eSPI tft;
int display_width = 320;
int display_height = 240;

// Grove AI
GroveAI ai = GroveAI(Wire);

// LED strip
#define LED_COUNT 10
#define LED_PIN 0
Adafruit_NeoPixel led_strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Global variables
int ai_confidence = 60;
int duration_eyes_closed = 0;
int duration_eyes_open = 0;
int meditation_threshold = 2;
bool person = false;
bool eyes_closed = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  // Init display
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);
  tft.setTextSize(2);
  // MQTT
  if (DEVICE == 1) {
    mqtt_topic_1 = env_mqtt_topic_1;
    mqtt_topic_2 = env_mqtt_topic_2;
  } else if (DEVICE == 2) {
    mqtt_topic_1 = env_mqtt_topic_2;
    mqtt_topic_2 = env_mqtt_topic_1;
  }
  // Init wifi
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  // Init camera
  if (!ai.begin(ALGO_OBJECT_DETECTION, MODEL_EXT_INDEX_1)) {
    Serial.println("Failed to find camera");
    while (true);
  }
  // Init LED strip
  led_strip.setBrightness(255);
  led_strip.begin();
}

void loop() {
  long now = millis();

  // Make sure you are connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Start processing video
  if (ai.invoke()) {
    while (ai.state() != CMD_STATE_IDLE);
    uint8_t len = ai.get_result_len();
    if (len > 0) {
      // some result found
      object_detection_t data;
      // find best accuracy to only display this one
      int high = 0, ihigh = -1;
      for (int i = 0; i < len; i++) {
        ai.get_result(i, (uint8_t*)&data, sizeof(object_detection_t));
        if (data.confidence > high) {
          high = data.confidence;
          ihigh = i;
        }
      }
      ai.get_result(ihigh, (uint8_t*)&data, sizeof(object_detection_t));
      if (data.confidence > ai_confidence) {
        int t = data.target;
        person = true;
        Serial.print("Detected target: ");
        switch (data.target) {
          case 0:
            eyes_closed = true;
            duration_eyes_closed++;
            Serial.print("Eyes closed");
            break;
          case 1:
            eyes_closed = false;
            duration_eyes_open++;
            Serial.print("Eyes open");
            if (duration_eyes_open >= meditation_threshold) {
              duration_eyes_closed = 0;
              duration_eyes_open = 0;
            }
            break;
        }
        Serial.print(" with accuracy ");
        Serial.print(data.confidence);
        Serial.println("%");
      }
    } else {
      person = false;
      duration_eyes_closed = 0;
      duration_eyes_open = 0;
    }
  }

  if (person) {
    if (eyes_closed && duration_eyes_closed >= meditation_threshold) {
      set_led_strip(0, 255, 0, 100);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor((display_width - tft.textWidth("sending out good waves")) / 2, display_height/2);
      tft.print("sending out good waves");
      set_led_strip(0, 50, 0, 100);
    } else {
      set_led_strip(0, 0, 255, 100);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor((display_width - tft.textWidth("close your eyes")) / 2, display_height/2);
      tft.print("close your eyes");
      set_led_strip(0, 0, 50, 100);
    }
  } else {
    set_led_strip(255, 0, 0, 100);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((display_width - tft.textWidth("come closer")) / 2, display_height/2);
    tft.print("come closer");
    set_led_strip(50, 0, 0, 100);
  }

  Serial.print("message received: ");
  Serial.println(msg_received);

  if (msg_received == "true") {
    set_led_strip(255, 255, 255, 100);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((display_width - tft.textWidth("receiving good waves <3")) / 2, display_height/2);
    tft.print("receiving good waves <3");
    set_led_strip(50, 50, 50, 100);
  }


  // Send message every second
  if (now - lastMsg > 1000) {
    lastMsg = now;
    if (duration_eyes_closed >= 1) {
      str = "true";
    } else {
      str = "false";
    }
    str.toCharArray(msg, 50);
    if (DEVICE == 1) {
      client.publish(mqtt_topic_1, msg);
    } else if (DEVICE == 2) {
      client.publish(mqtt_topic_2, msg);
    }
    Serial.print("Sending message: ");
    Serial.println(msg);
  }
}

void setup_wifi() {
  delay(10);
  tft.setCursor((display_width - tft.textWidth("connecting to wifi")) / 2, display_height/2);
  tft.print("connecting to wifi");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  tft.fillScreen(TFT_BLACK);
  tft.setCursor((display_width - tft.textWidth("connected!")) / 2, display_height/2);
  tft.print("connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  // Display Local IP Address
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char buff_p[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    buff_p[i] = (char)payload[i];
  }
  Serial.println();
  buff_p[length] = '\0';
  msg_received = String(buff_p);
  Serial.print("Receiving message: ");
  Serial.println(msg_received);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connection...");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((display_width - tft.textWidth("connecting to mqtt")) / 2, display_height/2);
    tft.print("connecting to mqtt");
    String clientId = "WioTerminal-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      tft.fillScreen(TFT_BLACK);
      tft.setCursor((display_width - tft.textWidth("Connected!")) / 2, display_height/2);
      tft.print("connected!");
      if (DEVICE == 1) {
        client.publish(mqtt_topic_1, "hello world");
        client.subscribe(mqtt_topic_2);
      } else if (DEVICE == 2) {
        client.publish(mqtt_topic_2, "hello world");
        client.subscribe(mqtt_topic_1);
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void set_led_strip(int red, int green, int blue, int led_delay) {
  for(int i=0;i<LED_COUNT;i++){
    led_strip.setPixelColor(i, led_strip.Color(red,green,blue));
    led_strip.show();
    delay(led_delay);
  }
}


