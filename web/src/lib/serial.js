/**
 * Web Serial API Wrapper for communicating with the ESP32-C3 firmware over USB Serial.
 */

let port = null;
let writer = null;
let reader = null;
let keepReading = false;
let disconnectListener = null; // the one 'disconnect' handler currently registered, if any

export const isSerialSupported = () => {
  return typeof navigator !== 'undefined' && 'serial' in navigator;
};

// Opens an already-obtained SerialPort (from requestPort() or getPorts())
// and wires up the writer/reader/disconnect-listener plumbing shared by
// both a user-initiated connect and a silent auto-reconnect.
async function openPort(requestedPort, onDisconnect, onDataReceived) {
  await requestedPort.open({ baudRate: 115200 });

  // The device can drop off USB right as it's opening (flaky radio/USB on
  // this board) — check the streams exist instead of assuming they do.
  if (!requestedPort.writable || !requestedPort.readable) {
    throw new Error('Port opened but has no data streams — the device likely reset or dropped off USB. Unplug/replug it and try again.');
  }

  port = requestedPort;

  // Set up text encoder and writer
  const textEncoder = new TextEncoderStream();
  textEncoder.readable.pipeTo(port.writable).catch(() => {});
  writer = textEncoder.writable.getWriter();

  // Monitor this specific port's disconnection — bound to requestedPort
  // (not the shared, mutable `port` variable), and always removed by
  // disconnectSerial() so it can't outlive this session.
  disconnectListener = (event) => {
    if (event.port === requestedPort) {
      disconnectSerial();
      if (onDisconnect) onDisconnect();
    }
  };
  navigator.serial.addEventListener('disconnect', disconnectListener);

  // Start read loop if requested
  if (onDataReceived) {
    startReadLoop(onDataReceived);
  }

  return true;
}

export async function connectSerial(onDisconnect, onDataReceived) {
  if (!isSerialSupported()) {
    throw new Error('Web Serial API is not supported in this browser. Please use Chrome, Edge, or Opera.');
  }

  // Tear down any previous session first — this also removes its
  // 'disconnect' listener so stale listeners never pile up on
  // navigator.serial across repeated connect/disconnect cycles (a leaked
  // listener from an old session firing mid-connect is what used to crash
  // this function with "Cannot read properties of null (reading 'writable')").
  if (port || disconnectListener) {
    await disconnectSerial();
  }

  let requestedPort = null;
  try {
    // Request a port (shows the browser's device picker) and open it.
    requestedPort = await navigator.serial.requestPort();
    return await openPort(requestedPort, onDisconnect, onDataReceived);
  } catch (err) {
    console.error('Serial connection failed:', err);
    if (requestedPort) port = requestedPort;
    await disconnectSerial();
    throw err;
  }
}

// Silently reconnects to a port this origin was already granted permission
// for — navigator.serial.getPorts() needs no user gesture (unlike
// requestPort()), so this can run on page load. Never throws; returns false
// if there's no unambiguous previously-authorized port (none, or more than
// one — picking among several would be a guess), so the caller just falls
// back to the manual Connect button.
export async function tryAutoConnectSerial(onDisconnect, onDataReceived) {
  if (!isSerialSupported()) return false;
  if (port || disconnectListener) {
    await disconnectSerial();
  }
  let ports;
  try {
    ports = await navigator.serial.getPorts();
  } catch {
    return false;
  }
  if (ports.length !== 1) return false;
  try {
    return await openPort(ports[0], onDisconnect, onDataReceived);
  } catch (err) {
    console.warn('Auto-reconnect failed, falling back to manual connect:', err);
    await disconnectSerial();
    return false;
  }
}

export async function disconnectSerial() {
  keepReading = false;

  if (disconnectListener) {
    navigator.serial.removeEventListener('disconnect', disconnectListener);
    disconnectListener = null;
  }

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
