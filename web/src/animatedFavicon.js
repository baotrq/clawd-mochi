// Animates the browser-tab favicon through the pixel-crab frames.
//
// Animated GIF/SVG favicons don't animate in Chromium or Safari (only the
// first frame renders), so the portable trick is to swap the <link rel="icon">
// href between preloaded PNG frames on a timer. Works in Chrome/Edge/Firefox.
//
// Frames live in web/public/favicon/frame{0..N}.png and are referenced through
// import.meta.env.BASE_URL so they resolve under the GitHub Pages subpath too.

const FRAME_COUNT = 5
const FRAME_MS = 160

export function startAnimatedFavicon() {
  const base = import.meta.env.BASE_URL
  const urls = Array.from(
    { length: FRAME_COUNT },
    (_, i) => `${base}favicon/frame${i}.png`,
  )

  let link = document.querySelector("link[rel~='icon']")
  if (!link) {
    link = document.createElement('link')
    link.rel = 'icon'
    document.head.appendChild(link)
  }
  link.type = 'image/png'

  // Preload so swapping the href doesn't flicker on the first loop.
  urls.forEach((u) => {
    const img = new Image()
    img.src = u
  })

  let i = 0
  link.href = urls[0]
  let timer = null

  const tick = () => {
    i = (i + 1) % urls.length
    link.href = urls[i]
  }
  const start = () => {
    if (timer == null) timer = setInterval(tick, FRAME_MS)
  }
  const stop = () => {
    if (timer != null) {
      clearInterval(timer)
      timer = null
    }
  }

  // Don't burn cycles animating a tab nobody is looking at.
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) stop()
    else start()
  })

  start()
}
