// Part of the Clawd Mochi sketch. Direct WiFi HTTPS fetching of Claude usage stats.
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

// Global stats variables declared in clawd_mochi.ino
extern int8_t   usageSessionPct;
extern int8_t   usageWeeklyPct;
extern uint16_t usageSessionResetMin;
extern uint16_t usageWeeklyResetMin;
extern uint32_t usageReceivedMillis;
extern Mode     currentMode;
void drawUsageView();

static uint32_t lastUsageFetchMs = 0;
static const uint32_t FETCH_INTERVAL_MS = 10 * 60 * 1000; // 10 minutes
static bool initialFetchDone = false;

void fetchClaudeUsage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Sync: Not connected to WiFi. Skipping fetch.");
    return;
  }

  // Ensure secrets exist
  String token = CLAUDE_ACCESS_TOKEN;
  if (token.length() == 0) {
    Serial.println("WiFi Sync: No CLAUDE_ACCESS_TOKEN provided in secrets.h.");
    return;
  }

  Serial.println("WiFi Sync: Fetching Claude usage from Anthropic API...");

  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate verification for simplicity

  HTTPClient http;
  
  // Set up connection to Anthropic messages endpoint
  if (http.begin(client, "https://api.anthropic.com/v1/messages")) {
    // Add required Anthropic headers
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("anthropic-version", "2023-06-01");
    http.addHeader("anthropic-beta", "oauth-2025-04-20");
    http.addHeader("User-Agent", "claude-code/2.1.5");
    http.addHeader("Content-Type", "application/json");

    // Collect headers from response
    const char* headerKeys[] = {
      "anthropic-ratelimit-unified-5h-utilization",
      "anthropic-ratelimit-unified-7d-utilization",
      "anthropic-ratelimit-unified-5h-reset",
      "anthropic-ratelimit-unified-7d-reset"
    };
    size_t headerKeysSize = sizeof(headerKeys) / sizeof(char*);
    http.collectHeaders(headerKeys, headerKeysSize);

    // Send a tiny request payload to trigger the headers response
    String payload = "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";
    
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      Serial.print("WiFi Sync: HTTP Response Code: ");
      Serial.println(httpResponseCode);

      String sessionUtilStr = http.header("anthropic-ratelimit-unified-5h-utilization");
      String weekUtilStr    = http.header("anthropic-ratelimit-unified-7d-utilization");
      String sessionResetStr = http.header("anthropic-ratelimit-unified-5h-reset");
      String weekResetStr    = http.header("anthropic-ratelimit-unified-7d-reset");

      if (sessionUtilStr.length() > 0 && weekUtilStr.length() > 0) {
        float sessionUtil = sessionUtilStr.toFloat();
        float weekUtil = weekUtilStr.toFloat();

        usageSessionPct = (int8_t)round(sessionUtil * 100);
        usageWeeklyPct  = (int8_t)round(weekUtil * 100);

        // Get time for calculating reset countdowns
        time_t nowSecs;
        time(&nowSecs);

        long sessionResetSec = sessionResetStr.toInt();
        long weeklyResetSec  = weekResetStr.toInt();

        long sessionMin = (sessionResetSec - nowSecs) / 60;
        long weeklyMin  = (weeklyResetSec - nowSecs) / 60;

        usageSessionResetMin = (sessionMin > 0) ? (uint16_t)sessionMin : 0;
        usageWeeklyResetMin  = (weeklyMin > 0) ? (uint16_t)weeklyMin : 0;
        usageReceivedMillis  = millis();

        Serial.printf("WiFi Sync Success: Session %d%% (resets in %d min), Weekly %d%% (resets in %d min)\n", 
                      usageSessionPct, usageSessionResetMin, usageWeeklyPct, usageWeeklyResetMin);

        // Redraw if we are currently looking at the usage mode
        if (currentMode == MODE_USAGE) {
          drawUsageView();
        }
      } else {
        Serial.println("WiFi Sync Error: Headers not found in response.");
      }
    } else {
      Serial.print("WiFi Sync Error: POST failed, error: ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
  } else {
    Serial.println("WiFi Sync Error: Unable to connect to API.");
  }
}

void updateWiFiSync() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  uint32_t now = millis();

  // Run initial fetch once WiFi is connected
  if (!initialFetchDone) {
    // Only run if time has synced (so we have a valid epoch for reset calculations)
    // wifi_time.ino sets timeSynced = true when NTP succeeds.
    extern bool timeSynced;
    if (timeSynced) {
      fetchClaudeUsage();
      lastUsageFetchMs = now;
      initialFetchDone = true;
    }
    return;
  }

  // Periodic updates
  if (now - lastUsageFetchMs >= FETCH_INTERVAL_MS) {
    fetchClaudeUsage();
    lastUsageFetchMs = now;
  }
}
