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
}

export default function App() {
  const [isConnected, setIsConnected] = useState(false)
  const [activeMode, setActiveMode] = useState(null) // 'animation' | 'clock' | 'terminal' | null
  const [activeAlarm, setActiveAlarm] = useState(null) // { durationMins, endTime }
  const [activeTimer, setActiveTimer] = useState(null) // { durationSecs, endTime }
  const [isPomoActive, setIsPomoActive] = useState(false)
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

  // Animated Favicon loop
  useEffect(() => {
    const faviconFrames = [0, 1, 2, 3, 4].map((i) => `/favicon/frame${i}.png`)
    let frameIdx = 0

    let faviconLink = document.querySelector("link[rel~='icon']")
    if (!faviconLink) {
      faviconLink = document.createElement('link')
      faviconLink.rel = 'icon'
      document.getElementsByTagName('head')[0].appendChild(faviconLink)
    }
    faviconLink.type = 'image/png'

    const interval = setInterval(() => {
      frameIdx = (frameIdx + 1) % faviconFrames.length
      faviconLink.href = faviconFrames[frameIdx]
    }, 250) // update frame every 250ms

    return () => clearInterval(interval)
  }, [])

  // Claude Pro usage polling (5h session % / 7d weekly %)
  const [usage, setUsage] = useState(null) // { sessionPct, weeklyPct, sessionResetAt, weeklyResetAt } | null

  const pollUsage = async () => {
    try {
      const res = await fetch('/api/usage')
      const data = await res.json()
      const next = res.ok && !data.error ? data : null
      setUsage(next)
      return next
    } catch {
      setUsage(null)
      return null
    }
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

  // Sync closest alarm to ESP32
  const wasConnectedRef = useRef(false)
  useEffect(() => {
    if (!isConnected) {
      wasConnectedRef.current = false
      return
    }
    
    const closest = getClosestAlarm(alarms)
    const justConnected = !wasConnectedRef.current
    wasConnectedRef.current = true
    
    if (closest) {
      if (justConnected || !activeAlarm || activeAlarm.targetTime !== closest.targetTime) {
        const [hh, mm] = closest.time.split(':')
        if (justConnected) {
          setTimeout(() => {
            writeSerial(`r${hh}${mm}\n`)
            setActiveAlarm(closest)
          }, 100)
        } else {
          writeSerial(`r${hh}${mm}\n`)
          setActiveAlarm(closest)
        }
      }
    } else {
      if (justConnected || activeAlarm) {
        if (justConnected) {
          setTimeout(() => {
            writeSerial('a')
            setActiveAlarm(null)
          }, 100)
        } else {
          writeSerial('a')
          setActiveAlarm(null)
        }
      }
    }
  }, [alarms, activeAlarm, isConnected])

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
        setActiveAlarm(null)
      }
      if (activeTimer && now >= activeTimer.endTime) {
        setActiveTimer(null)
      }
    }, 1000)
    return () => clearInterval(timer)
  }, [activeAlarm, activeTimer])

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
        await writeSerial(`T${hh}${mm}\n`)
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

  const handleTriggerMode = (key, mode, cmd) => {
    if (mode === 'clock') {
      // Auto-sync system time on Clock Mode switch
      const now = new Date()
      const hh = String(now.getHours()).padStart(2, '0')
      const mm = String(now.getMinutes()).padStart(2, '0')
      handleWriteSerial(`2t${hh}${mm}\n`)
      setActiveMode('clock')
    } else if (mode === 'usage') {
      // Refresh usage immediately rather than waiting on the 60s poll
      handleTestUsage()
    } else {
      // Send command to ESP32
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
        isConnected={isConnected}
        activeAlarm={activeAlarm}
        activeTimer={activeTimer}
        isPomoActive={isPomoActive}
        usage={usage}
        alarms={alarms}
        setAlarms={setAlarms}
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
            <div className="absolute bottom-10 right-0 z-30 w-40 bg-room-bg/90 border border-ascii-mid/20 rounded-lg p-2 font-mono text-[10px] text-ascii-bright/80 backdrop-blur shadow-xl flex flex-col gap-1">
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
