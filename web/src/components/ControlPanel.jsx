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
  { label: 'Anthropic Logo', cmd: 'z', face: ' ⬡ ', desc: 'Draw Anthropic logo line by line' },
]

export default function ControlPanel({
  activeMode,
  onClose,
  onWriteSerial,
  isConnected,
  activeAlarm,
  activeTimer,
  isPomoActive,
  usage,
  alarms = [],
  setAlarms,
}) {
  const [alarmHourStr, setAlarmHourStr] = useState('')
  const [alarmMinuteStr, setAlarmMinuteStr] = useState('')
  const [clockHourStr, setClockHourStr] = useState('')
  const [clockMinuteStr, setClockMinuteStr] = useState('')
  const [terminalInput, setTerminalInput] = useState('')
  const [clockTab, setClockTab] = useState('clock') // 'clock' | 'alarm' | 'timer'
  const [timerMin, setTimerMin] = useState('')
  const [timerSec, setTimerSec] = useState('')
  const [pomoFocusStr, setPomoFocusStr] = useState('25')
  const [pomoFocusSecStr, setPomoFocusSecStr] = useState('0')
  const [pomoBreakStr, setPomoBreakStr] = useState('5')
  const [alarmTimeLeft, setAlarmTimeLeft] = useState('')
  const [timerTimeLeft, setTimerTimeLeft] = useState('')
  const termInputRef = useRef(null)
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

  if (!activeMode) return null

  // Helper to sync time
  const handleSyncTime = () => {
    const now = new Date()
    const hh = String(now.getHours()).padStart(2, '0')
    const mm = String(now.getMinutes()).padStart(2, '0')
    onWriteSerial(`t${hh}${mm}\n`)
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
          {/* Claude Vibe — a one-shot trigger, not a toggle switch; pressing
              it again just pulses the idle-cycle command again, so there's
              no persistent "on" look to maintain here. */}
          <button
            onClick={() => onWriteSerial('m')}
            className={`w-full py-4 px-4 rounded-xl cursor-pointer transition-all duration-300 active:scale-[0.98] flex items-center justify-center group relative overflow-hidden border bg-transparent ${
              activeTimer || activeAlarm
                ? 'opacity-60 cursor-not-allowed border-ascii-mid/10'
                : 'border-[#d97756]/50 text-[#e8a98a] hover:border-[#d97756]/80 hover:text-[#f0b89a] hover:shadow-[0_0_18px_rgba(217,119,86,0.2)]'
            }`}
            disabled={!!activeTimer || !!activeAlarm}
            title={activeTimer || activeAlarm ? "Cannot vibe while timer/alarm is active" : "Trigger an idle-cycle pulse"}
          >
            <span className="text-lg font-serif-display italic font-medium tracking-wide">
              Let Claude Vibe
            </span>
          </button>

          {/* Secondary configuration (Backlight & speed control) */}
          <div className="flex gap-2 justify-between items-center text-[10px] bg-ascii-dim/10 border border-ascii-mid/10 rounded-lg px-2.5 py-1.5">
            <span className="text-ascii-mid font-semibold select-none font-sans">Screen Config</span>
            <div className="flex gap-1.5">
              <button
                onClick={() => onWriteSerial('b')}
                className="bg-ascii-dim/20 hover:bg-ascii-bright/10 border border-ascii-mid/20 hover:border-ascii-bright/40 text-ascii-bright px-2 py-0.5 rounded cursor-pointer transition-all text-[9px] font-sans font-medium"
                title="Toggle display backlight"
              >
                💡 Light
              </button>
              <button
                onClick={() => onWriteSerial('-')}
                className="bg-ascii-dim/20 hover:bg-ascii-bright/10 border border-ascii-mid/20 hover:border-ascii-bright/40 text-ascii-bright px-2 py-0.5 rounded cursor-pointer transition-all text-[9px] font-sans font-medium"
                title="Slow down animations"
              >
                🐢 Slower
              </button>
              <button
                onClick={() => onWriteSerial('=')}
                className="bg-ascii-dim/20 hover:bg-ascii-bright/10 border border-ascii-mid/20 hover:border-ascii-bright/40 text-ascii-bright px-2 py-0.5 rounded cursor-pointer transition-all text-[9px] font-sans font-medium"
                title="Speed up animations"
              >
                🐇 Faster
              </button>
            </div>
          </div>

          {/* Animations grid */}
          <div className="h-64 overflow-y-auto scrollbar-none pr-0.5">
            <div className="grid grid-cols-2 gap-2">
              {EXPRESSIONS.map((exp) => (
                <button
                  key={exp.cmd}
                  onClick={() => onWriteSerial(exp.cmd)}
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
                  onWriteSerial(`r${hh}${mm}\n`)
                  setAlarmHourStr('')
                  setAlarmMinuteStr('')
                }}
                className="space-y-2"
              >
                <div className="text-[10px] text-ascii-mid uppercase font-bold tracking-wider">
                  Add Alarm Time
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
                  <button
                    type="submit"
                    className="bg-ascii-bright/10 hover:bg-ascii-bright/25 border border-ascii-mid/30 text-ascii-spark px-3 py-1 rounded cursor-pointer font-bold"
                  >
                    Add
                  </button>
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
                        <span className="font-bold text-ascii-spark font-mono">
                          🔔 {alarm.time}
                        </span>
                        <div className="flex gap-2 items-center">
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
                    onWriteSerial(`y${totalSecs}\n`)
                    setTimerMin('')
                    setTimerSec('')
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
    </div>
  )
}
