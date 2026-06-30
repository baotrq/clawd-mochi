import { useEffect, useRef, useState } from 'react'
import mascotIcon from '../assets/ezgif-frame-016.png'

function formatCowsay(text = 'moo') {
  const maxWordLen = 28
  const words = text.split(/\s+/)
  const lines = []
  let currentLine = ''

  for (const word of words) {
    if ((currentLine + (currentLine ? ' ' : '') + word).length <= maxWordLen) {
      currentLine += (currentLine ? ' ' : '') + word
    } else {
      if (currentLine) {
        lines.push(currentLine)
      }
      currentLine = word
      while (currentLine.length > maxWordLen) {
        lines.push(currentLine.substring(0, maxWordLen))
        currentLine = currentLine.substring(maxWordLen)
      }
    }
  }
  if (currentLine) {
    lines.push(currentLine)
  }

  if (lines.length === 0) {
    lines.push('moo')
  }

  const maxLen = Math.max(...lines.map((l) => l.length))
  const borderTop = '  ' + '_'.repeat(maxLen + 2)
  const borderBottom = '  ' + '-'.repeat(maxLen + 2)

  const bubble = []
  bubble.push(borderTop)

  if (lines.length === 1) {
    bubble.push(`< ${lines[0]} >`)
  } else {
    for (let i = 0; i < lines.length; i++) {
      let left = '|'
      let right = '|'
      if (i === 0) {
        left = '/'
        right = '\\'
      } else if (i === lines.length - 1) {
        left = '\\'
        right = '/'
      }
      const pad = ' '.repeat(maxLen - lines[i].length)
      bubble.push(`${left} ${lines[i]}${pad} ${right}`)
    }
  }
  bubble.push(borderBottom)

  const cow = `        \\   ^__^
         \\  (oo)\\_______
            (__)\\       )\\/\\
                ||----w |
                ||     ||`

  return bubble.join('\n') + '\n' + cow
}

function formatResetIn(epochMs) {
  if (!epochMs) return null
  const diffMin = Math.round((epochMs - Date.now()) / 60000)
  if (diffMin <= 0) return 'soon'
  const h = Math.floor(diffMin / 60)
  const m = diffMin % 60
  return h > 0 ? `in ${h}h ${m}m` : `in ${m}m`
}

const EXPRESSIONS = [
  { label: 'Normal', cmd: 'w', face: '●  ●', desc: 'Standard blinking eyes' },
  { label: 'Squish', cmd: 's', face: '˃  ˂', desc: 'Happy squint eyes' },
  { label: 'Blink', cmd: 'e', face: '–  –', desc: 'Single blink' },
  { label: 'Double Blink', cmd: 'f', face: '⁑  ⁑', desc: 'Quick double blink' },
  { label: 'Look Around', cmd: 'g', face: '◀  ▶', desc: 'Eyes look left and right' },
  { label: 'Wink', cmd: 'h', face: '●  –', desc: 'Wink a random eye' },
  { label: 'Sleepy', cmd: 'i', face: '∪  ∪', desc: 'Eyes droop closed' },
  { label: 'Surprised', cmd: 'j', face: 'O  O', desc: 'Eyes widen in surprise' },
  { label: 'Squint', cmd: 'k', face: '¯  ¯', desc: 'Eyes narrow' },
  { label: 'Nod', cmd: 'l', face: '↕  ↕', desc: 'Eyes nod up and down' },
  { label: 'Shake', cmd: 'n', face: '↔  ↔', desc: 'Eyes shake side to side' },
  { label: 'Roll', cmd: 'o', face: '↺  ↺', desc: 'Eyes roll in a circle' },
  { label: 'Cross-eyed', cmd: 'u', face: '↘  ↙', desc: 'Eyes tilt inward' },
  { label: 'Confused', cmd: 'v', face: '∖  ∕', desc: 'Opposite tilted eyes' },
  { label: 'Excited', cmd: 'x', face: 'ˆ  ˆ', desc: 'Jumping excited eyes' },
  { label: 'Heart Eyes', cmd: 'p', face: '♥  ♥', desc: 'Heart-shaped eyes' },
  { label: 'Dizzy', cmd: 'q', face: '✖  ✖', desc: 'Dizzy cross eyes' },
  { label: 'Glitch', cmd: 'a', face: '▰  ▱', desc: 'Pixelated glitchy eyes' },
  { label: 'Hypnotized', cmd: 'y', face: '◎  ◎', desc: 'Spinning concentric spiral eyes' },
  { label: 'Crying', cmd: 'c', face: '〒  〒', desc: 'Sad slanting eyes with static wide blue tears' },
  { label: 'Neko Cute', cmd: 'd', face: '＾ ＾', desc: 'Anime-style cute blinking cat eyes' },
  { label: 'Anthropic Logo', cmd: 'z', face: ' ⬡ ', desc: 'Draw Anthropic logo line by line' },
]

// Mirrors the SONGS[] table in the firmware's sing.ino — keys must match.
const SONGS = [
  { key: 'q', label: 'Twinkle Star', emoji: '⭐' },
  { key: 'w', label: 'Ode to Joy', emoji: '🎼' },
  { key: 'e', label: "Mary's Lamb", emoji: '🐑' },
  { key: 'r', label: 'Jingle Bells', emoji: '🔔' },
  { key: 't', label: 'Happy Birthday', emoji: '🎂' },
  { key: 'y', label: 'Tháng Tư Là Lời Nói Dối Của Anh', emoji: '🌸' },
]

export default function ControlPanel({
  activeMode,
  onClose,
  onWriteSerial,
  onRefreshWeather,
  isConnected,
  activeAlarm,
  activeTimer,
  isPomoActive,
  usage,
  weatherData,
  alarms = [],
  setAlarms,
  selectedLocation,
  recentLocations = [],
  onLocationChange,
  onSearchLocations,
  onDeleteRecentLocation,
}) {
  const returnToIdleRef = useRef(null)
  useEffect(() => () => { if (returnToIdleRef.current) clearTimeout(returnToIdleRef.current) }, [])

  const handleExpressionClick = (cmd) => {
    if (returnToIdleRef.current) clearTimeout(returnToIdleRef.current)
    onWriteSerial(cmd)
    returnToIdleRef.current = setTimeout(() => {
      onWriteSerial('w')
      returnToIdleRef.current = null
    }, 5000)
  }

  const [alarmHourStr, setAlarmHourStr] = useState('')
  const [alarmMinuteStr, setAlarmMinuteStr] = useState('')
  const [alarmNameStr, setAlarmNameStr] = useState('')
  const [alarmDays, setAlarmDays] = useState([1, 2, 3, 4, 5]) // Default to weekdays (Mon-Fri)
  const [editingAlarmIndex, setEditingAlarmIndex] = useState(null)
  const [clockHourStr, setClockHourStr] = useState('')
  const [clockMinuteStr, setClockMinuteStr] = useState('')
  const [terminalInput, setTerminalInput] = useState('')
  const [clockTab, setClockTab] = useState('clock') // 'clock' | 'alarm' | 'timer'
  const [timerMin, setTimerMin] = useState('')
  const [timerSec, setTimerSec] = useState('')
  const [timerNameStr, setTimerNameStr] = useState('')
  const [pomoFocusStr, setPomoFocusStr] = useState('25')
  const [pomoFocusSecStr, setPomoFocusSecStr] = useState('0')
  const [pomoBreakStr, setPomoBreakStr] = useState('5')
  // Sing-mode UI state. The device owns the real playback state; these mirror
  // it optimistically so the buttons feel responsive (it's a toy jukebox).
  const [singSong, setSingSong] = useState(null) // key of the selected song
  const [singPlaying, setSingPlaying] = useState(false)
  const [singLoop, setSingLoop] = useState(true)
  const [alarmTimeLeft, setAlarmTimeLeft] = useState('')
  const [timerTimeLeft, setTimerTimeLeft] = useState('')
  const [searchQuery, setSearchQuery] = useState('')
  const [searchResults, setSearchResults] = useState([])
  const [isSearching, setIsSearching] = useState(false)
  const termInputRef = useRef(null)

  useEffect(() => {
    if (!searchQuery || searchQuery.trim().length < 2) {
      setSearchResults([])
      return
    }
    const delayDebounce = setTimeout(async () => {
      setIsSearching(true)
      try {
        const results = await onSearchLocations(searchQuery)
        setSearchResults(results || [])
      } catch (err) {
        console.error(err)
      } finally {
        setIsSearching(false)
      }
    }, 400)
    return () => clearTimeout(delayDebounce)
  }, [searchQuery, onSearchLocations])
  const termEndRef = useRef(null)
  const panelRef = useRef(null)
  const [terminalHistory, setTerminalHistory] = useState([])

  // Click outside the panel to close it (no need to hit the ✕). Hotspot
  // triggers manage their own activeMode transitions, so they're excluded
  // here to avoid a close-then-reopen race when switching dashboards.
  useEffect(() => {
    if (!activeMode) return
    const handleOutsideClick = (e) => {
      if (panelRef.current && panelRef.current.contains(e.target)) return
      if (e.target.closest('[data-hotspot-trigger]')) return
      onClose()
    }
    window.addEventListener('mousedown', handleOutsideClick)
    return () => window.removeEventListener('mousedown', handleOutsideClick)
  }, [activeMode, onClose])

  // Track Alarm time left
  useEffect(() => {
    if (!activeAlarm) {
      setAlarmTimeLeft('')
      return
    }
    const update = () => {
      const remainMs = activeAlarm.endTime - Date.now()
      if (remainMs <= 0) {
        setAlarmTimeLeft('')
      } else {
        const secs = Math.ceil(remainMs / 1000)
        const mm = Math.floor(secs / 60)
        const ss = secs % 60
        setAlarmTimeLeft(`${mm}:${String(ss).padStart(2, '0')} remaining`)
      }
    }
    update()
    const intv = setInterval(update, 1000)
    return () => clearInterval(intv)
  }, [activeAlarm])

  // Track Timer time left
  useEffect(() => {
    if (!activeTimer) {
      setTimerTimeLeft('')
      return
    }
    const update = () => {
      const remainMs = activeTimer.endTime - Date.now()
      if (remainMs <= 0) {
        setTimerTimeLeft('')
      } else {
        const secs = Math.ceil(remainMs / 1000)
        const mm = Math.floor(secs / 60)
        const ss = secs % 60
        setTimerTimeLeft(`${mm}:${String(ss).padStart(2, '0')}`)
      }
    }
    update()
    const intv = setInterval(update, 1000)
    return () => clearInterval(intv)
  }, [activeTimer])

  // Focus terminal input when it opens
  useEffect(() => {
    if (activeMode === 'terminal' && termInputRef.current) {
      termInputRef.current.focus()
    }
  }, [activeMode])

  // Scroll terminal to bottom
  useEffect(() => {
    if (activeMode === 'terminal' && termEndRef.current) {
      termEndRef.current.scrollIntoView({ behavior: 'smooth' })
    }
  }, [terminalHistory, activeMode])

  const formatAlarmDays = (days) => {
    if (!days || days.length === 0) return 'Never'
    if (days.length === 7) return 'Every day'
    if (days.length === 5 && !days.includes(0) && !days.includes(6)) return 'Weekdays'
    if (days.length === 2 && days.includes(0) && days.includes(6)) return 'Weekends'
    const DAY_NAMES = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']
    return days.map(d => DAY_NAMES[d]).join(', ')
  }

  if (!activeMode) return null

  // Helper to sync time. Seconds-precise (silent 'T', not minute-only 't') so
  // the device clock matches real wall-clock instead of truncating to :00 and
  // running up to ~59s slow.
  const handleSyncTime = () => {
    const now = new Date()
    const hh = String(now.getHours()).padStart(2, '0')
    const mm = String(now.getMinutes()).padStart(2, '0')
    const ss = String(now.getSeconds()).padStart(2, '0')
    onWriteSerial(`T${hh}${mm}${ss}\n`)
  }

  // Handle manual clock set submission
  const handleSetClock = (e) => {
    e.preventDefault()
    const hr = (parseInt(clockHourStr, 10) || 0) % 24
    const mn = (parseInt(clockMinuteStr, 10) || 0) % 60
    const hh = String(hr).padStart(2, '0')
    const mm = String(mn).padStart(2, '0')
    onWriteSerial(`t${hh}${mm}\n`)
    setClockHourStr('')
    setClockMinuteStr('')
  }

  const handleSendTerminalCommand = (e) => {
    e.preventDefault()
    if (!terminalInput.trim()) return
    const cmd = terminalInput.trim()
    
    // Handle local clear command
    if (cmd.toLowerCase() === 'clear') {
      setTerminalHistory([])
      onWriteSerial(cmd + '\n')
      setTerminalInput('')
      return
    }

    // Handle local cowsay command
    if (cmd.toLowerCase() === 'cowsay' || cmd.toLowerCase().startsWith('cowsay ')) {
      const msg = cmd.toLowerCase().startsWith('cowsay ') ? cmd.substring(7).trim() : 'moo'
      const cowsayOutput = formatCowsay(msg)
      setTerminalHistory((prev) => [...prev, `clawd:~$ ${cmd}`, cowsayOutput])
      onWriteSerial(cmd + '\n')
      setTerminalInput('')
      return
    }

    // Add to local history
    setTerminalHistory((prev) => [...prev, `clawd:~$ ${cmd}`])
    
    // Send to serial
    onWriteSerial(cmd + '\n')
    
    // Handle local exit command
    if (cmd.toLowerCase() === 'exit') {
      onClose()
    }
    
    // Clear input
    setTerminalInput('')
  }

  const handleExitTerminal = () => {
    onWriteSerial('exit\n')
    onClose()
  }

  // Starting sends one atomic "set durations + start" signal (P + 5 digits:
  // MMSSB — focus min, focus sec, break min, e.g. P24305 = 24:30 focus / 5
  // min break) so the device sets and starts in a single step. Stopping is
  // just 'p' (toggles the already-running session off). Typing a duration
  // alone never touches the device — there's no separate "Set".
  const handleStartStopPomo = () => {
    if (isPomoActive) {
      onWriteSerial('p')
      return
    }
    const focusMins = Math.min(99, Math.max(0, parseInt(pomoFocusStr, 10) || 0)) || 25
    const focusSecs = Math.min(59, Math.max(0, parseInt(pomoFocusSecStr, 10) || 0))
    const breakMins = Math.min(9, Math.max(1, parseInt(pomoBreakStr, 10) || 5))
    const mm = String(focusMins).padStart(2, '0')
    const ss = String(focusSecs).padStart(2, '0')
    const b = String(breakMins)
    onWriteSerial(`P${mm}${ss}${b}\n`)
  }

  if (activeMode === 'terminal') {
    return (
      <div ref={panelRef} className="absolute top-4 left-4 z-20 w-96 font-mono select-text bg-room-bg/95 border border-ascii-mid/30 rounded-xl p-5 flex flex-col gap-4 backdrop-blur shadow-2xl text-ascii-bright">
        {/* Header block with orange invader mascot */}
        <div className="flex gap-3 items-start select-none">
          {/* Mascot Image */}
          <img src={mascotIcon} alt="Claude Mascot" className="w-9 h-9 object-contain shrink-0 mt-0.5" />
          {/* Title block */}
          <div className="flex flex-col gap-0.5 leading-snug">
            <span className="text-ascii-spark font-bold">Claude Code v2.1.187</span>
            <span className="text-ascii-bright/90 text-[10px]">Sonnet 4.6 with high effort · Claude Pro</span>
            <span className="text-ascii-bright/90 text-[10px]">/home/vinit-admin</span>
          </div>
        </div>

        {/* Terminal output box */}
        <div className="h-44 overflow-y-auto space-y-1.5 scrollbar-none text-[11px] leading-relaxed mt-1">
          <div className="text-ascii-bright/60 select-none">
            │ Using Sonnet 4.6 (from .claude/settings.json) · /model
          </div>
          {terminalHistory.map((line, idx) => (
            <div key={idx} className="whitespace-pre-wrap text-ascii-spark">
              {line}
            </div>
          ))}
          <div ref={termEndRef} />
        </div>

        {/* Prompt input Form with line separators */}
        <div className="border-t border-b border-ascii-mid/30 py-2.5 my-1">
          <form onSubmit={handleSendTerminalCommand} className="flex gap-2 items-center">
            <span className="text-[#e05f3e] font-bold select-none shrink-0 font-sans text-sm">⟩</span>
            <input
              ref={termInputRef}
              type="text"
              value={terminalInput}
              onChange={(e) => setTerminalInput(e.target.value)}
              className="flex-1 bg-transparent border-none outline-none font-mono text-[11px] text-ascii-spark placeholder-ascii-mid/40"
              placeholder='Try "create a util logging.py that..."'
            />
          </form>
        </div>

        {/* Shortcuts Footer */}
        <div className="flex items-center justify-between text-[9px] text-ascii-bright/80 select-none px-1">
          <div className="flex gap-3">
            <span>? for shortcuts</span>
            <span>•</span>
            <span>← for agents</span>
          </div>
          <button
            onClick={handleExitTerminal}
            className="bg-red-950/25 hover:bg-red-950/45 border border-red-900/40 text-red-400 py-0.5 px-2.5 rounded cursor-pointer transition-colors font-sans text-[9px] font-medium"
          >
            Exit
          </button>
        </div>
      </div>
    )
  }

  return (
    <div ref={panelRef} className="absolute top-4 left-4 z-20 w-96 bg-room-bg/95 border border-ascii-mid/30 rounded-xl p-5 font-mono text-xs text-ascii-bright backdrop-blur shadow-2xl flex flex-col gap-4">
      {/* Header */}
      <div className="flex items-center justify-between border-b border-ascii-mid/20 pb-2">
        <span className="text-ascii-spark font-bold tracking-widest uppercase">
          {activeMode === 'animation' && 'Clawd Animations'}
          {activeMode === 'clock' && 'Clock Settings'}
          {activeMode === 'pomodoro' && 'Pomodoro Timer'}
          {activeMode === 'usage' && 'Claude Usage'}
          {activeMode === 'weather' && 'Weather Station'}
          {activeMode === 'sing' && 'Sing Mode'}
        </span>
        <button
          onClick={onClose}
          className="text-ascii-mid hover:text-ascii-spark text-sm font-bold cursor-pointer transition-colors p-1"
        >
          ✕
        </button>
      </div>

      {/* Warning if not connected */}
      {!isConnected && (
        <div className="bg-orange-950/40 border border-orange-800/40 rounded px-2.5 py-1.5 text-orange-300 leading-snug">
          ⚠️ Serial connection inactive. Connect via USB in the bottom-left to send commands.
        </div>
      )}

      {/* Mode content */}
      {activeMode === 'animation' && (
        <div className="space-y-4">
          <div className="w-full py-4 px-4 flex items-center justify-center">
            <span className="text-lg font-serif-display italic font-medium tracking-wide text-[#e8a98a]">
              Let Claude Vibe
            </span>
          </div>

          <div className="h-72 overflow-y-auto scrollbar-none pr-0.5">
            <div className="grid grid-cols-2 gap-2">
              {EXPRESSIONS.map((exp) => (
                <button
                  key={exp.cmd}
                  onClick={() => handleExpressionClick(exp.cmd)}
                  title={exp.desc}
                  className="bg-ascii-dim/5 hover:bg-ascii-bright/10 border border-ascii-mid/15 hover:border-ascii-bright/40 text-center py-2.5 px-2 rounded-xl transition-all cursor-pointer flex flex-col items-center justify-center group"
                >
                  <span className="text-base font-bold text-ascii-spark tracking-widest font-mono group-hover:scale-110 transition-transform">
                    {exp.face}
                  </span>
                  <span className="text-[9px] text-ascii-mid group-hover:text-ascii-spark uppercase tracking-wider font-sans transition-colors mt-1 block">
                    {exp.label}
                  </span>
                </button>
              ))}
            </div>
          </div>
        </div>
      )}

      {activeMode === 'usage' && (
        <div className="space-y-4">
          <div className="grid grid-cols-2 gap-2">
            <div className="bg-emerald-950/20 border border-emerald-500/30 rounded p-3 text-center">
              <div className="text-[9px] text-ascii-mid uppercase font-bold tracking-wider mb-1">5h Session</div>
              <div className="text-2xl font-bold font-mono text-ascii-spark">
                {usage ? `${usage.sessionPct}%` : '—'}
              </div>
              {usage?.sessionResetAt && (
                <div className="text-[9px] text-ascii-mid mt-1">resets {formatResetIn(usage.sessionResetAt)}</div>
              )}
            </div>
            <div className="bg-emerald-950/20 border border-emerald-500/30 rounded p-3 text-center">
              <div className="text-[9px] text-ascii-mid uppercase font-bold tracking-wider mb-1">7d Weekly</div>
              <div className="text-2xl font-bold font-mono text-ascii-spark">
                {usage ? `${usage.weeklyPct}%` : '—'}
              </div>
              {usage?.weeklyResetAt && (
                <div className="text-[9px] text-ascii-mid mt-1">resets {formatResetIn(usage.weeklyResetAt)}</div>
              )}
            </div>
          </div>
          {!usage && (
            <div className="text-[10px] text-ascii-mid text-center italic leading-relaxed">
              No data yet — make sure you're logged into Claude Code on this machine, then hit ↻ in the top-right status bar.
            </div>
          )}
          <div className="text-[10px] text-ascii-mid leading-relaxed text-center italic">
            Pushed to the device's Usage screen (command 5). Refreshes every 60s, or hit ↻ in the top-right status bar to test immediately.
          </div>
        </div>
      )}

      {activeMode === 'clock' && (
        <div className="space-y-4">
          {/* Sub-tabs */}
          <div className="grid grid-cols-3 gap-1 pb-3 border-b border-ascii-mid/10">
            <button
              onClick={() => setClockTab('clock')}
              className={`py-1 rounded text-center cursor-pointer transition-all text-[10px] ${
                clockTab === 'clock'
                  ? 'bg-ascii-bright/20 text-ascii-spark font-bold border border-ascii-mid/40'
                  : 'bg-transparent text-ascii-mid hover:text-ascii-bright border border-transparent'
              }`}
            >
              Clock
            </button>
            <button
              onClick={() => setClockTab('alarm')}
              className={`py-1 rounded text-center cursor-pointer transition-all text-[10px] ${
                clockTab === 'alarm'
                  ? 'bg-ascii-bright/20 text-ascii-spark font-bold border border-ascii-mid/40'
                  : 'bg-transparent text-ascii-mid hover:text-ascii-bright border border-transparent'
              }`}
            >
              Alarm
            </button>
            <button
              onClick={() => setClockTab('timer')}
              className={`py-1 rounded text-center cursor-pointer transition-all text-[10px] ${
                clockTab === 'timer'
                  ? 'bg-ascii-bright/20 text-ascii-spark font-bold border border-ascii-mid/40'
                  : 'bg-transparent text-ascii-mid hover:text-ascii-bright border border-transparent'
              }`}
            >
              Timer
            </button>
          </div>

          {/* Clock Tab */}
          {clockTab === 'clock' && (
            <div className="space-y-4">
              <button
                onClick={handleSyncTime}
                className="w-full bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark font-semibold py-2 rounded transition-all cursor-pointer text-center"
              >
                🕒 Sync System Time
              </button>

              <form onSubmit={handleSetClock} className="space-y-2">
                <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                  Set Time Manually
                </div>
                <div className="flex gap-2 items-center">
                  <input
                    type="number"
                    min={0}
                    max={23}
                    placeholder="Hr"
                    value={clockHourStr}
                    onChange={(e) => setClockHourStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2 py-1 text-ascii-spark outline-none font-mono"
                  />
                  <span>:</span>
                  <input
                    type="number"
                    min={0}
                    max={59}
                    placeholder="Min"
                    value={clockMinuteStr}
                    onChange={(e) => setClockMinuteStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2 py-1 text-ascii-spark outline-none font-mono"
                  />
                  <button
                    type="submit"
                    className="bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark px-3 py-1 rounded cursor-pointer font-bold"
                  >
                    Set
                  </button>
                </div>
              </form>
              
              <div className="text-[10px] text-ascii-mid leading-relaxed text-center italic">
                Device time automatically syncs when connecting or opening Clock Mode.
              </div>
            </div>
          )}

          {clockTab === 'alarm' && (
            <div className="space-y-4">
              {activeAlarm && (
                <div className="bg-emerald-950/20 border border-emerald-500/30 rounded p-2.5 text-emerald-400 text-center font-bold">
                  🔔 Active Alarm: {activeAlarm.targetTime || '08:00'}
                  {activeAlarm.name && (
                    <div className="text-[11px] text-ascii-bright/80 font-sans font-normal">{activeAlarm.name}</div>
                  )}
                  <div className="text-xl font-bold font-mono tracking-widest text-ascii-spark mt-1 animate-pulse">
                    {alarmTimeLeft || 'calculating...'}
                  </div>
                </div>
              )}

              <form
                onSubmit={(e) => {
                  e.preventDefault()
                  const hr = (parseInt(alarmHourStr, 10) || 0) % 24
                  const mn = (parseInt(alarmMinuteStr, 10) || 0) % 60
                  const hh = String(hr).padStart(2, '0')
                  const mm = String(mn).padStart(2, '0')
                  const nm = alarmNameStr.trim()
                  
                  setAlarms(prev => {
                    const alarmTime = `${hh}:${mm}`
                    const newAlarm = { time: alarmTime, enabled: true, name: nm, days: alarmDays }
                    if (editingAlarmIndex !== null) {
                      return prev.map((a, i) => i === editingAlarmIndex ? newAlarm : a)
                    } else {
                      const exists = prev.find(a => a.time === alarmTime)
                      if (exists) {
                        return prev.map(a => a.time === alarmTime ? { ...a, enabled: true, name: nm || a.name, days: alarmDays } : a)
                      } else {
                        return [...prev, newAlarm]
                      }
                    }
                  })

                  setAlarmHourStr('')
                  setAlarmMinuteStr('')
                  setAlarmNameStr('')
                  setAlarmDays([1, 2, 3, 4, 5]) // Reset to weekdays default
                  setEditingAlarmIndex(null)
                }}
                className="space-y-2"
              >
                <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                  {editingAlarmIndex !== null ? 'Edit Alarm Time' : 'Add Alarm Time'}
                </div>
                <div className="flex gap-2 items-center">
                  <input
                    type="number"
                    min={0}
                    max={23}
                    placeholder="Hr"
                    value={alarmHourStr}
                    onChange={(e) => setAlarmHourStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono"
                  />
                  <span>:</span>
                  <input
                    type="number"
                    min={0}
                    max={59}
                    placeholder="Min"
                    value={alarmMinuteStr}
                    onChange={(e) => setAlarmMinuteStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono"
                  />
                  <div className="flex gap-1">
                    <button
                      type="submit"
                      className="bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark px-3 py-1 rounded cursor-pointer font-bold"
                    >
                      {editingAlarmIndex !== null ? 'Save' : 'Add'}
                    </button>
                    {editingAlarmIndex !== null && (
                      <button
                        type="button"
                        onClick={() => {
                          setAlarmHourStr('')
                          setAlarmMinuteStr('')
                          setAlarmNameStr('')
                          setAlarmDays([1, 2, 3, 4, 5])
                          setEditingAlarmIndex(null)
                        }}
                        className="bg-ascii-dim/10 hover:bg-ascii-bright/15 border border-ascii-mid/30 text-ascii-mid px-2 py-1 rounded cursor-pointer"
                        title="Cancel editing"
                      >
                        ✕
                      </button>
                    )}
                  </div>
                </div>
                
                <input
                  type="text"
                  maxLength={18}
                  placeholder="Name (optional) — shown when it rings"
                  value={alarmNameStr}
                  onChange={(e) => setAlarmNameStr(e.target.value)}
                  className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono"
                />

                {/* Weekday Selection Buttons */}
                <div className="flex gap-1 justify-between my-2">
                  {['S', 'M', 'T', 'W', 'T', 'F', 'S'].map((dayName, idx) => {
                    const isSelected = alarmDays.includes(idx)
                    return (
                      <button
                        key={idx}
                        type="button"
                        onClick={() => {
                          setAlarmDays(prev => 
                            prev.includes(idx) 
                              ? prev.filter(d => d !== idx) 
                              : [...prev, idx].sort()
                          )
                        }}
                        className={`w-7 h-7 rounded-full text-[10px] font-bold transition-all flex items-center justify-center cursor-pointer border ${
                          isSelected 
                            ? 'bg-[#d97756]/20 border-[#d97756]/60 text-[#f0b89a] font-bold' 
                            : 'border-ascii-mid/20 text-ascii-mid hover:border-ascii-bright/35 hover:bg-ascii-bright/5'
                        }`}
                      >
                        {dayName}
                      </button>
                    )
                  })}
                </div>
              </form>

              {/* Saved Alarms Manager */}
              <div className="space-y-2 pt-2 border-t border-ascii-mid/10">
                <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                  Alarms List
                </div>
                {alarms.length === 0 ? (
                  <div className="text-[10px] text-ascii-mid italic">No alarms saved</div>
                ) : (
                  <div className="space-y-1.5 max-h-36 overflow-y-auto pr-1 scrollbar-none">
                    {alarms.map((alarm, idx) => (
                      <div key={idx} className="flex justify-between items-center bg-ascii-dim/5 border border-ascii-mid/10 rounded-lg p-2 text-[11px]">
                        <div className="flex flex-col gap-0.5">
                          <span className="font-bold text-ascii-spark font-mono">
                            🔔 {alarm.time}
                            {alarm.name && (
                              <span className="ml-1.5 text-ascii-bright/80 font-sans font-normal not-italic">— {alarm.name}</span>
                            )}
                          </span>
                          <span className="text-[9px] text-ascii-mid font-mono font-medium">
                            {formatAlarmDays(alarm.days)}
                          </span>
                        </div>
                        <div className="flex gap-2 items-center">
                          {/* Edit button */}
                          <button
                            onClick={() => {
                              const [hh, mm] = alarm.time.split(':')
                              setAlarmHourStr(hh)
                              setAlarmMinuteStr(mm)
                              setAlarmNameStr(alarm.name || '')
                              setAlarmDays(alarm.days || [0, 1, 2, 3, 4, 5, 6])
                              setEditingAlarmIndex(idx)
                            }}
                            className="text-ascii-mid hover:text-ascii-spark cursor-pointer font-sans text-xs"
                            title="Edit alarm"
                          >
                            ✏️
                          </button>
                          {/* Toggle switch */}
                          <button
                            onClick={() => {
                              setAlarms(prev => prev.map((a, i) => i === idx ? { ...a, enabled: !a.enabled } : a))
                            }}
                            className={`px-1.5 py-0.5 rounded text-[9px] font-sans font-medium cursor-pointer transition-all ${
                              alarm.enabled
                                ? 'bg-emerald-950/20 border border-emerald-900/30 text-emerald-400 font-bold'
                                : 'bg-ascii-dim/20 border border-ascii-mid/25 text-ascii-mid'
                            }`}
                          >
                            {alarm.enabled ? 'Enabled' : 'Disabled'}
                          </button>
                          {/* Delete button */}
                          <button
                            onClick={() => {
                              setAlarms(prev => prev.filter((_, i) => i !== idx))
                            }}
                            className="text-red-400/80 hover:text-red-400 cursor-pointer font-sans text-xs"
                            title="Delete alarm"
                          >
                            🗑️
                          </button>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </div>

              {/* Alarm presets */}
              <div className="space-y-2 pt-2 border-t border-ascii-mid/10">
                <div className="text-[9px] text-ascii-mid uppercase font-bold tracking-wider">Quick Presets (Relative)</div>
                <div className="grid grid-cols-4 gap-1.5">
                  {[5, 15, 30, 60].map((mins) => {
                    const getPresetTime = (offsetMins) => {
                      const target = new Date(Date.now() + offsetMins * 60000)
                      const hh = String(target.getHours()).padStart(2, '0')
                      const mm = String(target.getMinutes()).padStart(2, '0')
                      return `${hh}${mm}`
                    }
                    return (
                      <button
                        key={mins}
                        onClick={() => onWriteSerial(`r${getPresetTime(mins)}\n`)}
                        className="bg-ascii-dim/20 hover:bg-ascii-bright/15 border border-ascii-mid/20 hover:border-ascii-bright/40 text-[10px] py-1.5 rounded-lg text-center cursor-pointer transition-all text-ascii-bright hover:text-ascii-spark"
                      >
                        +{mins}m
                      </button>
                    )
                  })}
                </div>
              </div>

              {activeAlarm && (
                <button
                  onClick={() => onWriteSerial('a')}
                  className="w-full bg-red-950/20 hover:bg-red-950/40 border border-red-900/40 text-red-400 font-semibold py-1.5 rounded transition-all cursor-pointer text-center"
                >
                  Cancel Active Alarm
                </button>
              )}
            </div>
          )}

          {clockTab === 'timer' && (
            <div className="space-y-4">
              {activeTimer && (
                <div className="bg-emerald-950/20 border border-emerald-500/30 rounded p-2.5 text-emerald-400 text-center font-bold">
                  ⏱️ Countdown Active
                  {activeTimer.name && (
                    <div className="text-[11px] text-ascii-bright/80 font-sans font-normal">{activeTimer.name}</div>
                  )}
                  <div className="text-xl font-bold font-mono tracking-widest text-ascii-spark mt-1 animate-pulse">
                    {timerTimeLeft || '0:00'}
                  </div>
                </div>
              )}

              <form
                onSubmit={(e) => {
                  e.preventDefault()
                  const totalSecs = (parseInt(timerMin, 10) || 0) * 60 + (parseInt(timerSec, 10) || 0)
                  if (totalSecs > 0) {
                    const nm = timerNameStr.trim()
                    onWriteSerial(`y${totalSecs}${nm ? ' ' + nm : ''}\n`)
                    setTimerMin('')
                    setTimerSec('')
                    setTimerNameStr('')
                  } else {
                    alert('Please enter a duration larger than 0')
                  }
                }}
                className="space-y-2"
              >
                <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                  Set Countdown Timer
                </div>
                <div className="flex gap-2 items-center">
                  <input
                    type="number"
                    min={0}
                    placeholder="Min"
                    value={timerMin}
                    onChange={(e) => setTimerMin(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2 py-1 text-ascii-spark outline-none font-mono"
                  />
                  <span>:</span>
                  <input
                    type="number"
                    min={0}
                    max={59}
                    placeholder="Sec"
                    value={timerSec}
                    onChange={(e) => setTimerSec(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2 py-1 text-ascii-spark outline-none font-mono"
                  />
                  <button
                    type="submit"
                    className="bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark px-3 py-1 rounded cursor-pointer font-bold"
                  >
                    Start
                  </button>
                </div>
                <input
                  type="text"
                  maxLength={18}
                  placeholder="Name (optional) — shown when it rings"
                  value={timerNameStr}
                  onChange={(e) => setTimerNameStr(e.target.value)}
                  className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2 py-1 text-ascii-spark outline-none font-mono"
                />
              </form>

              {/* Timer presets */}
              <div className="space-y-1">
                <div className="text-[9px] text-ascii-mid uppercase font-bold tracking-wider">Presets</div>
                <div className="grid grid-cols-5 gap-1">
                  {[
                    { label: '30s', sec: 30 },
                    { label: '1m', sec: 60 },
                    { label: '2m', sec: 120 },
                    { label: '5m', sec: 300 },
                    { label: '10m', sec: 600 },
                  ].map((preset) => (
                    <button
                      key={preset.label}
                      onClick={() => onWriteSerial(`y${preset.sec}\n`)}
                      className="bg-ascii-dim/10 hover:bg-ascii-bright/10 border border-ascii-mid/20 hover:border-ascii-bright/35 text-[10px] py-1 rounded text-center cursor-pointer transition-all"
                    >
                      {preset.label}
                    </button>
                  ))}
                </div>
              </div>

              {/* Cancel timer */}
              <button
                onClick={() => onWriteSerial('q')}
                className="w-full bg-red-950/20 hover:bg-red-950/40 border border-red-900/40 text-red-400 font-semibold py-1.5 rounded transition-all cursor-pointer text-center"
              >
                Cancel Active Timer
              </button>
            </div>
          )}
        </div>
      )}

      {activeMode === 'pomodoro' && (
        <div className="space-y-4">
          {/* Active Pomodoro status */}
          <div className={`border rounded-xl p-4 text-center ${
            isPomoActive
              ? 'bg-emerald-950/20 border-emerald-500/30 text-emerald-400 font-bold'
              : 'bg-ascii-dim/5 border-ascii-mid/15 text-ascii-mid'
          }`}>
            <span className="text-xs uppercase tracking-wider font-sans">
              Status: {isPomoActive ? '🍅 Focus Active' : '⏱️ Idle'}
            </span>
            <div className="text-[10px] text-ascii-mid mt-1 font-sans font-normal">
              {isPomoActive
                ? 'Countdown is ticking down on the ESP32 screen'
                : 'Configure durations below and click Start'}
            </div>
          </div>

          {/* Focus & Break durations — typing here only updates local state;
              nothing is sent to the device until Start is pressed below. */}
          {!isPomoActive && (
            <div className="space-y-2">
              <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                Durations
              </div>
              <div className="flex gap-2 items-end">
                <div className="flex flex-col gap-1 w-14">
                  <span className="text-[9px] text-ascii-mid uppercase">Focus min</span>
                  <input
                    type="number"
                    min={0}
                    max={99}
                    placeholder="25"
                    value={pomoFocusStr}
                    onChange={(e) => setPomoFocusStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono text-xs"
                  />
                </div>
                <span className="mb-2 text-ascii-mid">:</span>
                <div className="flex flex-col gap-1 w-14">
                  <span className="text-[9px] text-ascii-mid uppercase">sec</span>
                  <input
                    type="number"
                    min={0}
                    max={59}
                    placeholder="0"
                    value={pomoFocusSecStr}
                    onChange={(e) => setPomoFocusSecStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono text-xs"
                  />
                </div>
                <div className="flex flex-col gap-1 w-14 ml-auto">
                  <span className="text-[9px] text-ascii-mid uppercase">Break min</span>
                  <input
                    type="number"
                    min={1}
                    max={9}
                    placeholder="5"
                    value={pomoBreakStr}
                    onChange={(e) => setPomoBreakStr(e.target.value)}
                    className="w-full bg-room-bg-deep border border-ascii-mid/30 focus:border-ascii-bright/60 rounded px-2.5 py-1.5 text-ascii-spark outline-none font-mono text-xs"
                  />
                </div>
              </div>
            </div>
          )}

          {/* Start/Stop Button */}
          <button
            onClick={handleStartStopPomo}
            className={`w-full py-2.5 rounded-xl transition-all cursor-pointer text-center font-bold border ${
              isPomoActive
                ? 'bg-red-950/20 hover:bg-red-950/40 border-red-900/40 text-red-400'
                : 'bg-emerald-950/25 hover:bg-emerald-900/30 border-emerald-500/30 text-emerald-400 shadow-[0_0_12px_rgba(52,211,153,0.1)]'
            }`}
          >
            {isPomoActive ? 'Stop Pomodoro' : 'Start Pomodoro'}
          </button>
        </div>
      )}

      {activeMode === 'weather' && (
        <div className="space-y-4">
          <div className="bg-room-bg-deep border border-ascii-mid/30 rounded-xl p-4 text-center">
            <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider mb-1">Live Weather</div>
            
            {/* Active Location Info */}
            <div className="text-sm font-bold font-mono text-ascii-spark mb-1">
              {weatherData?.cityName || selectedLocation.name}
            </div>
            {selectedLocation.country && (
              <div className="text-[9px] text-ascii-dim font-mono mb-3">
                {selectedLocation.admin1 ? `${selectedLocation.admin1}, ` : ''}{selectedLocation.country}
              </div>
            )}
            
            {weatherData ? (
              <div className="mt-3 space-y-2 border-t border-ascii-mid/10 pt-3 text-left font-mono text-[11px] text-ascii-bright">
                <div className="flex justify-between">
                  <span>Temperature:</span>
                  <span className="text-ascii-spark font-bold">{weatherData.temp}°C</span>
                </div>
                <div className="flex justify-between">
                  <span>Feels Like:</span>
                  <span className="text-ascii-spark">{weatherData.feels}°C</span>
                </div>
                <div className="flex justify-between">
                  <span>Humidity:</span>
                  <span className="text-ascii-spark">{weatherData.humidity}%</span>
                </div>
                <div className="flex justify-between text-[#e8a98a]">
                  <span>Condition:</span>
                  <span className="font-bold">{weatherData.condition}</span>
                </div>
              </div>
            ) : (
              <div className="text-[10px] text-ascii-dim mt-2 italic">
                No local weather sync data yet.
              </div>
            )}
            
            <button
              onClick={onRefreshWeather}
              className="mt-3 w-full bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark font-semibold py-1.5 rounded transition-all cursor-pointer text-center text-[10px]"
            >
              Refresh & Sync
            </button>
          </div>

          {/* Search Box & Suggestions */}
          <div className="bg-room-bg-deep border border-ascii-mid/30 rounded-xl p-4">
            <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider mb-2">Change Location</div>
            <div className="relative">
              <input
                type="text"
                placeholder="Search city (e.g. London, Paris...)"
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-room-bg/50 border border-ascii-mid/30 rounded px-2 py-1.5 text-ascii-bright font-mono text-xs focus:outline-none focus:border-ascii-spark placeholder-ascii-dim"
              />
              {isSearching && (
                <span className="absolute right-2.5 top-2.5 flex h-3.5 w-3.5 items-center justify-center">
                  <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-ascii-spark opacity-75"></span>
                  <span className="relative inline-flex rounded-full h-2 w-2 bg-ascii-spark"></span>
                </span>
              )}
            </div>

            {/* Search Results Dropdown List */}
            {searchResults.length > 0 && (
              <div className="mt-2 border border-ascii-mid/20 rounded bg-room-bg-deep overflow-hidden divide-y divide-ascii-mid/10 max-h-[150px] overflow-y-auto z-30 font-mono text-[11px]">
                {searchResults.map((result, idx) => (
                  <button
                    key={idx}
                    onClick={() => {
                      onLocationChange(result);
                      setSearchQuery('');
                      setSearchResults([]);
                    }}
                    className="w-full text-left px-3 py-2 text-ascii-bright hover:bg-ascii-bright/10 hover:text-ascii-spark transition-colors block"
                  >
                    <div className="font-semibold">{result.name}</div>
                    <div className="text-[9px] text-ascii-dim">
                      {result.admin1 ? `${result.admin1}, ` : ''}{result.country}
                    </div>
                  </button>
                ))}
              </div>
            )}

            {/* Recent Locations */}
            {recentLocations.length > 0 && (
              <div className="mt-4 pt-3 border-t border-ascii-mid/10">
                <div className="text-[9px] text-ascii-dim uppercase font-bold tracking-wider mb-2">Recent Locations</div>
                <div className="flex flex-wrap gap-1.5">
                  {recentLocations.map((loc, idx) => (
                    <div
                      key={idx}
                      className="group relative"
                    >
                      <button
                        onClick={() => onLocationChange(loc)}
                        className={`text-[9px] font-mono px-2 py-1 rounded border transition-all cursor-pointer ${
                          selectedLocation.lat === loc.lat && selectedLocation.lon === loc.lon
                            ? 'border-ascii-spark/60 text-ascii-spark bg-ascii-spark/10 font-bold'
                            : 'border-ascii-mid/20 text-ascii-bright hover:border-ascii-bright/35 hover:bg-ascii-bright/5'
                        }`}
                      >
                        {loc.name}
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          e.preventDefault();
                          onDeleteRecentLocation(loc);
                        }}
                        className="absolute -top-1.5 -right-1.5 w-3.5 h-3.5 rounded-full bg-room-bg border border-ascii-mid/30 hover:bg-ascii-bright/10 hover:border-ascii-spark/60 text-ascii-dim hover:text-ascii-spark text-[8px] flex items-center justify-center opacity-0 group-hover:opacity-100 transition-all cursor-pointer shadow-md select-none leading-none"
                        title="Remove location"
                      >
                        ×
                      </button>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>

          <div className="text-[10px] text-ascii-mid leading-relaxed text-center italic">
            This mode displays live temperature, feels-like temperature, humidity, and retro weather animations (clear, cloudy, fog, rain, storm, snowy, windy) on the device.
          </div>
        </div>
      )}

      {activeMode === 'sing' && (
        <div className="space-y-4">
          {/* Now playing banner */}
          <div className={`border rounded-xl p-3 text-center transition-all ${
            singPlaying
              ? 'bg-emerald-950/20 border-emerald-500/30 text-emerald-400'
              : 'bg-ascii-dim/5 border-ascii-mid/15 text-ascii-mid'
          }`}>
            <div className="text-[9px] uppercase tracking-wider font-bold font-sans">
              {singPlaying ? '🎵 Now Playing' : '⏸ Paused'}
            </div>
            <div className="text-sm font-bold font-mono text-ascii-spark mt-0.5">
              {SONGS.find((s) => s.key === singSong)?.label || 'Pick a tune'}
            </div>
          </div>

          {/* Song picker — sends the firmware's single-char song key */}
          <div className="space-y-2">
            <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">Songs</div>
            <div className="grid grid-cols-1 gap-2">
              {SONGS.map((song) => {
                const sel = singSong === song.key
                return (
                  <button
                    key={song.key}
                    onClick={() => {
                      // '7' guarantees the device is in Sing mode before the
                      // song key lands (handleSingKey only runs in MODE_SING).
                      onWriteSerial('7' + song.key)
                      setSingSong(song.key)
                      setSingPlaying(true)
                    }}
                    className={`flex items-center gap-3 px-3 py-2 rounded-xl border transition-all cursor-pointer text-left ${
                      sel
                        ? 'bg-ascii-spark/10 border-ascii-spark/60 text-ascii-spark font-bold'
                        : 'bg-ascii-dim/5 border-ascii-mid/15 text-ascii-bright hover:border-ascii-bright/40 hover:bg-ascii-bright/10'
                    }`}
                  >
                    <span className="text-lg leading-none">{song.emoji}</span>
                    <span className="text-xs font-sans">{song.label}</span>
                    {sel && singPlaying && (
                      <span className="ml-auto text-[9px] text-emerald-400 font-sans animate-pulse">▶ playing</span>
                    )}
                  </button>
                )
              })}
            </div>
          </div>

          {/* Transport controls */}
          <div className="grid grid-cols-3 gap-2">
            <button
              onClick={() => {
                onWriteSerial(' ')
                setSingPlaying((p) => {
                  if (!p && !singSong) setSingSong('q')
                  return !p
                })
              }}
              className="bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark font-semibold py-2 rounded transition-all cursor-pointer text-center"
            >
              {singPlaying ? '⏸ Pause' : '▶ Play'}
            </button>
            <button
              onClick={() => {
                onWriteSerial('x')
                setSingPlaying(false)
              }}
              className="bg-red-950/20 hover:bg-red-950/40 border border-red-900/40 text-red-400 font-semibold py-2 rounded transition-all cursor-pointer text-center"
            >
              ⏹ Stop
            </button>
            <button
              onClick={() => {
                onWriteSerial('m')
                setSingLoop((l) => !l)
              }}
              className={`py-2 rounded border font-semibold transition-all cursor-pointer text-center ${
                singLoop
                  ? 'bg-ascii-spark/10 border-ascii-spark/60 text-ascii-spark'
                  : 'bg-ascii-dim/5 border-ascii-mid/20 text-ascii-mid hover:text-ascii-bright'
              }`}
            >
              🔁 Loop {singLoop ? 'On' : 'Off'}
            </button>
          </div>

          <div className="text-[10px] text-ascii-mid leading-relaxed text-center italic">
            Clawd plays public-domain tunes on the buzzer. Playback runs on the device — leaving Sing mode stops the music.
          </div>
        </div>
      )}
    </div>
  )
}
