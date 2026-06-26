import { useState, useEffect, useRef } from 'react'
import RoomScene from './components/RoomScene.jsx'
import Clawd from './components/Clawd.jsx'
import HotspotOverlay from './components/HotspotOverlay.jsx'
import ControlPanel from './components/ControlPanel.jsx'

import {
  isSerialSupported,
  connectSerial,
  disconnectSerial,
  writeSerial,
} from './lib/serial.js'

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

const STORAGE_KEY = 'mochi-hotspots-calib'

const DEFAULT_HOTSPOTS = {
  painting: {
    x: 0.2686,
    y: 0.2633,
    r: 0.08,
    mode: 'animation',
    cmd: '1',
    label: 'Painting (Expression Mode)',
  },
  lock: {
    x: 0.2787,
    y: 0.4033,
    r: 0.0314,
    mode: 'clock',
    cmd: '2',
    label: 'Shelf Lock (Clock Mode)',
  },
  fan: {
    x: 0.9435,
    y: 0.278,
    r: 0.0366,
    mode: 'pomodoro',
    cmd: '3', // Sends 3 to switch to Pomodoro mode
    label: 'Spinning Fan (Pomodoro Mode)',
  },
  screen: {
    x: 0.5437,
    y: 0.4946,
    r: 0.0387,
    mode: 'terminal',
    cmd: '4',
    label: 'Computer Screen (Terminal Mode)',
  },
  cat: {
    x: 0.7742,
    y: 0.1814,
    r: 0.033,
    mode: 'usage',
    cmd: '5',
    label: 'Sleeping Cat (Claude Usage)',
  },
  window: {
    x: 0.51,
    y: 0.22,
    r: 0.09,
    mode: 'weather',
    cmd: '6',
    label: 'Window (Weather Mode)',
  },
}

export default function App() {
  const [isConnected, setIsConnected] = useState(false)
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

  const TRACKS = [
    'Ben Seretan – criss cross',
    'Claude – sonnet 4.6 lo-fi',
    'Mochi – chiptune sleep',
    'Lofi Girl – coding session',
    'Brian Eno – ambient coding',
    'Aphex Twin – selected track'
  ]

  const [currentTime, setCurrentTime] = useState(new Date())
  const [statusIdx, setStatusIdx] = useState(0)
  const [trackIdx, setTrackIdx] = useState(0)

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
  const suppressWeatherFetchRef = useRef(false)
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

  const getClosestAlarm = (alarmsList) => {
    const enabledAlarms = alarmsList.filter(a => a.enabled)
    if (enabledAlarms.length === 0) return null

    const now = new Date()
    let closestAlarm = null
    let minDiff = Infinity

    for (const alarm of enabledAlarms) {
      const [hh, mm] = alarm.time.split(':').map(Number)
      const target = new Date()
      target.setHours(hh, mm, 0, 0)
      if (target.getTime() <= now.getTime()) {
        target.setDate(target.getDate() + 1)
      }
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
  const handleSerialData = (chunk) => {
    console.log('[ESP32 Serial]:', chunk)
    serialLineBufRef.current += chunk
    const lines = serialLineBufRef.current.split('\n')
    serialLineBufRef.current = lines.pop() // keep any incomplete trailing line
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
        // Fire a one-shot ring on the device at the accurate T-0, then
        // re-pick the next occurrence (tomorrow for a daily alarm).
        if (isConnected) writeSerial('R\n')
        setActiveAlarm(getClosestAlarm(alarms))
      }
      if (activeTimer && now >= activeTimer.endTime) {
        setActiveTimer(null)
      }
    }, 1000)
    return () => clearInterval(timer)
  }, [activeAlarm, activeTimer, alarms, isConnected])

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
      await disconnectSerial()
      setIsConnected(false)
    } else {
      try {
        await connectSerial(
          // On disconnect
          () => setIsConnected(false),
          // On data received from ESP32
          handleSerialData
        )
        setIsConnected(true)

        // Silently sync system time on connect — stays on whatever mode
        // the device is currently showing (default Animation), no panel pop
        const now = new Date()
        const hh = String(now.getHours()).padStart(2, '0')
        const mm = String(now.getMinutes()).padStart(2, '0')
        const ss = String(now.getSeconds()).padStart(2, '0')
        // Seconds-precise so the device clock rolls over at the right moment
        // (the board has no RTC; minute-only sync drifts up to ~59s).
        await writeSerial(`T${hh}${mm}${ss}\n`)
      } catch (err) {
        alert(`Failed to connect to ESP32: ${err.message}`)
      }
    }
  }

  const handleWriteSerial = async (data) => {
    // Intercept alarm commands to manage list and sync closest automatically
    if (data.startsWith('r')) {
      const match = data.match(/^r(\d{4})/)
      if (match) {
        const hh = match[1].slice(0, 2)
        const mm = match[1].slice(2, 4)
        const alarmTime = `${hh}:${mm}`
        setAlarms(prev => {
          const exists = prev.find(a => a.time === alarmTime)
          if (exists) {
            return prev.map(a => a.time === alarmTime ? { ...a, enabled: true } : a)
          } else {
            return [...prev, { time: alarmTime, enabled: true }]
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
      const match = data.match(/^y(\d+)/)
      if (match) {
        const secs = parseInt(match[1], 10)
        setActiveTimer({
          durationSecs: secs,
          endTime: Date.now() + secs * 1000,
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
    if (id >= 700 && id < 800) return 45   // atmosphere (mist, fog, haze) → WC_FOG
    if (id >= 300 && id < 600) return 61   // drizzle, rain, snow → WC_RAIN
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

  // Auto-refresh weather every 2 minutes while in weather mode.
  // Skips the immediate fetch when a test-weather button was just used so
  // the real API doesn't overwrite the test data straight away.
  useEffect(() => {
    if (activeMode !== 'weather') return
    if (suppressWeatherFetchRef.current) {
      suppressWeatherFetchRef.current = false
    } else {
      handleFetchAndPushWeather()
    }
    const id = setInterval(() => handleFetchAndPushWeather(), 2 * 60 * 1000)
    return () => clearInterval(id)
  }, [activeMode]) // eslint-disable-line react-hooks/exhaustive-deps

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
      // Auto-sync system time on Clock Mode switch
      const now = new Date()
      const hh = String(now.getHours()).padStart(2, '0')
      const mm = String(now.getMinutes()).padStart(2, '0')
      handleWriteSerial(`2t${hh}${mm}\n`)
      setActiveMode('clock')
    } else if (mode === 'usage') {
      // Refresh usage immediately rather than waiting on the 60s poll
      handleTestUsage()
    } else if (mode === 'weather') {
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
          <div className="flex items-center gap-1.5 border-r border-ascii-mid/20 pr-3 select-none text-ascii-mid">
            <span className="text-[#e05f3e] text-[13px] font-bold animate-pulse">♫ )</span>
            <div className="w-[120px] overflow-hidden whitespace-nowrap relative flex items-center">
              <div className="inline-block animate-marquee">
                <span className="text-ascii-spark mr-8">{TRACKS[trackIdx]}</span>
                <span className="text-ascii-spark mr-8">{TRACKS[trackIdx]}</span>
              </div>
            </div>
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
        onRefreshWeather={() => handleFetchAndPushWeather(selectedLocation)}
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

          {isSerialSupported() ? (
            <button
              onClick={handleConnect}
              className={`flex items-center gap-2 px-2.5 py-1 rounded text-[10px] font-mono tracking-wider transition-all duration-300 border cursor-pointer ${
                isConnected
                  ? 'bg-orange-950/10 border-orange-900/30 text-orange-400/80 hover:bg-orange-950/20 hover:text-orange-400'
                  : 'bg-ascii-dim/15 border-ascii-mid/20 text-ascii-mid hover:text-ascii-spark hover:border-ascii-bright/40'
              }`}
            >
              <span className="text-[12px]">{isConnected ? '🔌' : '🔌'}</span>
              <span>{isConnected ? 'SERIAL: CONNECTED' : 'CONNECT ESP32'}</span>
            </button>
          ) : (
            <span className="text-[9px] text-red-400 bg-red-950/10 border border-red-900/20 px-2 py-0.5 rounded leading-none">
              Web Serial Unsupported (Use Chrome/Edge)
            </span>
          )}
        </div>

        {/* Right side: configuration control popover */}
        <div className="flex items-center gap-2 pointer-events-auto relative config-menu-container">
          {showConfigMenu && (
            <div className="absolute bottom-10 right-0 z-30 w-48 bg-room-bg/95 border border-ascii-mid/20 rounded-lg p-2 font-mono text-[10px] text-ascii-bright/80 backdrop-blur shadow-xl flex flex-col gap-1.5 max-h-[320px] overflow-y-auto">
              <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pb-1">System Controls</div>
              
              <button
                onClick={() => {
                  setCalibrateMode(!calibrateMode)
                  setShowConfigMenu(false)
                }}
                className={`w-full text-left px-2 py-1.2 rounded transition-all cursor-pointer flex items-center justify-between ${
                  calibrateMode
                    ? 'bg-orange-950/10 text-orange-400/80 font-bold border border-orange-900/20'
                    : 'hover:bg-ascii-bright/5 hover:text-ascii-spark text-ascii-bright/70'
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
                  className="w-full text-left px-2 py-1.2 rounded text-red-400/80 hover:bg-red-950/10 transition-all cursor-pointer border border-transparent hover:border-red-900/20"
                >
                  🗑️ Reset
                </button>
              )}

              <button
                onClick={() => {
                  const url = new URL(window.location.href)
                  url.searchParams.set('tune', '1')
                  window.location.href = url.toString()
                }}
                className="w-full text-left px-2 py-1.2 rounded hover:bg-ascii-bright/5 hover:text-ascii-spark text-ascii-bright/70 transition-all cursor-pointer"
              >
                🎨 Tune Mascot
              </button>

              <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Screen</div>
              <button
                onClick={() => handleWriteSerial('b')}
                className="w-full text-left px-2 py-1.2 rounded hover:bg-ascii-bright/5 hover:text-ascii-spark text-ascii-bright/70 transition-all cursor-pointer"
              >
                💡 Toggle Backlight
              </button>
              <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Idle Face Switch — every ~{idleIntervalSec}s</div>
              <div className="grid grid-cols-4 gap-1 px-1">
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

              <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Test Modes</div>
              <div className="grid grid-cols-3 gap-1">
                {[
                  { label: 'Expr', cmd: '1', mode: 'animation' },
                  { label: 'Clock', cmd: '2', mode: 'clock' },
                  { label: 'Pomo', cmd: '3', mode: 'pomodoro' },
                  { label: 'Term', cmd: '4', mode: 'terminal' },
                  { label: 'Usage', cmd: '5', mode: 'usage' },
                  { label: 'Wx', cmd: '6', mode: 'weather' }
                ].map((item) => (
                  <button
                    key={item.cmd}
                    onClick={async () => {
                      if (item.mode === 'clock') {
                        const now = new Date()
                        const hh = String(now.getHours()).padStart(2, '0')
                        const mm = String(now.getMinutes()).padStart(2, '0')
                        await handleWriteSerial(`2t${hh}${mm}\n`)
                      } else if (item.mode === 'usage') {
                        handleTestUsage()
                      } else {
                        await handleWriteSerial(item.cmd)
                      }
                      setActiveMode(item.mode)
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

              <div className="text-[9px] text-ascii-dim uppercase font-bold px-1 select-none border-b border-ascii-mid/10 pt-2 pb-1">Test Weather</div>
              <div className="flex flex-col gap-1">
                {[
                  { label: '☀️ Clear', cmd: 'W0,30,32,80,Ho Chi Minh City,Clear Sky\n' },
                  { label: '☁️ Cloudy', cmd: 'W3,25,27,85,Ho Chi Minh City,Partly Cloudy\n' },
                  { label: '🌫️ Fog', cmd: 'W45,20,20,95,Ho Chi Minh City,Foggy\n' },
                  { label: '🌧️ Rain', cmd: 'W61,22,22,90,Ho Chi Minh City,Light Drizzle\n' },
                  { label: '⛈️ Storm', cmd: 'W95,18,18,98,Ho Chi Minh City,Thunderstorm\n' }
                ].map((item, idx) => (
                  <button
                    key={idx}
                    onClick={async () => {
                      suppressWeatherFetchRef.current = true
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

          <button
            onClick={() => setShowConfigMenu(!showConfigMenu)}
            className={`px-2.5 py-1 rounded font-mono text-[10px] transition-all cursor-pointer border ${
              showConfigMenu || calibrateMode
                ? 'bg-orange-950/10 border-orange-900/40 text-orange-400/80'
                : 'bg-ascii-dim/15 border-ascii-mid/20 text-ascii-mid hover:text-ascii-spark hover:border-ascii-bright/40'
            }`}
            title="Configure settings"
          >
            ⚙️ CONFIGURE
          </button>
        </div>
      </div>
    </main>
  )
}
