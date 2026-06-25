import { useEffect, useRef, useState } from 'react'

const VIDEO_W = 1280
const VIDEO_H = 720

/** object-cover transform helper matching Clawd.jsx */
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

export default function HotspotOverlay({
  calibrateMode,
  hotspots,
  onUpdateHotspots,
  onTriggerMode,
}) {
  const containerRef = useRef(null)
  const [dimensions, setDimensions] = useState({ w: 0, h: 0 })
  const [dragState, setDragState] = useState(null) // { key, type: 'move'|'resize', startX, startY, startVal }

  // Track container resizing
  useEffect(() => {
    const container = containerRef.current
    if (!container) return

    const resize = () => {
      setDimensions({
        w: container.clientWidth,
        h: container.clientHeight,
      })
    }
    resize()

    const observer = new ResizeObserver(resize)
    observer.observe(container)
    return () => observer.disconnect()
  }, [])

  const { scale, ox, oy } = coverTransform(dimensions.w, dimensions.h)

  // Mouse drag handlers for calibration mode
  const handleMouseDown = (key, type, e) => {
    if (!calibrateMode) return
    e.stopPropagation()
    e.preventDefault()

    const hotspot = hotspots[key]
    setDragState({
      key,
      type,
      startX: e.clientX,
      startY: e.clientY,
      startVal: { ...hotspot },
    })
  }

  useEffect(() => {
    const handleMouseMove = (e) => {
      if (!dragState || !containerRef.current) return
      
      const { key, type, startX, startY, startVal } = dragState
      const dx = e.clientX - startX
      const dy = e.clientY - startY

      // Convert pixel deltas to virtual coordinates
      const virtualDx = dx / (VIDEO_W * scale)
      const virtualDy = dy / (VIDEO_H * scale)

      const updated = { ...hotspots }
      if (type === 'move') {
        updated[key] = {
          ...startVal,
          x: Math.max(0, Math.min(1, +(startVal.x + virtualDx).toFixed(4))),
          y: Math.max(0, Math.min(1, +(startVal.y + virtualDy).toFixed(4))),
        }
      } else if (type === 'resize') {
        // Dragging the handle scales the radius. Calculate based on horizontal drag
        const rDelta = dx / (VIDEO_H * scale)
        updated[key] = {
          ...startVal,
          r: Math.max(0.01, Math.min(0.5, +(startVal.r + rDelta).toFixed(4))),
        }
      }

      onUpdateHotspots(updated)
    };

    const handleMouseUp = () => {
      if (dragState) setDragState(null)
    }

    if (dragState) {
      window.addEventListener('mousemove', handleMouseMove)
      window.addEventListener('mouseup', handleMouseUp)
    }

    return () => {
      window.removeEventListener('mousemove', handleMouseMove)
      window.removeEventListener('mouseup', handleMouseUp)
    }
  }, [dragState, scale, hotspots, onUpdateHotspots])

  const copyCalibrationJson = () => {
    navigator.clipboard.writeText(JSON.stringify(hotspots, null, 2))
    alert('Hotspot coordinates copied to clipboard!')
  }

  return (
    <div
      ref={containerRef}
      className="absolute inset-0 z-10 overflow-hidden pointer-events-none"
    >
      {Object.entries(hotspots).map(([key, h]) => {
        // Map relative coordinates to screen pixels
        const cx = ox + h.x * VIDEO_W * scale
        const cy = oy + h.y * VIDEO_H * scale
        const radius = h.r * VIDEO_H * scale
        const size = radius * 2

        return (
          <div
            key={key}
            style={{
              position: 'absolute',
              left: cx - radius,
              top: cy - radius,
              width: size,
              height: size,
            }}
            className="group pointer-events-auto flex items-center justify-center"
          >
            {/* Clickable target */}
            <button
              data-hotspot-trigger
              onClick={() => !calibrateMode && onTriggerMode(key, h.mode, h.cmd)}
              className={`h-full w-full rounded-full transition-all duration-300 relative flex items-center justify-center cursor-pointer border ${
                calibrateMode
                  ? 'border-orange-500 bg-orange-500/20'
                  : 'border-transparent'
              }`}
              onMouseDown={(e) => handleMouseDown(key, 'move', e)}
            >

              {/* Hotspot label/tooltip */}
              {!calibrateMode && (
                <div className="absolute bottom-full mb-2 scale-90 opacity-0 group-hover:scale-100 group-hover:opacity-100 transition-all duration-200 pointer-events-none bg-room-bg-deep/90 border border-ascii-mid/30 text-ascii-bright text-[10px] px-2 py-0.5 rounded tracking-wider shadow-lg whitespace-nowrap">
                  {h.label}
                </div>
              )}

              {/* Calibration label */}
              {calibrateMode && (
                <div className="absolute text-[9px] text-orange-200 font-mono bg-black/85 px-1 rounded pointer-events-none leading-none select-none">
                  {key}
                  <br />
                  x:{h.x.toFixed(3)}
                  <br />
                  y:{h.y.toFixed(3)}
                  <br />
                  r:{h.r.toFixed(3)}
                </div>
              )}
            </button>

            {/* Resize handle (only visible in calibrate mode) */}
            {calibrateMode && (
              <div
                onMouseDown={(e) => handleMouseDown(key, 'resize', e)}
                style={{
                  position: 'absolute',
                  right: -6,
                  top: '50%',
                  transform: 'translateY(-50%)',
                  width: 12,
                  height: 12,
                }}
                className="bg-sky-400 hover:bg-sky-300 border border-black rounded-full cursor-ew-resize z-20 flex items-center justify-center"
                title="Drag to resize radius"
              />
            )}
          </div>
        )
      })}

      {/* Floating calibration toolbox */}
      {calibrateMode && (
        <div className="absolute bottom-4 right-20 z-30 pointer-events-auto bg-black/80 border border-orange-500/50 rounded-lg p-3 w-56 font-mono text-[11px] text-orange-200 backdrop-blur shadow-2xl space-y-2">
          <div className="text-orange-400 font-semibold border-b border-orange-500/30 pb-1">
            hotspot calibration
          </div>
          <div className="leading-relaxed">
            • drag hotspot to move
            <br />• drag blue dot to resize
          </div>
          <button
            onClick={copyCalibrationJson}
            className="w-full bg-orange-600 hover:bg-orange-500 text-black font-semibold py-1 rounded transition-colors cursor-pointer text-center"
          >
            copy coordinates json
          </button>
        </div>
      )}
    </div>
  )
}
