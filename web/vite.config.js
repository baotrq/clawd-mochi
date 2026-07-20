import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { getUsageData, usageErrorResponse } from './scripts/usage-core.mjs'

// Dev-only middleware exposing Claude Pro/Code usage. See web/scripts/usage-core.mjs
// for the actual logic (shared with the standalone usage-bridge.mjs script, which
// does the same job for when the web app is hosted remotely, e.g. GitHub Pages).
function claudeUsagePlugin() {
  return {
    name: 'claude-usage-api',
    configureServer(server) {
      server.middlewares.use('/api/usage', async (req, res) => {
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
    },
  }
}

// Dev-only middleware proxying football-data.org / balldontlie.io team-list
// lookups (favorite-teams picker) — both reject direct browser calls (no
// CORS headers, meant for server-side use), same problem claudeUsagePlugin
// above already solved for the Claude usage API. Runs server-side in the
// Vite dev process, where there's no CORS restriction, and hands back
// plain JSON to the browser.
function scoreTeamsProxyPlugin(env) {
  return {
    name: 'score-teams-proxy',
    configureServer(server) {
      server.middlewares.use('/api/football-teams', async (req, res) => {
        const code = new URL(req.url, 'http://x').searchParams.get('code')
        res.setHeader('Content-Type', 'application/json')
        try {
          const r = await fetch(`https://api.football-data.org/v4/competitions/${code}/teams`, {
            headers: { 'X-Auth-Token': env.VITE_FOOTBALL_DATA_API_KEY },
          })
          res.statusCode = r.status
          res.end(await r.text())
        } catch (err) {
          res.statusCode = 502
          res.end(JSON.stringify({ error: String(err) }))
        }
      })
      server.middlewares.use('/api/nba-teams', async (req, res) => {
        res.setHeader('Content-Type', 'application/json')
        try {
          const r = await fetch('https://api.balldontlie.io/v1/teams', {
            headers: { Authorization: env.VITE_BALLDONTLIE_API_KEY },
          })
          res.statusCode = r.status
          res.end(await r.text())
        } catch (err) {
          res.statusCode = 502
          res.end(JSON.stringify({ error: String(err) }))
        }
      })
    },
  }
}

// https://vite.dev/config/
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  return {
    // Relative base so the built app also runs from file:// (used for local
    // headless screenshot checks without a running dev server).
    base: './',
    plugins: [react(), tailwindcss(), claudeUsagePlugin(), scoreTeamsProxyPlugin(env)],
  }
})
