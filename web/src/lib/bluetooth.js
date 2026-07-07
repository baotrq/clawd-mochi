/**
 * Web Bluetooth API Wrapper for communicating with the ESP32-C3 firmware over BLE.
 * Uses the Nordic UART Service (NUS) UUIDs for generic serial-like stream behavior.
 */

const SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const CHARACTERISTIC_UUID_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"; // rx: write to device
const CHARACTERISTIC_UUID_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"; // tx: read/notify from device

let bluetoothDevice = null;
let gattServer = null;
let rxCharacteristic = null;
let txCharacteristic = null;
let disconnectListener = null;

export const isBluetoothSupported = () => {
  return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
};

export async function connectBluetooth(onDisconnect, onDataReceived) {
  if (!isBluetoothSupported()) {
    throw new Error('Web Bluetooth is not supported in this browser. Please use Chrome, Edge, or Opera.');
  }

  if (bluetoothDevice) {
    await disconnectBluetooth();
  }

  try {
    console.log('Requesting Bluetooth Device...');
    bluetoothDevice = await navigator.bluetooth.requestDevice({
      filters: [
        { name: 'Clawd Mochi' },
        { services: [SERVICE_UUID] }
      ],
      optionalServices: [SERVICE_UUID]
    });

    // Set up disconnect listener
    disconnectListener = () => {
      console.log('Bluetooth device disconnected');
      cleanupBluetooth();
      if (onDisconnect) onDisconnect();
    };
    bluetoothDevice.addEventListener('gattserverdisconnected', disconnectListener);

    console.log('Connecting to GATT Server...');
    gattServer = await bluetoothDevice.gatt.connect();

    console.log('Getting Service...');
    const service = await gattServer.getPrimaryService(SERVICE_UUID);

    console.log('Getting Characteristics...');
    rxCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID_RX);
    txCharacteristic = await service.getCharacteristic(CHARACTERISTIC_UUID_TX);

    // Set up notification listener for RX data (TX from the device's perspective)
    if (onDataReceived) {
      const textDecoder = new TextDecoder();
      txCharacteristic.addEventListener('characteristicvaluechanged', (event) => {
        const value = event.target.value;
        const text = textDecoder.decode(value);
        onDataReceived(text);
      });
      await txCharacteristic.startNotifications();
      console.log('Notifications started.');
    }

    return true;
  } catch (err) {
    console.error('Bluetooth connection failed:', err);
    cleanupBluetooth();
    throw err;
  }
}

export async function disconnectBluetooth() {
  if (bluetoothDevice && bluetoothDevice.gatt.connected) {
    bluetoothDevice.gatt.disconnect();
  }
  cleanupBluetooth();
}

function cleanupBluetooth() {
  if (bluetoothDevice && disconnectListener) {
    bluetoothDevice.removeEventListener('gattserverdisconnected', disconnectListener);
  }
  bluetoothDevice = null;
  gattServer = null;
  rxCharacteristic = null;
  txCharacteristic = null;
  disconnectListener = null;
}

export async function writeBluetooth(data) {
  if (!rxCharacteristic) {
    console.warn('Bluetooth is not connected. Data ignored:', data);
    return false;
  }
  try {
    const textEncoder = new TextEncoder();
    const encoded = textEncoder.encode(data);
    
    // MTU is usually around 20 bytes on basic BLE connections, so let's chunk writes to be safe
    const CHUNK_SIZE = 20;
    for (let i = 0; i < encoded.length; i += CHUNK_SIZE) {
      const chunk = encoded.slice(i, i + CHUNK_SIZE);
      await rxCharacteristic.writeValueWithoutResponse(chunk);
    }
    return true;
  } catch (err) {
    console.error('Failed to write to Bluetooth characteristic:', err);
    return false;
  }
}
