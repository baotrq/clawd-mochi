import { useEffect, useRef, useState } from 'react'

// The background video is 16:9; we reason in this virtual frame and map to the
// screen with the same object-cover transform the <video> uses, so Clawd stays
// pinned to the scene at any window size.
const VIDEO_W = 1280
const VIDEO_H = 720

// --- persisted tuning (written by ?tune mode, loaded here on the main site) ---
const STORAGE_KEY = 'clawd-tune'
function loadTune() {
  if (typeof localStorage === 'undefined') return null
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY) || 'null')
  } catch {
    return null
  }
}
const SAVED = loadTune()

// --- where Clawd lives in the scene (normalized 0..1 of the frame) ---
// He sits at the chair (left of the desk) facing right toward the screen.
// Defaults are the values dialed in via ?tune; localStorage overrides them.
const DESK = {
  x: SAVED?.x ?? 0.396, // seated-at-chair anchor (bottom-center)
  baseY: SAVED?.baseY ?? 0.606,
}
// The single ground line: the horizontal where Clawd's feet rest on the floor.
// Walking, the get-off landing, and the hop on/off the chair all align to it.
const GROUND_Y = SAVED?.ground ?? 0.76
const WALK_MIN = 0.14 // left edge of the walkable band
const WALK_MAX = 0.84 // right edge

// --- the real extracted frame sets (web/public/clawd/<dir>/<dir>_NN.png) ---
// get_off_chair plays forward to step OFF the chair (down); jumping_up plays
// forward to hop back ONTO the chair (up).
const base = import.meta.env.BASE_URL
// Every frame in a set is drawn at the SAME scale (native px × one factor), so
// frames stay consistent with each other — a taller frame is genuinely taller
// (legs extended), not squished to a target box. `h` sets that scale: it's the
// on-screen height (fraction of the scene) of a frame whose native height is
// `refPx`; all other frames scale by the same factor.
// getoff size == walking size so the bridge frame (get_off_chair_13, 490×330)
// and walking_01 (also 490×330) render identically — no pop at the seam.
const SETS = {
  typing: { dir: 'typing', count: 15, fps: 10, h: 0.13, flip: true, refPx: 291 },
  walking: { dir: 'walking', count: 23, fps: 14, h: 0.125, flip: true, refPx: 330 },
  getoff: { dir: 'get_off_chair', count: 13, fps: 14, h: 0.125, flip: false, refPx: 330 },
  jumpup: { dir: 'jumping_up', count: 7, fps: 12, h: 0.145, flip: false, refPx: 330 },
}
// apply any saved per-animation sizes
if (SAVED?.sizes) {
  for (const k of Object.keys(SETS)) {
    if (SAVED.sizes[k] != null) SETS[k].h = SAVED.sizes[k]
  }
}
// Default per-frame nudges (the get-off keyframes that carry him chair→floor).
// dy ramps 0→0.156 over the last frames so he steps down onto the floor; the
// final frame's offset defines the floor landing spot the walk hands off from.
const DEFAULT_FRAMES = {
  getoff: [
    { dx: 0, dy: 0 },
    { dx: 0, dy: 0 },
    { dx: -0.008, dy: 0 },
    { dx: -0.024, dy: 0 },
    { dx: -0.024, dy: 0 },
    { dx: -0.028, dy: 0 },
    { dx: -0.028, dy: 0 },
    { dx: -0.028, dy: 0 },
    { dx: -0.024, dy: 0 },
    { dx: 0.02, dy: 0.08 },
    { dx: 0.052, dy: 0.156 },
    { dx: 0.052, dy: 0.156 },
    { dx: 0.052, dy: 0.156 },
  ],
}
// saved per-frame nudges/scale, keyed by set → frame index → { dx, dy, sc }
const FRAME_ADJ = SAVED?.frames ?? DEFAULT_FRAMES
// build a full per-frame adjust table for a set, merging any saved values
const makeAdjust = (k) =>
  Array.from({ length: SETS[k].count }, (_, i) => ({
    dx: 0,
    dy: 0,
    sc: 1,
    ...(FRAME_ADJ[k]?.[i] ?? {}),
  }))
const ZERO_ADJ = { dx: 0, dy: 0, sc: 1 }

function loadSet({ dir, count }) {
  return Array.from({ length: count }, (_, i) => {
    const img = new Image()
    img.src = `${base}clawd/${dir}/${dir}_${String(i + 1).padStart(2, '0')}.png`
    return img
  })
}

/** object-cover transform: virtual frame coords → screen px. */
function coverTransform(w, h) {
  const videoAspect = VIDEO_W / VIDEO_H
  const screenAspect = w / h
  if (screenAspect > videoAspect) {
    const scale = w / VIDEO_W
    return { scale, ox: 0, oy: (h - VIDEO_H * scale) / 2 }
  }
  const scale = h / VIDEO_H
  return { scale, ox: (w - VIDEO_W * scale) / 2, oy: 0 }
}

const rand = (a, b) => a + Math.random() * (b - a)
const lerp = (a, b, t) => a + (b - a) * t

// Tuning mode: open the page with ?tune to place/size Clawd, then Save (which
// stores the values and returns to the main site). No effect in normal use.
const TUNE =
  typeof window !== 'undefined' &&
  new URLSearchParams(window.location.search).has('tune')

/**
 * Clawd — the real pixel-art mascot composited onto the room video.
 *
 * Behaviour loop: types at the desk → steps off the chair down to the floor
 * (get_off_chair) → wanders → walks back and hops up onto the chair
 * (jumping_up) to sit and type again. Click the scene to send him walking
 * there. window.__clawdWalkTo is exposed for later firmware-driven control.
 */
export default function Clawd() {
  const canvasRef = useRef(null)
  const frames = useRef(null)
  const state = useRef({
    x: DESK.x,
    baseY: DESK.baseY,
    targetBaseY: DESK.baseY,
    facing: 1,
    mode: 'typing', // typing | leaving | walk | idle | return | arriving
    target: null, // walk destination; null = pick a random wander spot
    wanders: 0,
    t: 0, // ms elapsed overall (for bob)
    until: 0, // when the current dwell ends (typing/idle)
    fi: 0, // frame index (float)
    jumpFrom: DESK.baseY, // baseY a transition started from (for the arc)
    jumpFromX: DESK.x, // x a transition started from (for the forward step)
  })

  // --- tune-mode controls (only used when the page is opened with ?tune) ---
  const [tuneKey, setTuneKey] = useState('typing') // which animation to preview
  const [sizes, setSizes] = useState(() =>
    Object.fromEntries(Object.entries(SETS).map(([k, v]) => [k, v.h])),
  )
  const [placed, setPlaced] = useState({ x: DESK.x, baseY: DESK.baseY })
  const [adjust, setAdjust] = useState(() =>
    Object.fromEntries(Object.keys(SETS).map((k) => [k, makeAdjust(k)])),
  )
  const [kfEdit, setKfEdit] = useState(false) // step frame-by-frame?
  const [kf, setKf] = useState(0) // which frame is being edited
  const [groundY, setGroundY] = useState(GROUND_Y) // the floor line
  // mirror into a ref so the rAF loop reads the latest without re-subscribing
  const tuneRef = useRef()
  tuneRef.current = { key: tuneKey, sizes, adjust, kfEdit, kf, ground: groundY }

  const bumpSize = (d) =>
    setSizes((s) => ({
      ...s,
      [tuneKey]: Math.max(0.02, +(s[tuneKey] + d).toFixed(4)),
    }))
  // per-frame editors (operate on the currently-selected frame `kf`)
  const editFrame = (fn) =>
    setAdjust((a) => ({
      ...a,
      [tuneKey]: a[tuneKey].map((f, i) => (i === kf ? fn({ ...f }) : f)),
    }))
  const nudge = (dx, dy) =>
    editFrame((f) => ({ ...f, dx: +(f.dx + dx).toFixed(4), dy: +(f.dy + dy).toFixed(4) }))
  const bumpFrameScale = (d) =>
    editFrame((f) => ({ ...f, sc: Math.max(0.2, +(f.sc + d).toFixed(3)) }))
  const resetFrame = () => editFrame(() => ({ dx: 0, dy: 0, sc: 1 }))
  const stepFrame = (d) =>
    setKf((k) => Math.max(0, Math.min(SETS[tuneKey].count - 1, k + d)))

  const tuneConfig = () => ({
    x: placed.x,
    baseY: placed.baseY,
    ground: groundY,
    sizes,
    frames: adjust,
  })
  const saveTune = () => {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(tuneConfig()))
    const url = new URL(window.location.href)
    url.searchParams.delete('tune')
    window.location.href = url.toString() // reload the main site with the values
  }
  const copyConfig = () => {
    const json = JSON.stringify(tuneConfig())
    navigator.clipboard?.writeText(json)
    console.log('[clawd tune] config:', json) // also logged so it's easy to grab
  }
  const clearSaved = () => {
    localStorage.removeItem(STORAGE_KEY) // fall back to the baked-in defaults
    window.location.reload()
  }

  // keyboard nudging while editing keyframes (arrows = move, [ ] = frame, - = scale)
  useEffect(() => {
    if (!TUNE || !kfEdit) return
    const onKey = (e) => {
      const step = e.shiftKey ? 0.01 : 0.002
      const map = {
        ArrowLeft: () => nudge(-step, 0),
        ArrowRight: () => nudge(step, 0),
        ArrowUp: () => nudge(0, -step),
        ArrowDown: () => nudge(0, step),
        '[': () => stepFrame(-1),
        ']': () => stepFrame(1),
        '-': () => bumpFrameScale(-0.02),
        '=': () => bumpFrameScale(0.02),
      }
      if (map[e.key]) {
        map[e.key]()
        e.preventDefault()
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [kfEdit, tuneKey, kf]) // eslint-disable-line react-hooks/exhaustive-deps
  const openTune = () => {
    const url = new URL(window.location.href)
    url.searchParams.set('tune', '1')
    window.location.href = url.toString()
  }

  useEffect(() => {
    frames.current = {
      typing: loadSet(SETS.typing),
      walking: loadSet(SETS.walking),
      getoff: loadSet(SETS.getoff),
      jumpup: loadSet(SETS.jumpup),
    }

    const canvas = canvasRef.current
    const ctx = canvas.getContext('2d')
    const s = state.current
    let raf
    let last = performance.now()
    // In tune mode he stays put at the desk so you can line up the chair.
    s.until = TUNE ? Infinity : last + rand(6000, 12000)
    let readout = `x: ${DESK.x.toFixed(3)}, baseY: ${DESK.baseY.toFixed(3)}`

    const resize = () => {
      const dpr = Math.min(window.devicePixelRatio || 1, 2)
      canvas.width = Math.round(canvas.clientWidth * dpr)
      canvas.height = Math.round(canvas.clientHeight * dpr)
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
      ctx.imageSmoothingEnabled = false
    }
    resize()
    const ro = new ResizeObserver(resize)
    ro.observe(canvas)

    const startWander = () => {
      s.target = rand(WALK_MIN, WALK_MAX)
      s.targetBaseY = GROUND_Y
      s.mode = 'walk'
      s.fi = 0
    }
    // Where the get-off animation visually leaves him: the chair anchor plus
    // the LAST get-off keyframe offset (the motion lives in those keyframes).
    const landingSpot = () => {
      const g = tuneRef.current.adjust.getoff
      const last = g[g.length - 1]
      // x comes from the last keyframe; feet land on the shared ground line
      return { x: DESK.x + last.dx, y: GROUND_Y }
    }
    const goToDesk = () => {
      // walk to the get-off landing spot, then hop back up onto the chair
      s.target = landingSpot().x
      s.targetBaseY = GROUND_Y
      s.mode = 'return'
      s.fi = 0
    }
    // At the desk → play get_off_chair. The descent is baked into the per-frame
    // keyframes, so we DON'T add an arc here — anchor stays at the chair and the
    // offsets carry him to the floor.
    const leaveDesk = () => {
      s.mode = 'leaving'
      s.fi = 0
    }

    // --- public control handle (used later by the Serial/WiFi bridge) ---
    window.__clawdWalkTo = (normX) => {
      s.target = Math.max(WALK_MIN, Math.min(WALK_MAX, normX))
      if (s.mode === 'typing') leaveDesk() // step off the chair, then walk there
      else if (s.mode !== 'leaving' && s.mode !== 'arriving') {
        s.targetBaseY = GROUND_Y
        s.mode = 'walk'
        s.fi = 0
      }
    }

    const onClick = (e) => {
      const rect = canvas.getBoundingClientRect()
      const { scale, ox, oy } = coverTransform(rect.width, rect.height)
      const nx = (e.clientX - rect.left - ox) / (VIDEO_W * scale)
      const ny = (e.clientY - rect.top - oy) / (VIDEO_H * scale)
      if (TUNE) {
        // drop Clawd's feet right where you clicked and remember it for Save
        s.x = nx
        s.baseY = ny
        s.targetBaseY = ny
        setPlaced({ x: nx, baseY: ny })
        readout = `x: ${nx.toFixed(3)}, baseY: ${ny.toFixed(3)}`
        return
      }
      // Click near the chair → go sit on it; anywhere else → walk there.
      if (Math.abs(nx - DESK.x) < 0.1) {
        if (s.mode === 'walk' || s.mode === 'idle') goToDesk()
      } else {
        window.__clawdWalkTo(nx)
      }
    }
    canvas.addEventListener('click', onClick)

    const frame = (now) => {
      // Clamp dt: when the tab is backgrounded, requestAnimationFrame pauses and
      // `now` jumps by the whole away-time on return. Un-clamped, that single
      // huge dt teleports Clawd far off-screen (s.x += speed * dt) and he only
      // crawls back slowly — looking "gone" until you refresh. Capping at ~100ms
      // makes the loop simply resume from where it left off.
      const dt = Math.min(now - last, 100)
      last = now
      s.t += dt
      const w = canvas.clientWidth
      const h = canvas.clientHeight
      const { scale, ox, oy } = coverTransform(w, h)
      const F = frames.current

      // ---------------- behaviour ----------------
      // In tune mode we don't run the wander loop — just play whichever
      // animation the panel selected, in place, so it can be sized/placed.
      if (TUNE) {
        // hold still on the selected frame while editing keyframes
        if (!tuneRef.current.kfEdit)
          s.fi += (dt / 1000) * SETS[tuneRef.current.key].fps
      } else
        switch (s.mode) {
          case 'typing':
            s.fi += (dt / 1000) * SETS.typing.fps // keep the keys clacking
            if (now > s.until) leaveDesk() // time to get up and wander
            break
          case 'leaving': {
            // Anchor stays at the chair; the get-off keyframe offsets carry him
            // down to the floor. When the clip ends, snap the anchor to the
            // landing spot the offsets drew him at, so the walk continues from
            // exactly there with no jump.
            s.fi += (dt / 1000) * SETS.getoff.fps
            if (s.fi >= SETS.getoff.count) {
              const land = landingSpot()
              s.x = land.x
              s.baseY = land.y
              if (s.target == null) startWander()
              else {
                s.targetBaseY = GROUND_Y
                s.mode = 'walk'
                s.fi = 0
              }
            }
            break
          }
          case 'walk':
          case 'return': {
            const speed = 0.16 // normalized units / sec
            const dir = Math.sign(s.target - s.x)
            if (dir !== 0) s.facing = dir
            s.x += dir * speed * (dt / 1000)
            s.fi += (dt / 1000) * SETS.walking.fps
            if (Math.abs(s.target - s.x) < 0.006) {
              s.x = s.target
              if (s.mode === 'return') {
                // arrived in front of the chair → hop up and back onto it
                s.mode = 'arriving'
                s.facing = 1
                s.jumpFrom = s.baseY
                s.jumpFromX = s.x
              } else {
                s.mode = 'idle'
                s.until = now + rand(1500, 4000)
              }
              s.fi = 0
            }
            break
          }
          case 'idle':
            if (now > s.until) {
              s.wanders += 1
              if (s.wanders >= 3 || Math.random() < 0.45) goToDesk()
              else startWander()
            }
            break
          case 'arriving': {
            // hop up onto the chair (jumping_up) and settle into typing.
            s.fi += (dt / 1000) * SETS.jumpup.fps
            if (s.fi >= SETS.jumpup.count) {
              s.mode = 'typing'
              s.x = DESK.x // settled back on the seat
              s.baseY = DESK.baseY
              s.targetBaseY = DESK.baseY
              s.until = now + rand(7000, 13000)
              s.wanders = 0
              s.target = null
              s.fi = 0
            }
            break
          }
          default:
            break
        }

      // ease the vertical anchor toward its target (smooths desk ⇄ floor)
      s.baseY = lerp(s.baseY, s.targetBaseY, Math.min(1, dt / 160))
      // 'leaving' has no arc — its motion is in the keyframe offsets. Only the
      // hop back UP onto the chair is driven here (jumping_up isn't keyframed).
      if (s.mode === 'arriving') {
        const p = Math.min(1, s.fi / SETS.jumpup.count)
        s.baseY =
          s.jumpFrom + (DESK.baseY - s.jumpFrom) * p - 0.05 * Math.sin(Math.PI * p)
        s.x = s.jumpFromX + (DESK.x - s.jumpFromX) * p
      }

      // ---------------- pick current frame ----------------
      let setKey = 'typing'
      if (TUNE) setKey = tuneRef.current.key
      else if (s.mode === 'leaving') setKey = 'getoff'
      else if (s.mode === 'arriving') setKey = 'jumpup'
      else if (s.mode === 'walk' || s.mode === 'return' || s.mode === 'idle')
        setKey = 'walking'
      const set = SETS[setKey]
      const imgs = F[setKey]
      const oneShot = s.mode === 'leaving' || s.mode === 'arriving'
      let idx
      if (TUNE)
        idx = tuneRef.current.kfEdit
          ? Math.min(set.count - 1, tuneRef.current.kf) // frozen on edited frame
          : Math.floor(s.fi) % set.count // loop the preview
      else if (oneShot) idx = Math.min(set.count - 1, Math.floor(s.fi))
      else if (s.mode === 'idle') idx = 0 // a settled standing frame
      else idx = Math.floor(s.fi) % set.count
      const img = imgs[idx]

      // ---------------- draw ----------------
      ctx.clearRect(0, 0, w, h)
      if (img && img.complete && img.naturalWidth) {
        const adj = tuneRef.current.adjust[setKey]?.[idx] ?? ZERO_ADJ
        const size = (TUNE ? tuneRef.current.sizes[setKey] : set.h) * adj.sc
        // one uniform factor for the whole set: native px → screen px. Every
        // frame uses it, so they stay consistent and only `size` changes them.
        const k = (size * VIDEO_H * scale) / set.refPx
        const dispW = img.naturalWidth * k
        const dispH = img.naturalHeight * k
        // Idle gets a gentle breathing bob; typing stays still (only the arm
        // moves) and walking carries its own motion.
        const bob = s.mode === 'idle' ? Math.sin(s.t / 520) * 1.5 * scale : 0
        // per-frame nudges (dx/dy) ride on top of the scene anchor
        const screenX = ox + (s.x + adj.dx) * VIDEO_W * scale
        const screenY = oy + (s.baseY + adj.dy) * VIDEO_H * scale + bob
        // Typing frames all share one height but widen on the right as the arm
        // extends — so pin the body's left edge (constant body width) instead
        // of re-centering each frame, keeping the body locked while the arm taps.
        const TYPE_BODY_W = (dispH * 409) / 291
        const drawX =
          setKey === 'typing' ? screenX - TYPE_BODY_W / 2 : screenX - dispW / 2
        const drawY = screenY - dispH

        ctx.save()
        if (set.flip && s.facing < 0) {
          ctx.translate(screenX, 0)
          ctx.scale(-1, 1)
          ctx.translate(-screenX, 0)
        }
        ctx.drawImage(img, drawX, drawY, dispW, dispH)
        ctx.restore()
      }

      // tune-mode HUD: ground line + coords + a marker under his feet
      if (TUNE) {
        // the shared ground line — align each floor frame's feet to this
        const gy = oy + tuneRef.current.ground * VIDEO_H * scale
        ctx.strokeStyle = 'rgba(90,200,255,0.6)'
        ctx.lineWidth = 1
        ctx.beginPath()
        ctx.moveTo(0, gy)
        ctx.lineTo(w, gy)
        ctx.stroke()
        ctx.fillStyle = 'rgba(90,200,255,0.9)'
        ctx.font = '12px monospace'
        ctx.fillText(`ground ${tuneRef.current.ground.toFixed(3)}`, w - 120, gy - 6)

        const screenX = ox + s.x * VIDEO_W * scale
        const screenY = oy + s.baseY * VIDEO_H * scale
        ctx.fillStyle = '#ff5a36'
        ctx.fillRect(screenX - 6, screenY - 1, 12, 2)
        ctx.fillRect(screenX - 1, screenY - 6, 2, 12)
        ctx.font = '14px monospace'
        ctx.fillStyle = '#ffd9cf'
        ctx.fillText('tune — click to place Clawd', 14, 24)
        ctx.fillText(readout, 14, 44)
        ctx.fillText(
          `${setKey} h: ${tuneRef.current.sizes[setKey].toFixed(3)}`,
          14,
          64,
        )
        if (tuneRef.current.kfEdit) {
          const a = tuneRef.current.adjust[setKey]?.[idx] ?? ZERO_ADJ
          ctx.fillText(
            `frame ${idx + 1}/${set.count}  dx ${a.dx.toFixed(3)} dy ${a.dy.toFixed(3)} sc ${a.sc.toFixed(2)}`,
            14,
            84,
          )
        }
      }

      raf = requestAnimationFrame(frame)
    }
    raf = requestAnimationFrame(frame)

    return () => {
      cancelAnimationFrame(raf)
      ro.disconnect()
      canvas.removeEventListener('click', onClick)
      delete window.__clawdWalkTo
    }
  }, [])

  return (
    <>
      <canvas
        ref={canvasRef}
        className="absolute inset-0 h-full w-full cursor-pointer"
      />

      {TUNE && (
        <div className="absolute right-4 top-4 z-20 w-44 space-y-2 rounded-lg bg-black/70 p-3 font-mono text-xs text-orange-100 backdrop-blur">
          <div className="text-orange-300">animation</div>
          <div className="grid grid-cols-2 gap-1">
            {Object.keys(SETS).map((k) => (
              <button
                key={k}
                onClick={() => {
                  setTuneKey(k)
                  setKf(0)
                }}
                className={`rounded px-2 py-1 ${
                  tuneKey === k
                    ? 'bg-orange-500 text-black'
                    : 'bg-white/10 hover:bg-white/20'
                }`}
              >
                {k}
              </button>
            ))}
          </div>
          <div className="pt-1 text-orange-300">
            size · {sizes[tuneKey].toFixed(3)}
          </div>
          <div className="flex gap-1">
            <button
              onClick={() => bumpSize(-0.005)}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              − smaller
            </button>
            <button
              onClick={() => bumpSize(0.005)}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              + bigger
            </button>
          </div>

          <div className="pt-1 text-sky-300">ground · {groundY.toFixed(3)}</div>
          <div className="flex gap-1">
            <button
              onClick={() => setGroundY((g) => +(g - 0.004).toFixed(4))}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              ↑ up
            </button>
            <button
              onClick={() => setGroundY((g) => +(g + 0.004).toFixed(4))}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              ↓ down
            </button>
          </div>

          <div className="leading-snug text-orange-200/70">
            click scene to place the chair
          </div>

          {/* per-keyframe editor */}
          <button
            onClick={() => setKfEdit((v) => !v)}
            className={`w-full rounded py-1 ${
              kfEdit ? 'bg-orange-500 text-black' : 'bg-white/10 hover:bg-white/20'
            }`}
          >
            {kfEdit ? 'editing frames ✓' : 'edit frames'}
          </button>
          {kfEdit && (
            <div className="space-y-1 rounded bg-white/5 p-2">
              <div className="flex items-center justify-between">
                <button
                  onClick={() => stepFrame(-1)}
                  className="rounded bg-white/10 px-2 hover:bg-white/20"
                >
                  ◀
                </button>
                <span>
                  frame {kf + 1}/{SETS[tuneKey].count}
                </span>
                <button
                  onClick={() => stepFrame(1)}
                  className="rounded bg-white/10 px-2 hover:bg-white/20"
                >
                  ▶
                </button>
              </div>
              <div className="grid grid-cols-3 gap-1 text-center">
                <span />
                <button
                  onClick={() => nudge(0, -0.004)}
                  className="rounded bg-white/10 hover:bg-white/20"
                >
                  ↑
                </button>
                <span />
                <button
                  onClick={() => nudge(-0.004, 0)}
                  className="rounded bg-white/10 hover:bg-white/20"
                >
                  ←
                </button>
                <button
                  onClick={resetFrame}
                  className="rounded bg-white/10 text-[10px] hover:bg-white/20"
                >
                  reset
                </button>
                <button
                  onClick={() => nudge(0.004, 0)}
                  className="rounded bg-white/10 hover:bg-white/20"
                >
                  →
                </button>
                <span />
                <button
                  onClick={() => nudge(0, 0.004)}
                  className="rounded bg-white/10 hover:bg-white/20"
                >
                  ↓
                </button>
                <span />
              </div>
              <div className="flex items-center justify-between gap-1">
                <span className="text-orange-200/70">
                  scale {adjust[tuneKey][kf].sc.toFixed(2)}
                </span>
                <button
                  onClick={() => bumpFrameScale(-0.02)}
                  className="rounded bg-white/10 px-2 hover:bg-white/20"
                >
                  −
                </button>
                <button
                  onClick={() => bumpFrameScale(0.02)}
                  className="rounded bg-white/10 px-2 hover:bg-white/20"
                >
                  +
                </button>
              </div>
              <div className="leading-snug text-orange-200/60">
                arrows nudge · [ ] frame · − = scale
              </div>
            </div>
          )}

          <button
            onClick={saveTune}
            className="w-full rounded bg-orange-500 py-1.5 font-semibold text-black hover:bg-orange-400"
          >
            save &amp; back to site
          </button>
          <div className="flex gap-1">
            <button
              onClick={copyConfig}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              copy json
            </button>
            <button
              onClick={clearSaved}
              className="flex-1 rounded bg-white/10 py-1 hover:bg-white/20"
            >
              reset saved
            </button>
          </div>
        </div>
      )}
    </>
  )
}
