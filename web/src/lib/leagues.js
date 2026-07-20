// Favorite-teams picker config (App.jsx + ControlPanel.jsx) — grouped by
// league so the picker can be checkboxes instead of free typing. Domestic
// leagues + NBA + World Cup fetch their real current roster from
// football-data.org / balldontlie.io (same APIs/keys the device itself
// uses for scores) — no hardcoded team list to go stale. Proxied through
// the Vite dev-server (see vite.config.js) since both APIs reject direct
// browser calls (no CORS headers). Champions League/Europa League don't
// get their own group: matching is by team name only (see
// teamMatchesFavorite in scores.ino), so picking a club under its domestic
// league already covers it in any competition it plays. VBA has no
// team-list API available (Flashscore's only confirmed endpoints are
// search/live, not a roster) — best-effort hardcoded list, may drift if
// the league's teams change.
export const LEAGUE_GROUPS = [
  { code: 'PL', label: 'Premier League', sport: 'football', source: 'football-data' },
  { code: 'PD', label: 'La Liga', sport: 'football', source: 'football-data' },
  { code: 'BL1', label: 'Bundesliga', sport: 'football', source: 'football-data' },
  { code: 'SA', label: 'Serie A', sport: 'football', source: 'football-data' },
  { code: 'FL1', label: 'Ligue 1', sport: 'football', source: 'football-data' },
  { code: 'DED', label: 'Eredivisie', sport: 'football', source: 'football-data' },
  { code: 'PPL', label: 'Primeira Liga', sport: 'football', source: 'football-data' },
  { code: 'ELC', label: 'Championship', sport: 'football', source: 'football-data' },
  { code: 'BSA', label: 'Brasileirão', sport: 'football', source: 'football-data' },
  { code: 'WC', label: 'World Cup (National Teams)', sport: 'football', source: 'football-data' },
  { code: 'NBA', label: 'NBA', sport: 'basketball', source: 'balldontlie' },
  {
    code: 'VBA',
    label: 'VBA (Vietnam Basketball)',
    sport: 'basketball',
    source: 'static',
    teams: ['Saigon Heat', 'Hanoi Buffaloes', 'Danang Dragons', 'Cantho Catfish', 'Thang Long Warriors'],
  },
]

export const SPORTS = [
  { id: 'football', label: '⚽ Football' },
  { id: 'basketball', label: '🏀 Basketball' },
]

export const MAX_FAVORITE_TEAMS = 6
