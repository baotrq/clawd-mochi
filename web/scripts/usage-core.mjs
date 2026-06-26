// Shared logic for reading Claude Pro/Code usage (5h + 7d rate-limit
// utilization) by pinging the Anthropic API with the local Claude Code
// OAuth token and reading its rate-limit headers — same trick as Clawdmeter.
// Used by both the Vite dev-server middleware and the standalone usage
// bridge script (for when the web app is hosted remotely, e.g. GitHub Pages).
import { readFileSync } from 'node:fs'
import { homedir } from 'node:os'
import { join } from 'node:path'

export async function getUsageData() {
  const credPath = join(homedir(), '.claude', '.credentials.json')
  const raw = readFileSync(credPath, 'utf-8')
  const token = JSON.parse(raw)?.claudeAiOauth?.accessToken
  if (!token) {
    return { status: 500, body: { error: 'no_token' } }
  }

  const apiRes = await fetch('https://api.anthropic.com/v1/messages', {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${token}`,
      'anthropic-version': '2023-06-01',
      'anthropic-beta': 'oauth-2025-04-20',
      'User-Agent': 'claude-code/2.1.5',
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      model: 'claude-haiku-4-5-20251001',
      max_tokens: 1,
      messages: [{ role: 'user', content: 'hi' }],
    }),
  })
  await apiRes.arrayBuffer() // drain body, only headers matter

  const h = apiRes.headers
  const sessionUtil = parseFloat(h.get('anthropic-ratelimit-unified-5h-utilization'))
  const weekUtil = parseFloat(h.get('anthropic-ratelimit-unified-7d-utilization'))
  const sessionReset = parseInt(h.get('anthropic-ratelimit-unified-5h-reset'), 10)
  const weekReset = parseInt(h.get('anthropic-ratelimit-unified-7d-reset'), 10)

  if (!Number.isFinite(sessionUtil) || !Number.isFinite(weekUtil)) {
    return { status: 502, body: { error: 'anthropic_api_error', status: apiRes.status } }
  }

  return {
    status: 200,
    body: {
      sessionPct: Math.round(sessionUtil * 100),
      weeklyPct: Math.round(weekUtil * 100),
      sessionResetAt: Number.isFinite(sessionReset) ? sessionReset * 1000 : null,
      weeklyResetAt: Number.isFinite(weekReset) ? weekReset * 1000 : null,
      fetchedAt: Date.now(),
    },
  }
}

export function usageErrorResponse(err) {
  return {
    status: err.code === 'ENOENT' ? 404 : 500,
    body: {
      error: err.code === 'ENOENT' ? 'no_credentials_file' : 'unknown_error',
      message: String(err.message || err),
    },
  }
}
