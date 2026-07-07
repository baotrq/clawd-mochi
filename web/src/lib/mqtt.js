/**
 * Web MQTT API Wrapper for communicating with the ESP32-C3 firmware over WiFi.
 * Connects to a public MQTT broker via WSS (WebSocket Secure).
 */

import mqtt from 'mqtt';

let client = null;

export function connectMQTT(topicSub, topicPub, onDisconnect, onDataReceived) {
  if (client) {
    disconnectMQTT();
  }

  try {
    // broker.hivemq.com WebSocket secure port is 8884
    const brokerUrl = 'wss://broker.hivemq.com:8884/mqtt';
    
    console.log('Connecting to MQTT broker:', brokerUrl);
    
    client = mqtt.connect(brokerUrl, {
      clientId: 'ClawdWeb-' + Math.random().toString(16).substring(2, 8),
      clean: true,
      connectTimeout: 5000,
      reconnectPeriod: 5000,
    });

    client.on('connect', () => {
      console.log('MQTT: Connected to broker.');
      client.subscribe(topicPub, (err) => {
        if (err) {
          console.error('MQTT: Subscribe failed:', err);
        } else {
          console.log('MQTT: Subscribed to status topic:', topicPub);
        }
      });
    });

    client.on('message', (topic, message) => {
      const payload = message.toString();
      if (onDataReceived) {
        onDataReceived(payload);
      }
    });

    client.on('close', () => {
      console.log('MQTT: Connection closed.');
    });

    client.on('error', (err) => {
      console.error('MQTT: Connection error:', err);
      if (onDisconnect) onDisconnect();
    });

    return true;
  } catch (err) {
    console.error('MQTT: Init failed:', err);
    disconnectMQTT();
    throw err;
  }
}

export function disconnectMQTT() {
  if (client) {
    try {
      client.end();
    } catch (e) {
      console.warn('MQTT: Error closing client:', e);
    }
    client = null;
  }
}

export function publishMQTT(topicSub, data) {
  if (!client || !client.connected) {
    console.warn('MQTT: Client is not connected. Data ignored:', data);
    return false;
  }
  try {
    client.publish(topicSub, data);
    return true;
  } catch (err) {
    console.error('MQTT: Publish failed:', err);
    return false;
  }
}
