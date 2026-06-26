#!/usr/bin/env node
// Standalone local bridge for Claude Pro usage stats — runs independently of
// `npm run dev`, so the GitHub Pages-hosted web app (which has no server of
// its own) can still fetch your real usage by hitting this on localhost.
// Run manually with `node scripts/usage-bridge.mjs`, or install as a systemd
// user service (see README.md) to have it start automatically on boot.
import { createServer } from 'node:http'
import { getUsageData, usageErrorResponse } from './usage-core.mjs'

const PORT = process.env.USAGE_BRIDGE_PORT || 8787

// Only these origins may read your usage data over CORS — the hosted page
// and common local dev ports. Not a wildcard, since this proves you're
// logged into Claude Code and reveals your usage %, however low-stakes.
const ALLOWED_ORIGINS = new Set([
  'https://baotrq.github.io',
  'http://localhost:5173',
  'http://localhost:5174',
  'http://127.0.0.1:5173',
  'http://127.0.0.1:5174',
])

const server = createServer(async (req, res) => {
  const origin = req.headers.origin
  if (origin && ALLOWED_ORIGINS.has(origin)) {
    res.setHeader('Access-Control-Allow-Origin', origin)
  }
  res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS')

  if (req.method === 'OPTIONS') {
    res.statusCode = 204
    res.end()
    return
  }

  if (req.url !== '/api/usage') {
    res.statusCode = 404
    res.end('Not found')
    return
  }

  res.setHeader('Content-Type', 'application/json')
  try {
    const { status, body } = await getUsageData()
    res.statusCode = status
    res.end(JSON.stringify(body))
  } catch (err) {
    const { status, body } = usageErrorResponse(err)
    res.statusCode = status
    res.end(JSON.stringify(body))
  }
})

server.listen(PORT, '127.0.0.1', () => {
  console.log(`Claude usage bridge listening on http://localhost:${PORT}/api/usage`)
})
