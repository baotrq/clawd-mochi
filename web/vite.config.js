import { defineConfig } from 'vite'
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

// https://vite.dev/config/
export default defineConfig({
  // Relative base so the built app also runs from file:// (used for local
  // headless screenshot checks without a running dev server).
  base: './',
  plugins: [react(), tailwindcss(), claudeUsagePlugin()],
})
