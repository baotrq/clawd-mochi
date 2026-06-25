/**
 * Clawd pixel sprite — the canonical ">< " happy-face Clawd, built as a pixel
 * matrix so it stays crisp at any size and animates by toggling pixels.
 *
 * Faces are symmetric, so direction is handled by the caller (flip) if needed.
 */

export const CLAWD_COLS = 17
export const CLAWD_ROWS = 13

export const CLAWD_PALETTE = {
  O: '#d07a58', // coral body
  K: '#141211', // eyes
}

/**
 * Build a Clawd frame.
 * @param {number} legPhase 0 = idle (all legs down), 1 = walk-A, 2 = walk-B
 * @returns {string[][]} grid of palette keys ('.' = transparent)
 */
export function buildClawd(legPhase = 0) {
  const g = Array.from({ length: CLAWD_ROWS }, () => Array(CLAWD_COLS).fill('.'))
  const fillRow = (y, a, b) => {
    for (let x = a; x <= b; x++) g[y][x] = 'O'
  }

  // main body cols 1..15, rows 0..9
  for (let y = 0; y < 10; y++) fillRow(y, 1, 15)
  // arm stubs at eye level
  for (let y = 4; y < 7; y++) {
    g[y][0] = 'O'
    g[y][16] = 'O'
  }
  // round the four body corners
  for (const [y, x] of [[0, 1], [0, 15], [9, 1], [9, 15]]) g[y][x] = '.'
  // center bottom notch
  for (let x = 7; x < 10; x++) g[9][x] = '.'

  // four 2-wide legs (rows 10..12); walk phases shorten one pair for a shuffle
  const legCols = [2, 5, 10, 13]
  legCols.forEach((lx, i) => {
    let len = 3
    if (legPhase === 1) len = i < 2 ? 3 : 2
    else if (legPhase === 2) len = i < 2 ? 2 : 3
    for (let y = 10; y < 10 + len; y++) {
      g[y][lx] = 'O'
      g[y][lx + 1] = 'O'
    }
  })

  // eyes: '>' (left) and '<' (right) — bold 2-thick chevrons, rows 3..7
  const chevron = (col, rightFacing) => {
    const offs = rightFacing ? [0, 1, 2, 1, 0] : [2, 1, 0, 1, 2]
    offs.forEach((off, i) => {
      g[3 + i][col + off] = 'K'
      g[3 + i][col + off + 1] = 'K'
    })
  }
  chevron(3, true)
  chevron(10, false)

  return g
}

/** Draw a Clawd matrix onto a 2D context at (ox, oy) with pixel size `px`. */
export function drawMatrix(ctx, g, ox, oy, px, palette = CLAWD_PALETTE) {
  const size = Math.ceil(px) + 1 // slight overlap to avoid seams
  for (let y = 0; y < g.length; y++) {
    for (let x = 0; x < g[y].length; x++) {
      const c = palette[g[y][x]]
      if (!c) continue
      ctx.fillStyle = c
      ctx.fillRect(Math.round(ox + x * px), Math.round(oy + y * px), size, size)
    }
  }
}
