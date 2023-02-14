#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <Seeed_Arduino_GroveAI.h>
#include <Wire.h>
#include <WS2812FX.h>

#define DEVICE 1

// Wifi
const char* ssid = "Akasha Hub";
const char* password = "q!=gh8PnvFrYSz5T";
WiFiClient wioClient;
PubSubClient client(wioClient);

// MQTT
const char* mqtt_server = "public.mqtthq.com";
const char* mqtt_topic_1 = "mqtthq-ayamola-iot02-01";
const char* mqtt_topic_2 = "mqtthq-ayamola-iot02-02";
long lastMsg = 0;
char msg[50];
String str;

// Display
TFT_eSPI tft;

// Grove AI
GroveAI ai = GroveAI(Wire);

// LED strip
#define LED_COUNT 10
#define LED_PIN 0
WS2812FX led_strip = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Global variables
int ai_confidence = 60;
int duration_eyes_closed = 0;
int duration_eyes_open = 0;
int meditation_threshold = 0;
bool person = false;
bool eyes_closed = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  // Init display
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);
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
  led_strip.init();
  led_strip.setColor(WHITE);
  led_strip.setBrightness(250);
  led_strip.start();
}

void loop() {
  long now = millis();
  led_strip.service();

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
            duration_eyes_closed ++;
            Serial.print("Eyes closed");
            break;
          case 1:
            eyes_closed = false;
            duration_eyes_open ++;
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
    }
  }

  if (person) {
    if (eyes_closed && duration_eyes_closed >= meditation_threshold) {
      led_strip.setSegment(0, 0, LED_COUNT-1, FX_MODE_DUAL_SCAN, MAGENTA, 1, false);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor((320 - tft.textWidth("Meditating...")) / 2, 120);
      tft.print("Meditating...");
    } else {
      led_strip.setSegment(0, 0, LED_COUNT-1, FX_MODE_SCAN, WHITE, 1, false);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor((320 - tft.textWidth("Close your eyes")) / 2, 120);
      tft.print("Close your eyes");
    }
  } else {
    led_strip.setSegment(0, 0, LED_COUNT-1, FX_MODE_BREATH, WHITE, 1, false);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((320 - tft.textWidth("Come closer")) / 2, 120);
    tft.print("Come closer");
  }


  // Send message every second
  if (now - lastMsg > 1000) {
    lastMsg = now;
    str = String(duration_eyes_closed);
    str.toCharArray(msg,50); 
    client.publish(mqtt_topic_1, msg);
  }

}

void setup_wifi() {
  delay(10);
  tft.setTextSize(2);
  tft.setCursor((320 - tft.textWidth("Connecting to Wi-Fi..")) / 2, 120);
  tft.print("Connecting to Wi-Fi..");
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
  tft.setCursor((320 - tft.textWidth("Connected!")) / 2, 120);
  tft.print("Connected!");
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
  String msg_p = String(buff_p);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor((320 - tft.textWidth("MQTT Message")) / 2, 90);
  tft.print("MQTT Message: ");
  tft.setCursor((320 - tft.textWidth(msg_p)) / 2, 120);
  tft.print(msg_p);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "WioTerminal-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.publish(mqtt_topic_1, "hello world");
      client.subscribe(mqtt_topic_2);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
