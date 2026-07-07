// Part of the Clawd Mochi sketch. MQTT client for receiving wireless commands over WiFi.
#include <MQTT.h>

WiFiClient netClient;
MQTTClient mqttClient(512); // Buffer size 512 bytes

static uint32_t lastMqttRetryMs = 0;
static const uint32_t MQTT_RETRY_INTERVAL_MS = 10000; // Retry every 10s
static bool mqttInitialized = false;

// Forward declarations
void handleChar(char c);

void messageReceived(String &topic, String &payload) {
  Serial.printf("MQTT Received [%s]: %s\n", topic.c_str(), payload.c_str());
  
  // Forward each character to handleChar
  for (unsigned int i = 0; i < payload.length(); i++) {
    handleChar(payload[i]);
  }
}

void initMQTT() {
  String broker = MQTT_BROKER;
  if (broker.length() == 0) {
    Serial.println("MQTT: Broker not configured in secrets.h. MQTT disabled.");
    return;
  }

  mqttClient.begin(broker.c_str(), 1883, netClient);
  mqttClient.onMessage(messageReceived);
  mqttInitialized = true;
  Serial.println("MQTT: Initialized.");
}

void connectMQTT() {
  if (!mqttInitialized || WiFi.status() != WL_CONNECTED) {
    return;
  }

  Serial.println("MQTT: Attempting connection...");
  
  // Generate a random client ID to avoid collisions on public broker
  String clientId = "ClawdMochi-" + String(random(1000, 9999));
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("MQTT: Connected!");
    mqttClient.subscribe(MQTT_TOPIC_SUB);
    Serial.printf("MQTT: Subscribed to topic: %s\n", MQTT_TOPIC_SUB);
  } else {
    Serial.print("MQTT: Connect failed. State: ");
    Serial.println(mqttClient.lastError());
  }
}

void updateMQTT() {
  if (!mqttInitialized) {
    return;
  }

  uint32_t now = millis();

  // If WiFi is disconnected, reset client
  if (WiFi.status() != WL_CONNECTED) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    return;
  }

  // Handle client loop if connected
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  // Reconnect if disconnected
  if (now - lastMqttRetryMs >= MQTT_RETRY_INTERVAL_MS) {
    lastMqttRetryMs = now;
    connectMQTT();
  }
}

// Function to publish status back to the web client
void mqttPublishStatus(const String& msg) {
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_PUB, msg);
  }
}
