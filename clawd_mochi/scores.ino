// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  LIVE SPORTS SCORES (MODE_SCORES)
//  Football (football-data.org) + NBA (balldontlie.io) fetched directly by
//  the device every 5 min. Europa League + VBA (Vietnamese Basketball) via
//  RapidAPI Flashscore every 2h — that key has a 500 req/month hard cap
//  shared by both, so it gets its own much slower timer.
//  ScoreEntry array / favoriteTeams / Mode/enum live in clawd_mochi.ino
//  (shared with mode_switching.ino for the 'F' command and mode digit '8').
// ═════════════════════════════════════════════════════════════

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ctype.h>
#include <strings.h>  // strcasecmp

const char* SCORE_LEAGUE_TAG[SL_COUNT] = {
  "WORLD CUP", "CHAMPIONS LG", "PREMIER LEAGUE", "LA LIGA", "BUNDESLIGA", "SERIE A", "LIGUE 1",
  "EURO CUP", "BRASILEIRAO", "PRIMEIRA LIGA", "EREDIVISIE", "CHAMPIONSHIP",
  "EUROPA LG", "NBA", "VBA"
};

// Drops any existing entries belonging to the source(s) about to be
// refetched, so a fetch pass replaces its own leagues without disturbing
// entries owned by the other two independent timers. Relies on SL_UCL..
// SL_ELC being the contiguous block of football-data.org leagues (see the
// enum comment in clawd_mochi.ino).
void clearScoreEntriesForSource(bool footballData, bool nba, bool rapidApi) {
  uint8_t w = 0;
  for (uint8_t r = 0; r < scoreEntryCount; r++) {
    ScoreEntry& e = scoreEntries[r];
    bool isFd = (e.league >= SL_WC && e.league <= SL_ELC);
    bool isNba = (e.league == SL_NBA);
    bool isRapid = (e.league == SL_UEL || e.league == SL_VBA);
    bool drop = (footballData && isFd) || (nba && isNba) || (rapidApi && isRapid);
    if (!drop) scoreEntries[w++] = e;
  }
  scoreEntryCount = w;
}

static void lowerCopy(char* dst, size_t dstSize, const char* src) {
  size_t i = 0;
  for (; src[i] && i < dstSize - 1; i++) dst[i] = tolower((unsigned char)src[i]);
  dst[i] = '\0';
}

// Case-insensitive substring match against the favorite-teams list pushed
// by the web app's 'F' command.
bool teamMatchesFavorite(const char* teamName) {
  char lname[24];
  lowerCopy(lname, sizeof(lname), teamName);
  for (uint8_t i = 0; i < favoriteTeamCount; i++) {
    char lfav[24];
    lowerCopy(lfav, sizeof(lfav), favoriteTeams[i]);
    if (lfav[0] && strstr(lname, lfav) != nullptr) return true;
  }
  return false;
}

// Formats (now - daysAgo days) as "YYYY-MM-DD" — used to build date-range
// queries so fetches return recent finished results too, not just whatever
// happens to be live at the exact moment of the request (which is empty
// most of the time, and always empty out of season).
static void formatDateDaysAgo(char* buf, size_t bufSize, int daysAgo) {
  time_t nowSecs;
  time(&nowSecs);
  time_t t = nowSecs - (time_t)daysAgo * 86400;
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(buf, bufSize, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}
#define SCORES_LOOKBACK_DAYS 3
#define SCORES_LOOKAHEAD_DAYS 7

// Formats a football-data.org "utcDate" (e.g. "2026-07-20T18:00:00Z") as a
// short relative-day label ("TODAY"/"TOM"/"in 3d") for upcoming favorite
// matches — only shown for favorites, see fetchFootballDataCompetition.
// Compares calendar days only (treats the date string as a local day, skips
// UTC->local time-of-day conversion) — close enough for a day-granularity
// status-line label, not worth the extra complexity for exact kickoff time.
void formatUpcomingStatus(char* buf, size_t bufSize, const char* utcDate) {
  int y, mo, d;
  if (strlen(utcDate) < 10 || sscanf(utcDate, "%4d-%2d-%2d", &y, &mo, &d) != 3) {
    snprintf(buf, bufSize, "UPCOMING");
    return;
  }
  struct tm matchTm = {0};
  matchTm.tm_year = y - 1900;
  matchTm.tm_mon = mo - 1;
  matchTm.tm_mday = d;
  matchTm.tm_hour = 12;
  time_t matchDay = mktime(&matchTm);

  time_t nowSecs;
  time(&nowSecs);
  struct tm nowTm;
  localtime_r(&nowSecs, &nowTm);
  nowTm.tm_hour = 12;
  nowTm.tm_min = 0;
  nowTm.tm_sec = 0;
  time_t today = mktime(&nowTm);

  int diffDays = (int)((matchDay - today) / 86400);
  if (diffDays <= 0) snprintf(buf, bufSize, "TODAY");
  else if (diffDays == 1) snprintf(buf, bufSize, "TOM");
  else snprintf(buf, bufSize, "in %dd", diffDays);
}

// ── football-data.org: all 12 competitions on the free tier, ordered
// biggest-league-first — fetched in this order so a busy day's shared
// scoreEntries[] cap fills with the bigger leagues before the smaller ones.
static const char* FD_COMP_CODES[] = {
  "WC", "CL", "PL", "PD", "BL1", "SA", "FL1", "EC", "BSA", "PPL", "DED", "ELC"
};
static const ScoreLeague FD_COMP_LEAGUE[] = {
  SL_WC, SL_UCL, SL_PL, SL_LALIGA, SL_BUNDESLIGA, SL_SERIEA, SL_LIGUE1,
  SL_EC, SL_BSA, SL_PPL, SL_DED, SL_ELC
};
#define FD_COMP_COUNT 12

// dateFrom/dateTo (YYYY-MM-DD) span the last SCORES_LOOKBACK_DAYS days —
// returns live AND recently-finished matches in one call, not just
// whatever's live at this exact second (empty most of the time/off-season).
void fetchFootballDataCompetition(const char* code, ScoreLeague league, const char* dateFrom, const char* dateTo) {
  WiFiClientSecure client;
  client.setInsecure();  // matches wifi_sync.ino — skip cert verification for simplicity
  HTTPClient http;
  String url = String("https://api.football-data.org/v4/competitions/") + code + "/matches?dateFrom=" + dateFrom + "&dateTo=" + dateTo;
  if (!http.begin(client, url)) {
    Serial.printf("Scores: football-data %s begin failed\n", code);
    return;
  }
  http.addHeader("X-Auth-Token", FOOTBALL_DATA_API_KEY);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument filter;
    filter["matches"][0]["homeTeam"]["name"] = true;
    filter["matches"][0]["awayTeam"]["name"] = true;
    filter["matches"][0]["score"]["fullTime"]["home"] = true;
    filter["matches"][0]["score"]["fullTime"]["away"] = true;
    filter["matches"][0]["status"] = true;
    filter["matches"][0]["utcDate"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err) {
      for (JsonObject m : doc["matches"].as<JsonArray>()) {
        if (scoreEntryCount >= MAX_SCORE_ENTRIES) break;
        const char* st = m["status"] | "";
        bool isPlaying = (strcmp(st, "IN_PLAY") == 0 || strcmp(st, "PAUSED") == 0);
        bool isFinished = (strcmp(st, "FINISHED") == 0 || strcmp(st, "AWARDED") == 0);
        bool isScheduled = (strcmp(st, "SCHEDULED") == 0 || strcmp(st, "TIMED") == 0);
        const char* homeName = m["homeTeam"]["name"] | "?";
        const char* awayName = m["awayTeam"]["name"] | "?";
        // Scheduled/upcoming matches only worth keeping for favorite teams —
        // otherwise 12 competitions x 10 days would flood the shared entry
        // cap with fixtures nobody asked to see.
        bool isFavoriteMatch = teamMatchesFavorite(homeName) || teamMatchesFavorite(awayName);
        if (!isPlaying && !isFinished && !(isScheduled && isFavoriteMatch)) continue;
        ScoreEntry& e = scoreEntries[scoreEntryCount];
        e.league = league;
        snprintf(e.home, sizeof(e.home), "%s", homeName);
        snprintf(e.away, sizeof(e.away), "%s", awayName);
        e.homeScore = m["score"]["fullTime"]["home"] | -1;
        e.awayScore = m["score"]["fullTime"]["away"] | -1;
        e.isLive = isPlaying;
        if (isPlaying) {
          snprintf(e.status, sizeof(e.status), "%s", strcmp(st, "PAUSED") == 0 ? "HT" : "LIVE");
        } else if (isFinished) {
          snprintf(e.status, sizeof(e.status), "FT");
        } else {
          formatUpcomingStatus(e.status, sizeof(e.status), (const char*)(m["utcDate"] | ""));
        }
        e.valid = true;
        scoreEntryCount++;
      }
    } else {
      Serial.printf("Scores: football-data %s JSON parse failed: %s\n", code, err.c_str());
    }
  } else {
    Serial.printf("Scores: football-data %s HTTP %d\n", code, httpCode);
  }
  http.end();
}

// football-data.org free tier caps at 10 requests/minute; firing all 12
// competition calls back-to-back blows past that (observed HTTP 429).
// 6.5s spacing keeps every rolling 60s window under the cap.
#define FD_CALL_SPACING_MS 6500

static bool footballDataPreCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Scores: WiFi not connected, skipping football leagues.");
    return false;
  }
  if (FOOTBALL_DATA_API_KEY[0] == '\0') {
    Serial.println("Scores: no FOOTBALL_DATA_API_KEY, skipping football leagues.");
    return false;
  }
  if (!timeSynced) {
    Serial.println("Scores: time not synced yet, skipping football leagues (need date range).");
    return false;
  }
  return true;
}

// Blocking, all-12-at-once — only used by the 'Z' manual test command,
// where a human is already sitting at Serial Monitor waiting for an
// immediate pass/fail per league. Takes ~80s. NOT used by the automatic
// background cycle (see tickFootballDataAutoCycle below) — blocking loop()
// for 80s every 5 minutes would freeze alarms/Serial/everything else.
void fetchFootballDataBlocking() {
  if (!footballDataPreCheck()) return;
  char dateFrom[11], dateTo[11];
  formatDateDaysAgo(dateFrom, sizeof(dateFrom), SCORES_LOOKBACK_DAYS);
  formatDateDaysAgo(dateTo, sizeof(dateTo), -SCORES_LOOKAHEAD_DAYS);
  clearScoreEntriesForSource(true, false, false);
  for (int i = 0; i < FD_COMP_COUNT; i++) {
    fetchFootballDataCompetition(FD_COMP_CODES[i], FD_COMP_LEAGUE[i], dateFrom, dateTo);
    if (i < FD_COMP_COUNT - 1) delay(FD_CALL_SPACING_MS);
  }
}

// Non-blocking version for the automatic background cycle: fetches at most
// one competition per call, spaced FD_CALL_SPACING_MS apart, so loop() only
// ever blocks for a single HTTP request's duration, not the whole batch.
static uint8_t fdAutoIndex = FD_COMP_COUNT;  // FD_COMP_COUNT = idle, no cycle in progress
static uint32_t fdAutoNextAt = 0;
static char fdAutoDateFrom[11] = "";
static char fdAutoDateTo[11] = "";

void startFootballDataAutoCycle() {
  if (!footballDataPreCheck()) return;
  formatDateDaysAgo(fdAutoDateFrom, sizeof(fdAutoDateFrom), SCORES_LOOKBACK_DAYS);
  formatDateDaysAgo(fdAutoDateTo, sizeof(fdAutoDateTo), -SCORES_LOOKAHEAD_DAYS);
  clearScoreEntriesForSource(true, false, false);
  fdAutoIndex = 0;
  fdAutoNextAt = millis();  // fire the first competition immediately
}

void tickFootballDataAutoCycle() {
  if (fdAutoIndex >= FD_COMP_COUNT) return;  // idle
  uint32_t now = millis();
  if (now < fdAutoNextAt) return;
  fetchFootballDataCompetition(FD_COMP_CODES[fdAutoIndex], FD_COMP_LEAGUE[fdAutoIndex], fdAutoDateFrom, fdAutoDateTo);
  fdAutoIndex++;
  fdAutoNextAt = now + FD_CALL_SPACING_MS;
  if (currentMode == MODE_SCORES) drawScoresView();  // reflect new entries as they trickle in
}

// ── balldontlie.io: NBA ──────────────────────────────────────────────
void fetchNbaGames() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Scores: WiFi not connected, skipping NBA.");
    return;
  }
  if (BALLDONTLIE_API_KEY[0] == '\0') {
    Serial.println("Scores: no BALLDONTLIE_API_KEY, skipping NBA.");
    return;
  }
  if (!timeSynced) {
    Serial.println("Scores: time not synced yet, skipping NBA (need today's date).");
    return;
  }

  clearScoreEntriesForSource(false, true, false);

  // Same range as football: recent finished games + live, plus upcoming
  // days too (kept below only for favorite teams — see the loop).
  String url = "https://api.balldontlie.io/v1/games?per_page=100";
  for (int d = -SCORES_LOOKAHEAD_DAYS; d <= SCORES_LOOKBACK_DAYS; d++) {
    char dateStr[11];
    formatDateDaysAgo(dateStr, sizeof(dateStr), d);
    url += "&dates[]=" + String(dateStr);
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("Scores: balldontlie begin failed");
    return;
  }
  http.addHeader("Authorization", BALLDONTLIE_API_KEY);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument filter;
    filter["data"][0]["home_team"]["full_name"] = true;
    filter["data"][0]["visitor_team"]["full_name"] = true;
    filter["data"][0]["home_team_score"] = true;
    filter["data"][0]["visitor_team_score"] = true;
    filter["data"][0]["status"] = true;
    filter["data"][0]["period"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err) {
      for (JsonObject g : doc["data"].as<JsonArray>()) {
        if (scoreEntryCount >= MAX_SCORE_ENTRIES) break;
        int period = g["period"] | 0;
        const char* status = g["status"] | "";
        bool isFinal = strcasecmp(status, "Final") == 0;
        bool notStarted = (period <= 0 && !isFinal);
        const char* homeName = g["home_team"]["full_name"] | "?";
        const char* awayName = g["visitor_team"]["full_name"] | "?";
        // Not-yet-started games only worth keeping for favorite teams —
        // same reasoning as football-data's scheduled-match filter above.
        bool isFavoriteMatch = teamMatchesFavorite(homeName) || teamMatchesFavorite(awayName);
        if (notStarted && !isFavoriteMatch) continue;
        ScoreEntry& e = scoreEntries[scoreEntryCount];
        e.league = SL_NBA;
        snprintf(e.home, sizeof(e.home), "%s", homeName);
        snprintf(e.away, sizeof(e.away), "%s", awayName);
        // balldontlie returns 0 (not null) for a not-started game's score,
        // so force -1 there to get the "vs" fallback in drawScoresView()
        // instead of a misleading "0 - 0".
        e.homeScore = notStarted ? -1 : (int8_t)(g["home_team_score"] | -1);
        e.awayScore = notStarted ? -1 : (int8_t)(g["visitor_team_score"] | -1);
        // balldontlie's own "status" field is already a human-readable
        // scheduled start time (e.g. "7:00 PM ET") for not-started games —
        // reuse directly, no extra formatting needed.
        snprintf(e.status, sizeof(e.status), "%s", status);
        e.isLive = !isFinal && !notStarted;
        e.valid = true;
        scoreEntryCount++;
      }
    } else {
      Serial.printf("Scores: balldontlie JSON parse failed: %s\n", err.c_str());
    }
  } else {
    Serial.printf("Scores: balldontlie HTTP %d\n", httpCode);
  }
  http.end();
}

// ── RapidAPI Flashscore: Europa League + VBA ────────────────────────
// Endpoints confirmed live via the RapidAPI playground (2026-07):
//   Search:       GET /api/flashscore/v2/general/search?q=<term>
//                 -> flat array of {id, type, name, url, sport:{id,name},
//                    gender, country_name, ...}. Tournament results have
//                    type "tournament_template" (NOT "tournament" — this
//                    was the wrong guess that 404'd before). Confirmed
//                    example: Europa League id "ClDjv3V5".
//   Live matches: GET /api/flashscore/v2/matches/live?sport_id=<N>&timezone=<tz>
//                 -> array of tournament groups: {tournament_id, name,
//                    country_name, matches:[{match_id, match_status:{stage,
//                    is_in_progress, is_finished, live_time, ...}, timestamp,
//                    home_team:{name,...}, away_team:{name,...},
//                    scores:{home,away}}]}. No per-tournament filter param —
//                    fetch all live matches for a sport, match tournament_id
//                    against our cached one client-side.
//   sport_id: confirmed 1=Soccer, 3=Basketball (from playground searches).
// NOTE: "Live" matches only — no finished/recent results for EL/VBA yet
// (would need the separate "Get Matches by day/date" endpoint, unverified).
// So this stays empty outside an actual live EL or VBA match, unlike
// football-data/NBA which also show the last few days' results.
#define RAPIDAPI_FLASHSCORE_HOST "flashscore4.p.rapidapi.com"
#define FS_SPORT_SOCCER 1
#define FS_SPORT_BASKETBALL 3

// FlashscoreTournament struct itself lives in clawd_mochi.ino (see comment
// there) — only this file's data array is here. Both ids pre-seeded from
// confirmed real search results (2026-07), so fetchFlashscoreTournamentId()
// never has to run for these — no runtime fuzzy-match dependency, and one
// less RapidAPI call per boot against the 500/month cap.
FlashscoreTournament FS_TOURNAMENTS[] = {
  { "UEFA Europa League", FS_SPORT_SOCCER, SL_UEL, "ClDjv3V5", true },
  { "VBA", FS_SPORT_BASKETBALL, SL_VBA, "U5ZhqLVi", true },
};
#define FS_TOURNAMENT_COUNT 2

String urlEncodeSpaces(const char* in) {
  String out;
  for (size_t i = 0; in[i]; i++) {
    char c = in[i];
    if (isalnum((unsigned char)c)) out += c;
    else if (c == ' ') out += "%20";
    else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      out += buf;
    }
  }
  return out;
}

// One-time lookup per tournament, cached in FlashscoreTournament::cachedId
// for every later poll (search results are stable ids, no need to re-fetch).
bool fetchFlashscoreTournamentId(FlashscoreTournament& t) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://") + RAPIDAPI_FLASHSCORE_HOST + "/api/flashscore/v2/general/search?q=" + urlEncodeSpaces(t.searchName);
  if (!http.begin(client, url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-rapidapi-host", RAPIDAPI_FLASHSCORE_HOST);
  http.addHeader("x-rapidapi-key", RAPIDAPI_FLASHSCORE_KEY);
  int httpCode = http.GET();
  bool found = false;
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      for (JsonObject r : doc.as<JsonArray>()) {
        const char* type = r["type"] | "";
        if (strcmp(type, "tournament_template") == 0) {
          snprintf(t.cachedId, sizeof(t.cachedId), "%s", (const char*)(r["id"] | ""));
          t.idCached = true;
          found = true;
          break;
        }
      }
      if (!found) Serial.printf("Scores: Flashscore search for %s had no tournament_template result\n", t.searchName);
    } else {
      Serial.printf("Scores: Flashscore search JSON parse failed for %s\n", t.searchName);
    }
  } else {
    Serial.printf("Scores: Flashscore search HTTP %d for %s\n", httpCode, t.searchName);
  }
  http.end();
  return found;
}

// Fetches ALL live matches for one sport, then keeps only the groups whose
// tournament_id matches one of our cached FlashscoreTournament entries for
// that same sport (there's no per-tournament filter param on this endpoint).
void fetchFlashscoreLiveForSport(uint8_t sportId) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://") + RAPIDAPI_FLASHSCORE_HOST + "/api/flashscore/v2/matches/live?sport_id=" + sportId + "&timezone=Asia%2FHo_Chi_Minh";
  if (!http.begin(client, url)) {
    Serial.printf("Scores: Flashscore live begin failed (sport %d)\n", sportId);
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-rapidapi-host", RAPIDAPI_FLASHSCORE_HOST);
  http.addHeader("x-rapidapi-key", RAPIDAPI_FLASHSCORE_KEY);
  int httpCode = http.GET();
  if (httpCode == 200) {
    // No tournament filter on this endpoint, so the raw payload can be
    // large on a busy day (every live match worldwide for this sport) —
    // the raw HTTP response itself can't be shrunk, but this filter at
    // least caps what ArduinoJson actually parses/stores into RAM.
    String payload = http.getString();
    JsonDocument filter;
    filter[0]["tournament_id"] = true;
    filter[0]["matches"][0]["match_status"]["is_in_progress"] = true;
    filter[0]["matches"][0]["match_status"]["is_finished"] = true;
    filter[0]["matches"][0]["match_status"]["stage"] = true;
    filter[0]["matches"][0]["home_team"]["name"] = true;
    filter[0]["matches"][0]["away_team"]["name"] = true;
    filter[0]["matches"][0]["scores"]["home"] = true;
    filter[0]["matches"][0]["scores"]["away"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err) {
      for (JsonObject group : doc.as<JsonArray>()) {
        const char* groupTid = group["tournament_id"] | "";
        for (int i = 0; i < FS_TOURNAMENT_COUNT; i++) {
          FlashscoreTournament& t = FS_TOURNAMENTS[i];
          if (t.sportId != sportId || !t.idCached) continue;
          if (strcmp(groupTid, t.cachedId) != 0) continue;
          for (JsonObject m : group["matches"].as<JsonArray>()) {
            if (scoreEntryCount >= MAX_SCORE_ENTRIES) break;
            JsonObject status = m["match_status"];
            bool isPlaying = status["is_in_progress"] | false;
            bool isFinished = status["is_finished"] | false;
            if (!isPlaying && !isFinished) continue;
            ScoreEntry& e = scoreEntries[scoreEntryCount];
            e.league = t.league;
            snprintf(e.home, sizeof(e.home), "%s", (const char*)(m["home_team"]["name"] | "?"));
            snprintf(e.away, sizeof(e.away), "%s", (const char*)(m["away_team"]["name"] | "?"));
            e.homeScore = m["scores"]["home"] | -1;
            e.awayScore = m["scores"]["away"] | -1;
            e.isLive = isPlaying;
            snprintf(e.status, sizeof(e.status), "%s", isPlaying ? (const char*)(status["stage"] | "LIVE") : "FT");
            e.valid = true;
            scoreEntryCount++;
          }
        }
      }
    } else {
      Serial.printf("Scores: Flashscore live JSON parse failed (sport %d): %s\n", sportId, err.c_str());
    }
  } else {
    Serial.printf("Scores: Flashscore live HTTP %d (sport %d)\n", httpCode, sportId);
  }
  http.end();
}

void fetchRapidApiFlashscore() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Scores: WiFi not connected, skipping Europa League/VBA.");
    return;
  }
  if (RAPIDAPI_FLASHSCORE_KEY[0] == '\0') {
    Serial.println("Scores: no RAPIDAPI_FLASHSCORE_KEY, skipping Europa League/VBA.");
    return;
  }
  clearScoreEntriesForSource(false, false, true);
  for (int i = 0; i < FS_TOURNAMENT_COUNT; i++) {
    FlashscoreTournament& t = FS_TOURNAMENTS[i];
    if (!t.idCached && !fetchFlashscoreTournamentId(t)) {
      Serial.printf("Scores: Flashscore could not resolve tournament id for %s\n", t.searchName);
    }
  }
  fetchFlashscoreLiveForSport(FS_SPORT_SOCCER);
  fetchFlashscoreLiveForSport(FS_SPORT_BASKETBALL);
}

// ── Fetch scheduling ─────────────────────────────────────────────────
#define SCORES_FOOTBALL_NBA_INTERVAL_MS (5UL * 60000)
#define SCORES_RAPIDAPI_INTERVAL_MS (2UL * 3600000)  // 500 req/month cap on the RapidAPI key — stay conservative

// Underflow trick (same idea as wifi_time.ino) so the first fetch fires
// promptly after boot instead of waiting a full interval.
static uint32_t lastFootballNbaFetchMs = 0 - SCORES_FOOTBALL_NBA_INTERVAL_MS;
static uint32_t lastRapidApiFetchMs = 0 - SCORES_RAPIDAPI_INTERVAL_MS;

// Scans scoreEntries[] for live favorite-team matches and compares against
// favoriteTrack[] (clawd_mochi.ino) to detect kickoff/goal events. Sets
// pendingFavoriteNotify, consumed by checkFavoriteNotifyInterrupt() in
// clawd_mochi.ino's loop(). Cheap enough (a handful of short strcmps) to
// call after every fetch regardless of which source actually changed.
void checkFavoriteMatchChanges() {
  if (favoriteTeamCount == 0) return;
  for (uint8_t i = 0; i < scoreEntryCount; i++) {
    ScoreEntry& e = scoreEntries[i];
    if (!e.valid || !e.isLive) continue;
    if (!teamMatchesFavorite(e.home) && !teamMatchesFavorite(e.away)) continue;

    int8_t slot = -1;
    int8_t freeSlot = -1;
    for (uint8_t s = 0; s < MAX_TRACKED_FAVORITE_MATCHES; s++) {
      if (favoriteTrack[s].valid && strcmp(favoriteTrack[s].home, e.home) == 0 && strcmp(favoriteTrack[s].away, e.away) == 0) {
        slot = s;
        break;
      }
      if (!favoriteTrack[s].valid && freeSlot < 0) freeSlot = s;
    }

    if (slot < 0) {
      // New live favorite match — kickoff event.
      if (freeSlot < 0) continue;  // all 4 tracking slots busy, drop it
      slot = freeSlot;
      snprintf(favoriteTrack[slot].home, sizeof(favoriteTrack[slot].home), "%s", e.home);
      snprintf(favoriteTrack[slot].away, sizeof(favoriteTrack[slot].away), "%s", e.away);
      favoriteTrack[slot].homeScore = e.homeScore;
      favoriteTrack[slot].awayScore = e.awayScore;
      favoriteTrack[slot].valid = true;
      pendingFavoriteNotify = true;
    } else if (favoriteTrack[slot].homeScore != e.homeScore || favoriteTrack[slot].awayScore != e.awayScore) {
      // Score changed since last check — goal event.
      favoriteTrack[slot].homeScore = e.homeScore;
      favoriteTrack[slot].awayScore = e.awayScore;
      pendingFavoriteNotify = true;
    }
  }

  // Free any tracked slot whose match is no longer live (finished/dropped)
  // so it can be reused by a different favorite match later.
  for (uint8_t s = 0; s < MAX_TRACKED_FAVORITE_MATCHES; s++) {
    if (!favoriteTrack[s].valid) continue;
    bool stillLive = false;
    for (uint8_t i = 0; i < scoreEntryCount; i++) {
      ScoreEntry& e = scoreEntries[i];
      if (e.valid && e.isLive && strcmp(e.home, favoriteTrack[s].home) == 0 && strcmp(e.away, favoriteTrack[s].away) == 0) {
        stillLive = true;
        break;
      }
    }
    if (!stillLive) favoriteTrack[s].valid = false;
  }
}

void updateScores() {
  uint32_t now = millis();

  if (now - lastFootballNbaFetchMs >= SCORES_FOOTBALL_NBA_INTERVAL_MS) {
    lastFootballNbaFetchMs = now;
    startFootballDataAutoCycle();
    fetchNbaGames();  // single request, fine to block briefly like wifi_sync.ino's usage fetch
    if (currentMode == MODE_SCORES) drawScoresView();
  }
  tickFootballDataAutoCycle();  // advances the football-data cycle at most one competition per call

  if (now - lastRapidApiFetchMs >= SCORES_RAPIDAPI_INTERVAL_MS) {
    lastRapidApiFetchMs = now;
    fetchRapidApiFlashscore();
    if (currentMode == MODE_SCORES) drawScoresView();
  }

  checkFavoriteMatchChanges();
}

// ── Display: one card at a time, rotating, favorite-live pinned ─────
static uint32_t scoreRotateAt = 0;
static uint8_t scoreShowIdx = 0;
static bool scoreShowIsFavorite = false;
#define SCORE_ROTATE_INTERVAL_MS 6000

int8_t findFavoriteLiveEntry() {
  for (uint8_t i = 0; i < scoreEntryCount; i++) {
    if (scoreEntries[i].valid && scoreEntries[i].isLive && (teamMatchesFavorite(scoreEntries[i].home) || teamMatchesFavorite(scoreEntries[i].away))) {
      return i;
    }
  }
  return -1;
}

// ── Favorite-team badge: a colored circle + short abbreviation, standing
// in for a real crest without fetching/decoding one. Only drawn for
// favorite teams (not every team crossing the screen) — the color is a
// deterministic hash of the team name, not real club colors (which aren't
// reliably known for arbitrary teams across 15 leagues), so it's a stable
// per-team identifier rather than an accuracy claim.
uint16_t hashTeamColor(const char* name) {
  uint32_t h = 5381;
  for (const char* p = name; *p; p++) h = ((h << 5) + h) + (unsigned char)*p;  // djb2
  static const uint16_t PALETTE[] = {
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF, 0x051D, 0x781F, 0xF81F, 0xFC9F, 0xFBE0,
  };
  return PALETTE[h % (sizeof(PALETTE) / sizeof(PALETTE[0]))];
}

void teamAbbrev(char* out, size_t outSize, const char* name) {
  size_t oi = 0;
  bool atWordStart = true;
  for (const char* p = name; *p && oi < outSize - 1; p++) {
    if (*p == ' ') {
      atWordStart = true;
      continue;
    }
    if (atWordStart) {
      out[oi++] = toupper((unsigned char)*p);
      atWordStart = false;
    }
  }
  out[oi] = '\0';
}

void drawTeamBadge(int16_t cx, int16_t cy, const char* name) {
  uint16_t col = hashTeamColor(name);
  tft.fillCircle(cx, cy, 15, col);
  tft.drawCircle(cx, cy, 15, C_WHITE);
  char abbr[4];
  teamAbbrev(abbr, sizeof(abbr), name);
  tft.setTextColor(C_BLACK);
  tft.setTextSize(1);
  tft.setCursor(cx - (int16_t)strlen(abbr) * 3, cy - 4);
  tft.print(abbr);
}

// Tiny ball icon (soccer/basketball) next to the league tag — geometric,
// not a real logo, but a quick visual "what sport is this" cue.
void drawBallIcon(int16_t cx, int16_t cy, bool basketball) {
  if (basketball) {
    tft.fillCircle(cx, cy, 8, tft.color565(230, 126, 34));
    tft.drawLine(cx - 8, cy, cx + 8, cy, C_BLACK);
    tft.drawLine(cx, cy - 8, cx, cy + 8, C_BLACK);
    tft.drawCircle(cx, cy, 8, C_BLACK);
  } else {
    tft.fillCircle(cx, cy, 8, C_WHITE);
    tft.drawCircle(cx, cy, 8, C_BLACK);
    tft.fillCircle(cx, cy, 2, C_BLACK);
    tft.fillCircle(cx - 5, cy - 3, 1, C_BLACK);
    tft.fillCircle(cx + 5, cy - 3, 1, C_BLACK);
    tft.fillCircle(cx - 3, cy + 5, 1, C_BLACK);
    tft.fillCircle(cx + 3, cy + 5, 1, C_BLACK);
  }
}

int16_t textWidthPx(uint8_t size, const char* s) {
  return (int16_t)strlen(s) * 6 * size;
}

// One scoreboard row: badge (favorite teams only) + name on the left,
// score right-aligned on the right — same row, not stacked separately.
void drawTeamRow(int16_t y, const char* name, int8_t score, bool hasScore, bool isFav, uint16_t nameColor) {
  int16_t textX = 14;
  if (isFav) {
    drawTeamBadge(28, y + 8, name);
    textX = 50;
  }
  char nameBuf[14];
  snprintf(nameBuf, sizeof(nameBuf), "%s", name);
  tft.setTextColor(nameColor);
  tft.setTextSize(2);
  tft.setCursor(textX, y);
  tft.print(nameBuf);

  if (hasScore) {
    char scoreBuf[4];
    snprintf(scoreBuf, sizeof(scoreBuf), "%d", score);
    int16_t w = textWidthPx(3, scoreBuf);
    tft.setTextColor(C_GREEN);
    tft.setTextSize(3);
    tft.setCursor(DISP_W - 14 - w, y - 4);
    tft.print(scoreBuf);
  }
}

#define LIVE_DOT_X 14
#define LIVE_DOT_Y 197

void drawScoresView() {
  tft.fillScreen(C_DARKBG);

  if (scoreEntryCount == 0) {
    tft.fillRect(0, 0, DISP_W, 4, C_MUTED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(12, 14);
    tft.print("Live Scores");
    tft.setTextColor(C_MUTED);
    tft.setTextSize(1);
    tft.setCursor(12, 70);
    tft.print("waiting for data...");
    return;
  }

  if (scoreShowIdx >= scoreEntryCount) scoreShowIdx = 0;
  ScoreEntry& e = scoreEntries[scoreShowIdx];
  bool upcoming = (e.homeScore < 0 && e.awayScore < 0 && !e.isLive);
  uint16_t barColor = e.isLive ? tft.color565(255, 60, 60) : (upcoming ? C_ORANGE : C_MUTED);

  // Status accent bar — color reads live/upcoming/finished at a glance.
  tft.fillRect(0, 0, DISP_W, 4, barColor);

  // Header: sport icon + league tag, rotation position top-right.
  bool basketball = (e.league == SL_NBA || e.league == SL_VBA);
  drawBallIcon(16, 20, basketball);
  tft.setTextColor(C_MUTED);
  tft.setTextSize(1);
  tft.setCursor(30, 16);
  tft.print(SCORE_LEAGUE_TAG[e.league]);
  char posBuf[8];
  snprintf(posBuf, sizeof(posBuf), "%d/%d", scoreShowIdx + 1, scoreEntryCount);
  tft.setCursor(DISP_W - 12 - textWidthPx(1, posBuf), 16);
  tft.print(posBuf);

  tft.drawFastHLine(12, 34, DISP_W - 24, C_MUTED);

  bool homeIsFav = teamMatchesFavorite(e.home);
  bool awayIsFav = teamMatchesFavorite(e.away);
  bool hasScore = (e.homeScore >= 0 && e.awayScore >= 0);
  drawTeamRow(52, e.home, e.homeScore, hasScore, homeIsFav, C_WHITE);
  tft.drawFastHLine(12, 100, DISP_W - 24, tft.color565(30, 30, 34));
  drawTeamRow(112, e.away, e.awayScore, hasScore, awayIsFav, C_WHITE);

  // Status line — live gets a pulsing dot (see updateScoresLiveDot), color
  // accent matches the top bar so live/upcoming/finished reads consistently.
  if (e.isLive) {
    tft.fillCircle(LIVE_DOT_X, LIVE_DOT_Y, 4, barColor);
    tft.setTextColor(C_WHITE);
  } else {
    tft.setTextColor(upcoming ? C_ORANGE : C_MUTED);
  }
  tft.setTextSize(2);
  tft.setCursor(28, LIVE_DOT_Y - 8);
  tft.print(e.status);

  if (scoreShowIsFavorite) {
    tft.setTextColor(C_ORANGE);
    tft.setTextSize(1);
    tft.setCursor(12, 224);
    tft.print("* your team");
  }
}

// Redraws just the live-status dot on its own faster tick (not the whole
// screen) so a live match's status pulses like a heartbeat without the
// full-card flicker a repeated drawScoresView() would cause.
static uint32_t liveDotLastToggle = 0;
static bool liveDotOn = true;

void updateScoresLiveDot() {
  if (currentMode != MODE_SCORES) return;
  if (scoreEntryCount == 0 || scoreShowIdx >= scoreEntryCount) return;
  if (!scoreEntries[scoreShowIdx].isLive) return;
  uint32_t now = millis();
  if (now - liveDotLastToggle < 500) return;
  liveDotLastToggle = now;
  liveDotOn = !liveDotOn;
  tft.fillCircle(LIVE_DOT_X, LIVE_DOT_Y, 4, liveDotOn ? tft.color565(255, 60, 60) : C_DARKBG);
}

// Rotates the shown card every ~6s while MODE_SCORES is on screen. A live
// game involving a favorite team is pinned (kept shown) instead of rotated
// away from, per the round-robin below.
void updateScoresViewIfShown() {
  if (currentMode != MODE_SCORES) return;
  uint32_t now = millis();
  if (now - scoreRotateAt < SCORE_ROTATE_INTERVAL_MS) {
    updateScoresLiveDot();
    return;
  }
  scoreRotateAt = now;

  if (scoreEntryCount == 0) {
    drawScoresView();
    return;
  }

  int8_t favIdx = findFavoriteLiveEntry();
  if (favIdx >= 0) {
    scoreShowIdx = favIdx;
    scoreShowIsFavorite = true;
  } else {
    scoreShowIsFavorite = false;
    uint8_t next = scoreShowIdx;
    for (uint8_t tries = 0; tries < scoreEntryCount; tries++) {
      next = (next + 1) % scoreEntryCount;
      if (scoreEntries[next].valid) {
        scoreShowIdx = next;
        break;
      }
    }
  }
  drawScoresView();
}
