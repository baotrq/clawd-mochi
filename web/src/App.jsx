import { useState, useEffect, useRef } from 'react'
import RoomScene from './components/RoomScene.jsx'
import Clawd from './components/Clawd.jsx'
import HotspotOverlay from './components/HotspotOverlay.jsx'
import ControlPanel from './components/ControlPanel.jsx'

import {
  isSerialSupported,
  connectSerial,
  disconnectSerial,
  writeSerial as rawWriteSerial,
} from './lib/serial.js'

import {
  isBluetoothSupported,
  connectBluetooth,
  disconnectBluetooth,
  writeBluetooth,
} from './lib/bluetooth.js'

import {
  connectMQTT,
  disconnectMQTT,
  publishMQTT,
} from './lib/mqtt.js'

function removeVietnameseTones(str) {
  if (!str) return '';
  let result = String(str);
  
  // Map specific Vietnamese accented characters to plain English letters
  result = result.replace(/[àáạảãâầấậẩẫăằắặẳẵ]/g, "a");
  result = result.replace(/[èéẹẻẽêềếệểễ]/g, "e");
  result = result.replace(/[ìíịỉĩ]/g, "i");
  result = result.replace(/[òóọỏõôồốộổỗơờớợởỡ]/g, "o");
  result = result.replace(/[ùúụủũưừứựửữ]/g, "u");
  result = result.replace(/[ỳýỵỷỹ]/g, "y");
  result = result.replace(/[đ]/g, "d");
  
  result = result.replace(/[ÀÁẠẢÃÂẦẤẬẨẪĂẰẮẶẲẴ]/g, "A");
  result = result.replace(/[ÈÉẸẺẼÊỀẾỆỂỄ]/g, "E");
  result = result.replace(/[ÌÍỊỈĨ]/g, "I");
  result = result.replace(/[ÒÓỌỎÕÔỒỐỘỔỖƠỜỚỢỞỠ]/g, "O");
  result = result.replace(/[ÙÚỤỦŨƯỪỨỰỬỮ]/g, "U");
  result = result.replace(/[ỲÝỴỶỸ]/g, "Y");
  result = result.replace(/[Đ]/g, "D");
  
  // Decompose any leftover accents (combining diacritics) and remove them
  result = result.normalize("NFD").replace(/[\u0300-\u036f]/g, "");
  
  // Keep only printable ASCII characters 32-126
  let cleanStr = "";
  for (let i = 0; i < result.length; i++) {
    const code = result.charCodeAt(i);
    if (code >= 32 && code <= 126) {
      cleanStr += result[i];
    }
  }
  return cleanStr;
}

// Names shown on the device must be plain ASCII (the firmware font has no
// accents) and short enough to fit the ring screen. Strip tones, drop any
// remaining non-printable chars, collapse whitespace, cap length.
function asciiName(str) {
  return removeVietnameseTones(str || '')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, 18)
}

const STORAGE_KEY = 'mochi-hotspots-calib'

const DEFAULT_HOTSPOTS = {
  painting: {
    x: 0.2672,
    y: 0.2633,
    r: 0.0849,
    mode: 'animation',
    cmd: '1',
    label: 'Painting (Expression Mode)',
  },
  lock: {
    x: 0.2783,
    y: 0.4142,
    r: 0.0314,
    mode: 'clock',
    cmd: '2',
    label: 'Shelf Lock (Clock Mode)',
  },
  fan: {
    x: 0.9441,
    y: 0.2781,
    r: 0.0443,
    mode: 'pomodoro',
    cmd: '3', // Sends 3 to switch to Pomodoro mode
    label: 'Spinning Fan (Pomodoro Mode)',
  },
  screen: {
    x: 0.5443,
    y: 0.507,
    r: 0.0665,
    mode: 'terminal',
    cmd: '4',
    label: 'Computer Screen (Terminal Mode)',
  },
  cat: {
    x: 0.7667,
    y: 0.18,
    r: 0.0423,
    mode: 'usage',
    cmd: '5',
    label: 'Sleeping Cat (Claude Usage)',
  },
  window: {
    x: 0.4325,
    y: 0.2514,
    r: 0.09,
    mode: 'weather',
    cmd: '6',
    label: 'Window (Weather Mode)',
  },
  sing: {
    x: 0.8137,
    y: 0.394,
    r: 0.045,
    mode: 'sing',
    cmd: '7',
    label: 'Music Box (Sing Mode)',
  },
}

export default function App() {
  const [isConnected, setIsConnected] = useState(false)
  const [connectionType, setConnectionType] = useState('serial') // 'serial' | 'bluetooth' | 'wifi'
  const [mqttKey, setMqttKey] = useState(() => {
    if (typeof localStorage !== 'undefined') {
      return localStorage.getItem('mochi-mqtt-key') || 'mochi_baotrq_relay';
    }
    return 'mochi_baotrq_relay';
  })

  const mqttTopicSub = `baotrq/clawd/control/${mqttKey}`;
  const mqttTopicPub = `baotrq/clawd/status/${mqttKey}`;

  const writeSerial = async (data) => {
    if (connectionType === 'bluetooth') {
      return await writeBluetooth(data)
    } else if (connectionType === 'wifi') {
      return await publishMQTT(mqttTopicSub, data)
    } else {
      return await rawWriteSerial(data)
    }
  }



  const activeModeRef = useRef(null)
  const [activeMode, setActiveMode] = useState(null) // 'animation' | 'clock' | 'terminal' | null
  const [activeAlarm, setActiveAlarm] = useState(null) // { durationMins, endTime }
  const [activeTimer, setActiveTimer] = useState(null) // { durationSecs, endTime }
  const [isPomoActive, setIsPomoActive] = useState(false)
  const [idleIntervalSec, setIdleIntervalSec] = useState(8)
  const [showConfigMenu, setShowConfigMenu] = useState(false)
  const [calibrateMode, setCalibrateMode] = useState(() => {
    if (typeof window !== 'undefined') {
      return new URLSearchParams(window.location.search).has('calibrate')
    }
    return false
  })

  const STATUSES = [
    'clauding...',
    'hallucinating...',
    'pondering...',
    'thinking with high effort...',
    'cooking prompt...',
    'analyzing codebase...',
    'refactoring...',
    'overclocking...',
    'simulating mochi...',
    'loading weights...',
    'constituting rules...'
  ]

  const STREAMS = [
    { name: 'Lofi Cafe', url: 'https://radio.loficafe.net/listen/chilling/radio.mp3' },
    { name: 'Hotmix Lofi', url: 'https://streaming.hotmixradio.com/hotmix-lofi-en-mp3' },
    { name: 'Chillhop Radio', url: 'https://streams.fluxfm.de/Chillhop/mp3-128/' }
  ]

  const [currentTime, setCurrentTime] = useState(new Date())
  const [statusIdx, setStatusIdx] = useState(0)
  const [isPlaying, setIsPlaying] = useState(false)
  const [volume, setVolume] = useState(0.5)
  const [streamIdx, setStreamIdx] = useState(0)
  const [isMuted, setIsMuted] = useState(false)
  const [settingsTab, setSettingsTab] = useState('sys')
  const [serialLog, setSerialLog] = useState([]) // [{ ts, text }] raw device Serial output, capped
  const serialLogViewRef = useRef(null)
  const audioRef = useRef(null)

  // Manage Audio element lifecycle
  useEffect(() => {
    if (!audioRef.current) {
      audioRef.current = new Audio(STREAMS[streamIdx].url)
    } else {
      audioRef.current.src = STREAMS[streamIdx].url
    }
    audioRef.current.volume = isMuted ? 0 : volume
    audioRef.current.preload = 'none'
    
    if (isPlaying) {
      audioRef.current.play().catch(err => {
        console.error('Audio play failed:', err)
        setIsPlaying(false)
      })
    } else {
      audioRef.current.pause()
    }
  }, [streamIdx])

  useEffect(() => {
    if (audioRef.current) {
      if (isPlaying) {
        audioRef.current.play().catch(err => {
          console.error('Audio play failed:', err)
          setIsPlaying(false)
        })
      } else {
        audioRef.current.pause()
      }
    }
  }, [isPlaying])

  useEffect(() => {
    if (audioRef.current) {
      audioRef.current.volume = isMuted ? 0 : volume
    }
  }, [volume, isMuted])

  useEffect(() => {
    return () => {
      if (audioRef.current) {
        audioRef.current.pause()
        audioRef.current = null
      }
    }
  }, [])

  // Update clock every second
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date())
    }, 1000)
    return () => clearInterval(timer)
  }, [])

  // Cycle statuses
  useEffect(() => {
    const statusTimer = setInterval(() => {
      setStatusIdx((prev) => (prev + 1) % STATUSES.length)
    }, 6000)
    return () => clearInterval(statusTimer)
  }, [])

  // Cycle music tracks
  useEffect(() => {
    const trackTimer = setInterval(() => {
      setTrackIdx((prev) => (prev + 1) % TRACKS.length)
    }, 15000)
    return () => clearInterval(trackTimer)
  }, [])

  // Claude Pro usage polling (5h session % / 7d weekly %)
  const [usage, setUsage] = useState(null) // { sessionPct, weeklyPct, sessionResetAt, weeklyResetAt } | null
  const [weatherData, setWeatherData] = useState(null) // { temp, feels, humidity, condition }

  const [selectedLocation, setSelectedLocation] = useState(() => {
    const saved = localStorage.getItem('mochi-weather-location')
    if (saved) {
      try {
        return JSON.parse(saved)
      } catch (e) {}
    }
    return { name: 'Ho Chi Minh City', shortName: 'Ho Chi Minh City', lat: 10.823, lon: 106.630, timezone: 'Asia/Ho_Chi_Minh', country: 'Vietnam' }
  })

  const [recentLocations, setRecentLocations] = useState(() => {
    const saved = localStorage.getItem('mochi-recent-locations')
    if (saved) {
      try {
        return JSON.parse(saved)
      } catch (e) {}
    }
    return [
      { name: 'Ho Chi Minh City', shortName: 'Ho Chi Minh City', lat: 10.823, lon: 106.630, timezone: 'Asia/Ho_Chi_Minh', country: 'Vietnam' }
    ]
  })

  // Try the relative /api/usage first (works via the Vite dev-server
  // middleware when running `npm run dev` locally). That route doesn't
  // exist on static hosting (e.g. GitHub Pages), so fall back to the local
  // usage-bridge script (web/scripts/usage-bridge.mjs) on localhost — it
  // reads the same local credentials and serves the same shape with CORS
  // enabled for the hosted origin. If neither is reachable, usage is null.
  const fetchUsageFrom = async (url) => {
    const res = await fetch(url)
    const data = await res.json()
    if (!res.ok || data.error) throw new Error(data?.error || `HTTP ${res.status}`)
    return data
  }

  const pollUsage = async () => {
    for (const url of ['/api/usage', 'http://localhost:8787/api/usage']) {
      try {
        const data = await fetchUsageFrom(url)
        setUsage(data)
        return data
      } catch {
        // try the next source
      }
    }
    setUsage(null)
    return null
  }

  useEffect(() => {
    let cancelled = false
    const tick = async () => {
      if (!cancelled) await pollUsage()
    }
    tick()
    const usageTimer = setInterval(tick, 60000)
    return () => {
      cancelled = true
      clearInterval(usageTimer)
    }
  }, [])

  // Push usage to the device whenever it changes (not on every poll tick).
  // The board has no RTC, so it can't compute "resets in Xh Ym" itself —
  // we send minutes-remaining once and it counts down locally via millis().
  // True while showing a Test Weather preview — suppresses all real API
  // fetches so the test data on the device isn't overwritten. Cleared
  // automatically when weather mode is left.
  const [weatherTest, setWeatherTest] = useState(false)
  const weatherTestRef = useRef(weatherTest)
  useEffect(() => {
    weatherTestRef.current = weatherTest
  }, [weatherTest])
  const lastSentUsageRef = useRef(null)
  useEffect(() => {
    if (!usage || !isConnected) return
    const key = `${usage.sessionPct}-${usage.weeklyPct}`
    if (lastSentUsageRef.current === key) return
    lastSentUsageRef.current = key
    const s = String(usage.sessionPct).padStart(2, '0')
    const w = String(usage.weeklyPct).padStart(2, '0')
    const sessionMin = usage.sessionResetAt
      ? Math.max(0, Math.round((usage.sessionResetAt - Date.now()) / 60000))
      : 0
    const weeklyMin = usage.weeklyResetAt
      ? Math.max(0, Math.round((usage.weeklyResetAt - Date.now()) / 60000))
      : 0
    const r = String(Math.min(999, sessionMin)).padStart(3, '0')
    const t = String(Math.min(99999, weeklyMin)).padStart(5, '0')
    writeSerial(`U${s}${w}${r}${t}\n`)
  }, [usage, isConnected])

  const [alarms, setAlarms] = useState(() => {
    if (typeof localStorage !== 'undefined') {
      try {
        const saved = localStorage.getItem('mochi-alarms')
        return saved ? JSON.parse(saved) : []
      } catch {
        return []
      }
    }
    return []
  })

  useEffect(() => {
    localStorage.setItem('mochi-alarms', JSON.stringify(alarms))
  }, [alarms])

  const syncAlarmsToDevice = async (alarmsList) => {
    if (!isConnected || !alarmsList || !Array.isArray(alarmsList)) return;
    try {
      await writeSerial('A C\n');
      for (let i = 0; i < Math.min(10, alarmsList.length); i++) {
        const alarm = alarmsList[i];
        if (!alarm || !alarm.time || typeof alarm.time !== 'string') continue;
        const enabledVal = alarm.enabled ? 1 : 0;
        const parts = alarm.time.split(':');
        if (parts.length < 2) continue;
        const hh = parseInt(parts[0], 10);
        const mm = parseInt(parts[1], 10);
        if (isNaN(hh) || isNaN(mm)) continue;
        
        let daysByte = 0;
        const days = alarm.days && alarm.days.length > 0 ? alarm.days : [0, 1, 2, 3, 4, 5, 6];
        days.forEach(d => {
          daysByte |= (1 << d);
        });
        
        const cleanName = removeVietnameseTones(alarm.name || '').substring(0, 20);
        const cmd = `A S ${i} ${enabledVal} ${hh} ${mm} ${daysByte}${cleanName ? ' ' + cleanName : ''}\n`;
        await writeSerial(cmd);
      }
    } catch (e) {
      console.error('Failed to sync alarms to device:', e);
    }
  }

  useEffect(() => {
    if (isConnected) {
      syncAlarmsToDevice(alarms);
    }
  }, [alarms, isConnected]);

  const getClosestAlarm = (alarmsList) => {
    if (!alarmsList || !Array.isArray(alarmsList)) return null
    const enabledAlarms = alarmsList.filter(a => a && a.enabled)
    if (enabledAlarms.length === 0) return null

    const now = new Date()
    let closestAlarm = null
    let minDiff = Infinity

    for (const alarm of enabledAlarms) {
      if (!alarm || !alarm.time || typeof alarm.time !== 'string') continue
      const [hh, mm] = alarm.time.split(':').map(Number)
      const days = alarm.days && alarm.days.length > 0 ? alarm.days : [0, 1, 2, 3, 4, 5, 6]
      
      let target = new Date(now)
      target.setHours(hh, mm, 0, 0)
      
      let daysChecked = 0
      if (target.getTime() <= now.getTime()) {
        target.setDate(target.getDate() + 1)
        daysChecked = 1
      }
      
      while (!days.includes(target.getDay()) && daysChecked < 8) {
        target.setDate(target.getDate() + 1)
        daysChecked++
      }

      if (daysChecked >= 8) continue

      const diff = target.getTime() - now.getTime()
      if (diff < minDiff) {
        minDiff = diff
        closestAlarm = {
          ...alarm,
          endTime: target.getTime(),
          durationMins: Math.round(diff / 60000),
          targetTime: alarm.time,
        }
      }
    }
    return closestAlarm
  }

  // Track the closest upcoming alarm. The web owns alarm timing (it has
  // accurate time; the board has no RTC and drifts), so we DON'T arm a
  // countdown on the device — we just hold the target here and fire a
  // one-shot ring ('R') at the exact moment in the expiry poll below.
  const wasConnectedRef = useRef(false)
  useEffect(() => {
    if (!isConnected) {
      wasConnectedRef.current = false
      return
    }
    wasConnectedRef.current = true
    setActiveAlarm(getClosestAlarm(alarms))
  }, [alarms, isConnected])

  // Incoming serial text arrives as arbitrary chunks, not line-aligned, so
  // buffer until we see a newline before checking for the device's status
  // echoes (e.g. "POMO:ON"/"POMO:OFF") — this is the actual source of truth
  // for Pomodoro state, not a local guess made when we send 'p'.
  const serialLineBufRef = useRef('')
  const SERIAL_LOG_MAX = 300
  const handleSerialData = (chunk) => {
    console.log('[ESP32 Serial]:', chunk)
    serialLineBufRef.current += chunk
    const lines = serialLineBufRef.current.split('\n')
    serialLineBufRef.current = lines.pop() // keep any incomplete trailing line

    const printable = lines.map(l => l.replace(/\r$/, '')).filter(l => l.length > 0)
    if (printable.length) {
      const ts = new Date().toLocaleTimeString([], { hour12: false })
      setSerialLog(prev => {
        const next = [...prev, ...printable.map(text => ({ ts, text }))]
        return next.length > SERIAL_LOG_MAX ? next.slice(next.length - SERIAL_LOG_MAX) : next
      })
    }

    for (const line of lines) {
      const trimmed = line.trim()
      if (trimmed === 'POMO:ON') setIsPomoActive(true)
      else if (trimmed === 'POMO:OFF') setIsPomoActive(false)
      else if (trimmed === 'MODE:ANIMATION') setActiveMode(null)
      else if (trimmed === 'MODE:CLOCK') setActiveMode('clock')
      else if (trimmed === 'MODE:POMODORO') setActiveMode('pomodoro')
      else if (trimmed === 'MODE:TERMINAL') setActiveMode('terminal')
      else if (trimmed === 'MODE:USAGE') setActiveMode('usage')
      else if (trimmed === 'WEATHER') setActiveMode('weather')
      else if (trimmed === 'MODE:SING') setActiveMode('sing')
      else if (trimmed.startsWith('USAGE_DATA ')) {
        const parts = trimmed.substring(11).split(' ')
        if (parts.length >= 4) {
          const sessionPct = parseInt(parts[0], 10)
          const weeklyPct = parseInt(parts[1], 10)
          const sessionMin = parseInt(parts[2], 10)
          const weeklyMin = parseInt(parts[3], 10)
          
          setUsage({
            sessionPct,
            weeklyPct,
            sessionResetAt: sessionMin > 0 ? Date.now() + sessionMin * 60000 : null,
            weeklyResetAt: weeklyMin > 0 ? Date.now() + weeklyMin * 60000 : null,
          })
        }
      }
      else if (trimmed.startsWith('ALARM_DATA ')) {
        const parts = trimmed.substring(11).split(' ')
        if (parts.length >= 5) {
          const idx = parseInt(parts[0], 10)
          const enabled = parseInt(parts[1], 10) === 1
          const hour = parseInt(parts[2], 10)
          const minute = parseInt(parts[3], 10)
          const daysByte = parseInt(parts[4], 10)
          const name = parts.slice(5).join(' ')
          
          const days = []
          for (let d = 0; d < 7; d++) {
            if (daysByte & (1 << d)) {
              days.push(d)
            }
          }
          
          const timeStr = `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`
          const cleanName = name === '-' ? '' : name
          
          setAlarms(prev => {
            const next = Array.isArray(prev) ? [...prev] : []
            while (next.length <= idx) {
              next.push({ id: Math.random().toString(36).substring(2, 9), name: '', time: '00:00', enabled: false, days: [] })
            }
            const nextAlarm = {
              id: next[idx]?.id || Math.random().toString(36).substring(2, 9),
              name: cleanName,
              time: timeStr,
              enabled,
              days,
            }
            if (
              !next[idx] ||
              next[idx].enabled !== nextAlarm.enabled ||
              next[idx].time !== nextAlarm.time ||
              next[idx].name !== nextAlarm.name ||
              JSON.stringify(next[idx].days) !== JSON.stringify(nextAlarm.days)
            ) {
              next[idx] = nextAlarm
              const filtered = next.filter(a => a)
              localStorage.setItem('mochi-alarms', JSON.stringify(filtered))
              return filtered
            }
            return prev
          })
        }
      }
    }
  }

  const formattedTime = currentTime.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit', second: '2-digit', hour12: true })
  const formattedDate = currentTime.toLocaleDateString([], { weekday: 'short', month: 'short', day: 'numeric' })

  // Load custom hotspot coordinates if calibrated
  const [hotspots, setHotspots] = useState(() => {
    if (typeof localStorage !== 'undefined') {
      try {
        const saved = localStorage.getItem(STORAGE_KEY)
        if (saved) {
          const parsed = JSON.parse(saved)
          const merged = { ...DEFAULT_HOTSPOTS }
          for (const key of Object.keys(merged)) {
            if (parsed[key]) {
              merged[key] = {
                ...merged[key],
                x: parsed[key].x ?? merged[key].x,
                y: parsed[key].y ?? merged[key].y,
                r: parsed[key].r ?? merged[key].r,
              }
            }
          }
          return merged
        }
      } catch {
        return DEFAULT_HOTSPOTS
      }
    }
    return DEFAULT_HOTSPOTS
  })

  // Poll alarm and timer expirations
  useEffect(() => {
    const timer = setInterval(() => {
      const now = Date.now()
      if (activeAlarm && now >= activeAlarm.endTime) {
        // Alarms trigger standalone on the ESP32 now.
        // We just recalculate the next occurrence for the browser UI.
        setActiveAlarm(getClosestAlarm(alarms))
      }
      if (activeTimer && now >= activeTimer.endTime) {
        setActiveTimer(null)
      }
    }, 1000)
    return () => clearInterval(timer)
  }, [activeAlarm, activeTimer, alarms])

  // Auto-scroll the Serial log view to the newest line whenever it's open
  useEffect(() => {
    if (settingsTab === 'log' && serialLogViewRef.current) {
      serialLogViewRef.current.scrollTop = serialLogViewRef.current.scrollHeight
    }
  }, [serialLog, settingsTab])

  // Listen to keyboard shortcuts for calibration mode
  useEffect(() => {
    const handleKeyDown = (e) => {
      // Secret key to trigger calibration: Alt + C
      if (e.altKey && e.key.toLowerCase() === 'c') {
        setCalibrateMode((v) => !v)
        e.preventDefault()
      }
    }
    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [])

  // Close config menu when clicking outside
  useEffect(() => {
    if (!showConfigMenu) return
    const handleOutsideClick = (e) => {
      if (!e.target.closest('.config-menu-container')) {
        setShowConfigMenu(false)
      }
    }
    window.addEventListener('click', handleOutsideClick)
    return () => window.removeEventListener('click', handleOutsideClick)
  }, [showConfigMenu])

  const handleConnect = async () => {
    if (isConnected) {
      if (connectionType === 'bluetooth') {
        await disconnectBluetooth()
      } else if (connectionType === 'wifi') {
        await disconnectMQTT()
      } else {
        await disconnectSerial()
      }
      setIsConnected(false)
    } else {
      try {
        if (connectionType === 'bluetooth') {
          await connectBluetooth(
            // On disconnect
            () => setIsConnected(false),
            // On data received from ESP32
            handleSerialData
          )
        } else if (connectionType === 'wifi') {
          await connectMQTT(
            mqttTopicSub,
            mqttTopicPub,
            // On disconnect
            () => setIsConnected(false),
            // On data received from ESP32
            handleSerialData
          )
        } else {
          await connectSerial(
            // On disconnect
            () => setIsConnected(false),
            // On data received from ESP32
            handleSerialData
          )
        }
        setIsConnected(true)

        // Silently sync system time on connect — stays on whatever mode
        // the device is currently showing (default Animation), no panel pop
        const epoch = Math.floor(Date.now() / 1000)
        await writeSerial(`T${epoch}\n`)
        // Query status (alarms, usage, pomo state)
        await writeSerial('Q\n')
      } catch (err) {
        alert(`Failed to connect to ESP32: ${err.message}`)
      }
    }
  }

  const handleWriteSerial = async (data) => {
    // Intercept alarm commands to manage list and sync closest automatically
    if (data.startsWith('r')) {
      const match = data.match(/^r(\d{4})(?: ([^\n]*))?/)
      if (match) {
        const hh = match[1].slice(0, 2)
        const mm = match[1].slice(2, 4)
        const alarmTime = `${hh}:${mm}`
        const name = asciiName(match[2])
        setAlarms(prev => {
          const exists = prev.find(a => a.time === alarmTime)
          if (exists) {
            return prev.map(a => a.time === alarmTime ? { ...a, enabled: true, name: name || a.name } : a)
          } else {
            return [...prev, { time: alarmTime, enabled: true, name, days: [0, 1, 2, 3, 4, 5, 6] }]
          }
        })
      }
      return
    } else if (data === 'a') {
      if (activeAlarm) {
        setAlarms(prev => prev.map(a => a.time === activeAlarm.targetTime ? { ...a, enabled: false } : a))
      }
      setActiveAlarm(null)
      return
    }

    if (isConnected) {
      await writeSerial(data)
    } else {
      console.log('[Mock Write to Serial]:', data)
    }

    // Parse commands to update UI state for Timer/Pomodoro
    if (data.startsWith('y')) {
      const match = data.match(/^y(\d+)(?: ([^\n]*))?/)
      if (match) {
        const secs = parseInt(match[1], 10)
        setActiveTimer({
          durationSecs: secs,
          endTime: Date.now() + secs * 1000,
          name: asciiName(match[2]),
        })
      }
    } else if (data === 'q') {
      setActiveTimer(null)
    } else if (data.startsWith('P')) {
      // 'P...' is the atomic set+start signal — it always starts a session,
      // so flip the UI to "running" immediately rather than waiting on the
      // device's POMO:ON echo. This is a deterministic set (not a blind
      // toggle), so it can't fall out of sync the way the old guess did.
      setIsPomoActive(true)
    } else if (data === 'p') {
      // Plain 'p' is only ever sent from the dashboard to STOP a running
      // session, so optimistically flip to idle.
      setIsPomoActive(false)
    }
    // The device also echoes "POMO:ON"/"POMO:OFF" (handled in
    // handleSerialData) which reconciles these optimistic updates with
    // reality — e.g. after a physical-button toggle or a reconnect.
  }

  // Manual "Test" trigger: fetch usage right now (bypassing the 60s poll),
  // push it to the device's dedicated Usage screen, and open the matching
  // dashboard panel in the web UI.
  const handleTestUsage = async () => {
    lastSentUsageRef.current = null // force a resend even if value is unchanged
    await pollUsage()
    await handleWriteSerial('5')
    setActiveMode('usage')
  }

  const handleUpdateHotspots = (updated) => {
    setHotspots(updated)
    localStorage.setItem(STORAGE_KEY, JSON.stringify(updated))
  }

  // Maps OpenWeather condition ID to a WMO-compatible code for the firmware's wmoToCondition()
  const owIdToWmo = (id) => {
    if (id === 800) return 0           // clear sky
    if (id >= 801 && id <= 804) return 2   // clouds → WC_CLOUDY
    if (id >= 700 && id < 800) {
      if (id === 771 || id === 781) return 771 // windy → WC_WINDY
      return 45   // atmosphere (mist, fog, haze) → WC_FOG
    }
    if (id >= 600 && id < 700) return 71   // snow → WC_SNOWY
    if (id >= 300 && id < 600) return 61   // drizzle, rain → WC_RAIN
    if (id >= 200 && id < 300) return 95   // thunderstorm → WC_STORM
    return 2
  }

  const handleFetchAndPushWeather = async (loc = selectedLocation, switchToWeather = true) => {
    const apiKey = import.meta.env.VITE_OPENWEATHER_API_KEY
    try {
      const latVal = typeof loc.lat === 'number' && !isNaN(loc.lat) ? loc.lat : 0
      const lonVal = typeof loc.lon === 'number' && !isNaN(loc.lon) ? loc.lon : 0
      const r = await fetch(`https://api.openweathermap.org/data/2.5/weather?lat=${latVal}&lon=${lonVal}&units=metric&appid=${apiKey}`)
      const d = await r.json()
      if (weatherTestRef.current) {
        console.log('[Weather] Skipping real weather fetch result because weatherTest is active.')
        return
      }
      if (!d.main) {
        throw new Error('No weather data returned')
      }

      const owId = d.weather?.[0]?.id ?? 800
      const condText = d.weather?.[0]?.description
        ? d.weather[0].description.replace(/\b\w/g, c => c.toUpperCase())
        : 'Cloudy'
      const wmoCode = owIdToWmo(owId)

      setWeatherData({
        temp: Math.round(d.main.temp),
        feels: Math.round(d.main.feels_like),
        humidity: d.main.humidity,
        condition: condText,
        cityName: loc.name
      })

      const cleanLocName = removeVietnameseTones(loc.shortName || loc.name)
      const cleanCondText = removeVietnameseTones(condText)
      const msg = `W${wmoCode},${Math.round(d.main.temp)},${Math.round(d.main.feels_like)},${d.main.humidity},${cleanLocName},${cleanCondText}\n`
      await handleWriteSerial(msg)
      if (switchToWeather) {
        await handleWriteSerial('6')
        setActiveMode('weather')
      }
    } catch (err) {
      console.error('Failed to fetch/push weather:', err)
      if (switchToWeather) {
        await handleWriteSerial('6')
        setActiveMode('weather')
      }
    }
  }

  const handleSearchLocations = async (query) => {
    if (!query || query.trim().length < 2) return []
    const apiKey = import.meta.env.VITE_OPENWEATHER_API_KEY
    try {
      const r = await fetch(`https://api.openweathermap.org/geo/1.0/direct?q=${encodeURIComponent(query)}&limit=5&appid=${apiKey}`)
      const d = await r.json()
      if (Array.isArray(d) && d.length > 0) {
        return d.map(item => {
          const cleanName = removeVietnameseTones(item.name || '')
          const cleanCountry = removeVietnameseTones(item.country || '')
          const cleanAdmin1 = removeVietnameseTones(item.state || '')

          return {
            name: cleanName,
            shortName: cleanName.substring(0, 20),
            lat: item.lat,
            lon: item.lon,
            timezone: 'auto',
            country: cleanCountry,
            admin1: cleanAdmin1
          }
        })
      }
    } catch (err) {
      console.error('Geocoding search failed:', err)
    }
    return []
  }

  // Clear test-weather mode whenever we leave the weather screen, so
  // re-entering weather later pulls real data again.
  useEffect(() => {
    if (activeMode !== 'weather') setWeatherTest(false)
  }, [activeMode])

  // Auto-refresh weather every 2 minutes while in weather mode.
  // While previewing a test condition, skip the real API entirely so the
  // test data stays on the device until the user leaves weather mode.
  useEffect(() => {
    if (activeMode !== 'weather' || weatherTest) return
    handleFetchAndPushWeather()
    const id = setInterval(() => handleFetchAndPushWeather(), 2 * 60 * 1000)
    return () => clearInterval(id)
  }, [activeMode, weatherTest]) // eslint-disable-line react-hooks/exhaustive-deps

  const handleLocationChange = (loc) => {
    setSelectedLocation(loc)
    localStorage.setItem('mochi-weather-location', JSON.stringify(loc))
    
    // Add to recent locations list (avoid duplicates, max 5 items)
    setRecentLocations(prev => {
      const filtered = prev.filter(item => 
        !(item.lat === loc.lat && item.lon === loc.lon)
      )
      const updated = [loc, ...filtered].slice(0, 5)
      localStorage.setItem('mochi-recent-locations', JSON.stringify(updated))
      return updated
    })

    // Push updated data to device without switching to weather mode
    setWeatherTest(false)
    handleFetchAndPushWeather(loc, false)
  }

  const handleDeleteRecentLocation = (locToDelete) => {
    setRecentLocations(prev => {
      const updated = prev.filter(item => 
        !(item.lat === locToDelete.lat && item.lon === locToDelete.lon)
      )
      localStorage.setItem('mochi-recent-locations', JSON.stringify(updated))
      return updated
    })
  }

  const handleTriggerMode = (key, mode, cmd) => {
    if (mode === 'animation') {
      handleWriteSerial('1')
      setActiveMode('animation')
    } else if (mode === 'clock') {
      // Auto-sync system time on Clock Mode switch. Seconds-precise (silent 'T',
      // not minute-only 't') so the device clock matches real wall-clock — a
      // minute-only sync truncates to :00 and runs up to ~59s slow, which makes
      // the displayed minute lag behind a web-timed alarm ring.
      const epoch = Math.floor(Date.now() / 1000)
      handleWriteSerial(`2T${epoch}\n`)
      setActiveMode('clock')
    } else if (mode === 'usage') {
      // Refresh usage immediately rather than waiting on the 60s poll
      handleTestUsage()
    } else if (mode === 'weather') {
      setWeatherTest(false)
      handleFetchAndPushWeather()
    } else {
      handleWriteSerial(cmd)
      setActiveMode(mode)
    }
  }

  const handleResetHotspots = () => {
    if (window.confirm('Reset hotspots to default positions?')) {
      setHotspots(DEFAULT_HOTSPOTS)
      localStorage.removeItem(STORAGE_KEY)
    }
  }

  return (
    <main className="relative h-dvh w-screen overflow-hidden bg-room-bg-deep select-none">
      <RoomScene />
      <Clawd />

      {/* Top right status bar (subtle status panel) */}
      <div className="absolute top-4 right-4 z-20 flex gap-2 items-center pointer-events-auto select-none">
        <div className="bg-room-bg/90 border border-ascii-mid/20 rounded-xl px-3 py-1.5 font-mono text-[10px] text-ascii-bright backdrop-blur shadow-xl flex items-center gap-3">
          {/* Music Widget */}
          <div className="flex items-center gap-2 border-r border-ascii-mid/20 pr-3 select-none text-ascii-mid">
            {/* Play/Pause Button */}
            <button
              onClick={() => setIsPlaying(!isPlaying)}
              className="text-[#e05f3e] hover:text-[#f27452] text-xs font-bold transition-all cursor-pointer w-4 h-4 flex items-center justify-center border border-ascii-mid/20 rounded bg-ascii-bright/5 hover:bg-ascii-bright/10 active:scale-95"
              title={isPlaying ? 'Pause' : 'Play Lofi Radio'}
            >
              {isPlaying ? '⏸' : '▶'}
            </button>

            {/* Clickable Marquee for cycling streams */}
            <button
              onClick={() => setStreamIdx(prev => (prev + 1) % STREAMS.length)}
              className="w-[100px] overflow-hidden whitespace-nowrap relative flex items-center text-left cursor-pointer hover:opacity-80 active:scale-95 font-mono text-[10px]"
              title="Click to cycle lofi streams"
            >
              <div className={`inline-block ${isPlaying ? 'animate-marquee' : ''}`}>
                <span className="text-[#e05f3e] mr-1.5 font-bold animate-pulse">♫</span>
                <span className="text-ascii-spark mr-8">{STREAMS[streamIdx].name}</span>
                <span className="text-[#e05f3e] mr-1.5 font-bold animate-pulse">♫</span>
                <span className="text-ascii-spark mr-8">{STREAMS[streamIdx].name}</span>
              </div>
            </button>
          </div>

          {/* Status Prompt */}
          <div className="flex items-center gap-1.5 border-r border-ascii-mid/20 pr-3">
            <span className="text-[#e05f3e] animate-pulse">●</span>
            <span className="text-ascii-bright capitalize">{STATUSES[statusIdx]}</span>
          </div>

          {/* Claude Usage */}
          <div className="flex items-center gap-1.5 border-r border-ascii-mid/20 pr-3 select-none">
            <span className="text-ascii-mid">5h</span>
            <span className={usage ? 'text-ascii-bright' : 'text-ascii-dim'}>
              {usage ? `${usage.sessionPct}%` : '—'}
            </span>
            <span className="text-ascii-mid">· 7d</span>
            <span className={usage ? 'text-ascii-bright' : 'text-ascii-dim'}>
              {usage ? `${usage.weeklyPct}%` : '—'}
            </span>
            <button
              onClick={handleTestUsage}
              className="text-ascii-mid hover:text-ascii-spark cursor-pointer transition-colors"
              title="Test: refresh usage now, push to device, and open the dashboard"
            >
              ↻
            </button>
          </div>

          {/* Date */}
          <div className="flex items-center gap-1 border-r border-ascii-mid/20 pr-3 select-none">
            <span className="text-ascii-bright">{formattedDate}</span>
          </div>

          {/* Time */}
          <div className="flex items-center gap-1 select-none">
            <span className="text-ascii-bright">{formattedTime}</span>
          </div>
        </div>
      </div>

      {/* Responsive interactive hotspots layer */}
      <HotspotOverlay
        calibrateMode={calibrateMode}
        hotspots={hotspots}
        onUpdateHotspots={handleUpdateHotspots}
        onTriggerMode={handleTriggerMode}
      />

      {/* Floating control panels */}
      <ControlPanel
        activeMode={activeMode}
        onClose={() => setActiveMode(null)}
        onWriteSerial={handleWriteSerial}
        onRefreshWeather={() => {
          setWeatherTest(false)
          handleFetchAndPushWeather(selectedLocation)
        }}
        isConnected={isConnected}
        activeAlarm={activeAlarm}
        activeTimer={activeTimer}
        isPomoActive={isPomoActive}
        usage={usage}
        weatherData={weatherData}
        alarms={alarms}
        setAlarms={setAlarms}
        selectedLocation={selectedLocation}
        recentLocations={recentLocations}
        onLocationChange={handleLocationChange}
        onSearchLocations={handleSearchLocations}
        onDeleteRecentLocation={handleDeleteRecentLocation}
      />

      {/* Bottom control chrome bar (Warm, low-contrast design) */}
      <div className="absolute bottom-4 left-5 right-5 z-20 flex justify-between items-center pointer-events-none">
        {/* Left side: branding and Serial status */}
        <div className="flex items-center gap-4 pointer-events-auto">
          <span className="text-xs tracking-widest text-ascii-dim font-bold select-none">
            clawd mochi
          </span>

          {/* Connection Type Toggle */}
          {!isConnected && (
            <div className="flex gap-1 bg-ascii-dim/10 border border-ascii-mid/20 rounded p-0.5 select-none font-mono text-[9px]">
              <button
                onClick={() => setConnectionType('serial')}
                className={`px-1.5 py-0.5 rounded cursor-pointer transition-colors ${
                  connectionType === 'serial'
                    ? 'bg-[#e05f3e] text-white font-bold'
                    : 'text-ascii-mid hover:text-ascii-bright'
                }`}
                title="Use USB Serial Connection"
              >
                USB
              </button>
              <button
                onClick={() => setConnectionType('bluetooth')}
                className={`px-1.5 py-0.5 rounded cursor-pointer transition-colors ${
                  connectionType === 'bluetooth'
                    ? 'bg-[#e05f3e] text-white font-bold'
                    : 'text-ascii-mid hover:text-ascii-bright'
                }`}
                title="Use Bluetooth Wireless Connection"
              >
                BLE
              </button>
              <button
                onClick={() => setConnectionType('wifi')}
                className={`px-1.5 py-0.5 rounded cursor-pointer transition-colors ${
                  connectionType === 'wifi'
                    ? 'bg-[#e05f3e] text-white font-bold'
                    : 'text-ascii-mid hover:text-ascii-bright'
                }`}
                title="Use WiFi Cloud Relay Connection"
              >
                WIFI
              </button>
            </div>
          )}

          {(connectionType === 'wifi' || (connectionType === 'serial' && isSerialSupported()) || (connectionType === 'bluetooth' && isBluetoothSupported())) ? (
            <button
              onClick={handleConnect}
              className={`flex items-center gap-2 px-2.5 py-1 rounded text-[10px] font-mono tracking-wider transition-all duration-300 border cursor-pointer ${
                isConnected
                  ? 'bg-orange-950/10 border-orange-900/30 text-orange-400/80 hover:bg-orange-950/20 hover:text-orange-400'
                  : 'bg-ascii-dim/15 border-ascii-mid/20 text-ascii-mid hover:text-ascii-spark hover:border-ascii-bright/40'
              }`}
            >
              <span className="text-[12px]">{connectionType === 'wifi' ? '☁️' : (connectionType === 'bluetooth' ? '📡' : '🔌')}</span>
              <span>{isConnected ? `${connectionType.toUpperCase()}: CONNECTED` : `CONNECT ${connectionType.toUpperCase()}`}</span>
            </button>
          ) : (
            <span className="text-[9px] text-red-400 bg-red-950/10 border border-red-900/20 px-2 py-0.5 rounded leading-none">
              {connectionType === 'serial' ? 'Web Serial Unsupported' : 'Web Bluetooth Unsupported'}
            </span>
          )}
        </div>

        {/* Right side: configuration control popover */}
        <div className="flex items-center gap-2 pointer-events-auto relative config-menu-container">
          {showConfigMenu && (
            <div className="absolute bottom-10 right-0 z-30 w-64 bg-room-bg/95 border border-ascii-mid/20 rounded-lg p-3 font-mono text-[10px] text-ascii-bright/80 backdrop-blur shadow-xl flex flex-col gap-1.5 max-h-[350px] overflow-y-auto">
              {/* Tab Bar */}
              <div className="flex border-b border-ascii-mid/20 text-[9px] mb-2 font-bold uppercase select-none">
                {[
                  { id: 'sys', label: '⚙️ Sys' },
                  { id: 'audio', label: '🔊 Audio' },
                  { id: 'log', label: '📜 Log' },
                  { id: 'test', label: '🧪 Test' }
                ].map(tab => (
                  <button
                    key={tab.id}
                    onClick={() => setSettingsTab(tab.id)}
                    className={`flex-1 text-center py-1 cursor-pointer transition-all border-b-2 ${
                      settingsTab === tab.id
                        ? 'border-[#e05f3e] text-[#f0b89a] font-bold'
                        : 'border-transparent text-ascii-mid hover:text-ascii-bright'
                    }`}
                  >
                    {tab.label}
                  </button>
                ))}
              </div>

              {/* Tab Content */}
              {settingsTab === 'sys' && (
                <div className="flex flex-col gap-1.5">
                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pb-1">Mascots & Hotspots</div>
                  
                  <button
                    onClick={() => {
                      setCalibrateMode(!calibrateMode)
                      setShowConfigMenu(false)
                    }}
                    className={`w-full text-left px-2 py-1.5 rounded transition-all cursor-pointer flex items-center justify-between border ${
                      calibrateMode
                        ? 'bg-orange-950/10 border-orange-900/40 text-orange-400 font-bold'
                        : 'bg-ascii-bright/5 border-ascii-mid/15 hover:border-ascii-spark hover:text-ascii-spark text-ascii-bright/80'
                    }`}
                  >
                    <span>🎯 Calibrate</span>
                    {calibrateMode && <span>✓</span>}
                  </button>

                  {calibrateMode && (
                    <button
                      onClick={() => {
                        handleResetHotspots()
                        setShowConfigMenu(false)
                      }}
                      className="w-full text-left px-2 py-1.5 rounded text-red-400 bg-red-950/10 border border-red-900/35 hover:bg-red-950/25 transition-all cursor-pointer"
                    >
                      🗑️ Reset Hotspots
                    </button>
                  )}

                  <button
                    onClick={() => {
                      const url = new URL(window.location.href)
                      url.searchParams.set('tune', '1')
                      window.location.href = url.toString()
                    }}
                    className="w-full text-left px-2 py-1.5 rounded bg-ascii-bright/5 border border-ascii-mid/15 hover:border-ascii-spark hover:text-ascii-spark text-ascii-bright/80 transition-all cursor-pointer"
                  >
                    🎨 Tune Mascot
                  </button>

                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Display settings</div>
                  
                  <button
                    onClick={() => handleWriteSerial('b')}
                    className="w-full text-left px-2 py-1.5 rounded bg-ascii-bright/5 border border-ascii-mid/15 hover:border-ascii-spark hover:text-ascii-spark text-ascii-bright/80 transition-all cursor-pointer"
                  >
                    💡 Toggle Backlight
                  </button>

                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Idle Face Switch (seconds)</div>
                  <div className="grid grid-cols-3 gap-1 px-1 mb-2">
                    {[3, 5, 8, 15, 30, 60].map(s => (
                      <button
                        key={s}
                        onClick={() => { handleWriteSerial(`I${s}\n`); setIdleIntervalSec(s) }}
                        className={`py-1 rounded text-[9px] font-mono font-bold cursor-pointer transition-all border ${
                          idleIntervalSec === s
                            ? 'bg-[#d97756]/20 border-[#d97756]/60 text-[#f0b89a]'
                            : 'bg-ascii-dim/10 border-ascii-mid/20 text-ascii-mid hover:border-ascii-mid/40 hover:text-ascii-bright'
                        }`}
                      >
                        {s}s
                      </button>
                    ))}
                  </div>

                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">MQTT Cloud Relay Key</div>
                  <div className="px-1 flex flex-col gap-1">
                    <input
                      type="text"
                      value={mqttKey}
                      onChange={(e) => {
                        const val = e.target.value;
                        setMqttKey(val);
                        localStorage.setItem('mochi-mqtt-key', val);
                      }}
                      disabled={isConnected}
                      className="w-full bg-ascii-dim/10 border border-ascii-mid/20 rounded px-2 py-1 text-ascii-bright font-mono text-[9px] focus:outline-none focus:border-[#e05f3e] disabled:opacity-50"
                      placeholder="e.g. mochi_baotrq_relay"
                    />
                    <div className="text-[8px] text-ascii-dim leading-normal mt-0.5">
                      Must match the suffix inside your ESP32's `secrets.h`.
                    </div>
                  </div>
                </div>
              )}

              {settingsTab === 'audio' && (
                <div className="flex flex-col gap-1.5">
                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pb-1">Lofi Volume</div>
                  <div className="flex flex-col gap-3 p-2 bg-ascii-bright/5 rounded border border-ascii-mid/10">
                    <div className="flex items-center justify-between">
                      <span className="text-ascii-mid text-[9px]">Mute Audio</span>
                      <button
                        onClick={() => setIsMuted(!isMuted)}
                        className={`px-2 py-0.5 rounded text-[9px] font-mono cursor-pointer transition-all border ${
                          isMuted
                            ? 'bg-red-950/20 border-red-900/30 text-red-400 font-bold'
                            : 'bg-ascii-dim/10 border-ascii-mid/20 text-ascii-mid hover:border-ascii-mid/40 hover:text-ascii-bright'
                        }`}
                      >
                        {isMuted ? 'Muted' : 'Active'}
                      </button>
                    </div>
                    
                    <div className="flex flex-col gap-1.5">
                      <div className="flex justify-between text-ascii-mid text-[9px]">
                        <span>Volume Level</span>
                        <span className="text-ascii-spark font-bold font-mono">{Math.round((isMuted ? 0 : volume) * 100)}%</span>
                      </div>
                      <input
                        type="range"
                        min={0}
                        max={1}
                        step={0.05}
                        value={isMuted ? 0 : volume}
                        onChange={(e) => {
                          setVolume(parseFloat(e.target.value))
                          setIsMuted(false)
                        }}
                        className="w-full h-1.5 bg-ascii-mid/20 rounded-lg appearance-none cursor-pointer accent-[#e05f3e] outline-none"
                      />
                    </div>
                  </div>
                </div>
              )}

              {settingsTab === 'log' && (
                <div className="flex flex-col gap-1.5">
                  <div className="flex items-center justify-between px-1 border-b border-ascii-mid/10 pb-1">
                    <div className="text-[9px] text-ascii-dim uppercase font-bold select-none">ESP32 Serial Log</div>
                    <button
                      onClick={() => setSerialLog([])}
                      className="text-[9px] text-ascii-mid hover:text-red-400 transition-all cursor-pointer"
                    >
                      🗑️ Clear
                    </button>
                  </div>
                  <div
                    ref={serialLogViewRef}
                    className="h-[190px] overflow-y-auto bg-black/40 border border-ascii-mid/15 rounded p-1.5 font-mono text-[9px] leading-tight whitespace-pre-wrap break-all"
                  >
                    {serialLog.length === 0 ? (
                      <div className="text-ascii-mid italic">
                        {isConnected ? 'Waiting for device output…' : 'Not connected — connect over Serial to see boot/WiFi/NTP output here.'}
                      </div>
                    ) : (
                      serialLog.map((entry, i) => (
                        <div key={i} className="text-ascii-bright/70">
                          <span className="text-ascii-mid">{entry.ts}</span> {entry.text}
                        </div>
                      ))
                    )}
                  </div>
                  <div className="text-[8px] text-ascii-dim px-1 select-none">
                    Look for "WiFi: time synced via NTP. Got internet." to confirm.
                  </div>
                </div>
              )}

              {settingsTab === 'test' && (
                <div className="flex flex-col gap-1.5">
                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pb-1">Firmware Screens</div>
                  <div className="grid grid-cols-3 gap-1">
                    {[
                      { label: 'Expr', cmd: '1', mode: 'animation' },
                      { label: 'Clock', cmd: '2', mode: 'clock' },
                      { label: 'Pomo', cmd: '3', mode: 'pomodoro' },
                      { label: 'Term', cmd: '4', mode: 'terminal' },
                      { label: 'Usage', cmd: '5', mode: 'usage' },
                      { label: 'Wx', cmd: '6', mode: 'weather' },
                      { label: 'Sing', cmd: '7', mode: 'sing' }
                    ].map((item) => (
                      <button
                        key={item.cmd}
                        onClick={async () => {
                          if (item.mode === 'clock') {
                            const now = new Date()
                            const hh = String(now.getHours()).padStart(2, '0')
                            const mm = String(now.getMinutes()).padStart(2, '0')
                            await handleWriteSerial(`2t${hh}${mm}\n`)
                            setActiveMode(item.mode)
                          } else if (item.mode === 'usage') {
                            handleTestUsage()
                          } else if (item.mode === 'weather') {
                            setWeatherTest(false)
                            await handleFetchAndPushWeather()
                          } else {
                            await handleWriteSerial(item.cmd)
                            setActiveMode(item.mode)
                          }
                        }}
                        className={`px-1 py-1 rounded border text-center transition-all cursor-pointer text-[9px] ${
                          activeMode === item.mode
                            ? 'bg-ascii-spark/10 border-ascii-spark text-ascii-spark font-bold'
                            : 'bg-ascii-bright/5 border-ascii-mid/10 hover:border-ascii-spark hover:text-ascii-spark'
                        }`}
                      >
                        {item.label}
                      </button>
                    ))}
                  </div>

                  <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Mock Weather</div>
                  <div className="flex flex-col gap-1 max-h-[120px] overflow-y-auto pr-1">
                    {[
                      { label: '☀️ Clear', cmd: 'W0,30,32,80,Ho Chi Minh City,Clear Sky\n', data: { temp: 30, feels: 32, humidity: 80, condition: 'Clear Sky', cityName: 'Ho Chi Minh City' } },
                      { label: '☁️ Cloudy', cmd: 'W3,25,27,85,Ho Chi Minh City,Partly Cloudy\n', data: { temp: 25, feels: 27, humidity: 85, condition: 'Partly Cloudy', cityName: 'Ho Chi Minh City' } },
                      { label: '🌫️ Fog', cmd: 'W45,20,20,95,Ho Chi Minh City,Foggy\n', data: { temp: 20, feels: 20, humidity: 95, condition: 'Foggy', cityName: 'Ho Chi Minh City' } },
                      { label: '🌧️ Rain', cmd: 'W61,22,22,90,Ho Chi Minh City,Light Drizzle\n', data: { temp: 22, feels: 22, humidity: 90, condition: 'Light Drizzle', cityName: 'Ho Chi Minh City' } },
                      { label: '⛈️ Storm', cmd: 'W95,18,18,98,Ho Chi Minh City,Thunderstorm\n', data: { temp: 18, feels: 18, humidity: 98, condition: 'Thunderstorm', cityName: 'Ho Chi Minh City' } },
                      { label: '❄️ Snowy', cmd: 'W71,0,-2,90,Ho Chi Minh City,Light Snow\n', data: { temp: 0, feels: -2, humidity: 90, condition: 'Light Snow', cityName: 'Ho Chi Minh City' } },
                      { label: '💨 Windy', cmd: 'W771,24,22,70,Ho Chi Minh City,Blowing Wind\n', data: { temp: 24, feels: 22, humidity: 70, condition: 'Blowing Wind', cityName: 'Ho Chi Minh City' } }
                    ].map((item, idx) => (
                      <button
                        key={idx}
                        onClick={async () => {
                          setWeatherTest(true)
                          setWeatherData(item.data)
                          await handleWriteSerial(item.cmd)
                          await handleWriteSerial('6')
                          setActiveMode('weather')
                        }}
                        className="w-full text-left px-2 py-1 rounded hover:bg-ascii-bright/5 hover:text-ascii-spark text-ascii-bright/70 transition-all cursor-pointer text-[9px] flex items-center justify-between"
                      >
                        <span>{item.label}</span>
                        <span className="text-[8px] text-ascii-dim font-mono">Send</span>
                      </button>
                    ))}
                  </div>
                </div>
              )}
            </div>
          )}

          <button
            onClick={() => setShowConfigMenu(!showConfigMenu)}
            className={`px-2.5 py-1 rounded font-mono text-[10px] transition-all cursor-pointer border ${
              showConfigMenu || calibrateMode
                ? 'bg-orange-950/10 border-orange-900/40 text-orange-400/80'
                : 'bg-ascii-dim/15 border-ascii-mid/20 text-ascii-mid hover:text-ascii-spark hover:border-ascii-bright/40'
            }`}
            title="Open settings"
          >
            ⚙️ SETTINGS
          </button>
        </div>
      </div>
    </main>
  )
}
