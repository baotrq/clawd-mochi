/**
 * Web Serial API Wrapper for communicating with the ESP32-C3 firmware over USB Serial.
 */

let port = null;
let writer = null;
let reader = null;
let keepReading = false;

export const isSerialSupported = () => {
  return typeof navigator !== 'undefined' && 'serial' in navigator;
};

export async function connectSerial(onDisconnect, onDataReceived) {
  if (!isSerialSupported()) {
    throw new Error('Web Serial API is not supported in this browser. Please use Chrome, Edge, or Opera.');
  }

  try {
    // Request a port and open a connection.
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });

    // Set up text encoder and writer
    const textEncoder = new TextEncoderStream();
    const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
    writer = textEncoder.writable.getWriter();

    // Monitor port disconnection events
    navigator.serial.addEventListener('disconnect', (event) => {
      if (event.port === port) {
        disconnectSerial();
        if (onDisconnect) onDisconnect();
      }
    });

    // Start read loop if requested
    if (onDataReceived) {
      startReadLoop(onDataReceived);
    }

    return true;
  } catch (err) {
    console.error('Serial connection failed:', err);
    await disconnectSerial();
    throw err;
  }
}

export async function disconnectSerial() {
  keepReading = false;

  if (reader) {
    try {
      await reader.cancel();
    } catch (e) {
      console.warn('Error cancelling reader:', e);
    }
    reader = null;
  }

  if (writer) {
    try {
      await writer.close();
    } catch (e) {
      console.warn('Error closing writer:', e);
    }
    writer = null;
  }

  if (port) {
    try {
      await port.close();
    } catch (e) {
      console.warn('Error closing port:', e);
    }
    port = null;
  }
}

export async function writeSerial(data) {
  if (!writer) {
    console.warn('Serial is not connected. Data ignored:', data);
    return false;
  }
  try {
    await writer.write(data);
    return true;
  } catch (err) {
    console.error('Failed to write to serial port:', err);
    return false;
  }
}

async function startReadLoop(onDataReceived) {
  if (!port || !port.readable) return;
  
  keepReading = true;
  const textDecoder = new TextDecoderStream();
  const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
  reader = textDecoder.readable.getReader();

  try {
    while (keepReading) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      if (value && onDataReceived) {
        onDataReceived(value);
      }
    }
  } catch (err) {
    console.error('Serial read loop error:', err);
  } finally {
    try {
      reader.releaseLock();
    } catch (e) {}
  }
}
