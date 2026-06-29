// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  WEATHER VIEW
// ═════════════════════════════════════════════════════════════

WeatherCondition wmoToCondition(uint16_t wmo) {
  if (wmo <= 1) return WC_CLEAR;
  if (wmo == 45 || wmo == 48) return WC_FOG;
  if ((wmo >= 71 && wmo <= 77) || wmo == 85 || wmo == 86) return WC_SNOWY;
  if (wmo == 70 || wmo == 771) return WC_WINDY;
  if (wmo >= 51 && wmo <= 82) return WC_RAIN;
  if (wmo == 95 || wmo == 96 || wmo == 99) return WC_STORM;
  return WC_CLOUDY;
}

#define PX 4

void fillPx(int16_t x, int16_t y, uint16_t col) {
  if (x >= 0 && x < 60 && y >= 0 && y < 30) {
    tft.fillRect(x * PX, y * PX, PX, PX, col);
  }
}

void drawSunBody(int8_t sx, int8_t sy, uint16_t col) {
  for (int8_t r = 0; r < 16; r++) {
    uint16_t rowVal = pgm_read_word(&SUN_MASK[r]);
    for (int8_t c = 0; c < 16; c++) {
      if ((rowVal >> (15 - c)) & 1) {
        fillPx(sx + c, sy + r, col);
      }
    }
  }
}

void drawSunRays(int8_t sx, int8_t sy, int8_t len, uint16_t col) {
  int8_t cx = sx + 8;
  int8_t cy = sy + 8;
  for (int8_t i = 0; i < len; i++) {
    fillPx(cx, cy - 9 - i, col);         // N
    fillPx(cx, cy + 9 + i, col);         // S
    fillPx(cx - 9 - i, cy, col);         // W
    fillPx(cx + 9 + i, cy, col);         // E
    fillPx(cx - 7 - i, cy - 7 - i, col); // NW
    fillPx(cx + 7 - i, cy - 7 - i, col); // NE
    fillPx(cx - 7 - i, cy + 7 + i, col); // SW
    fillPx(cx + 7 - i, cy + 7 + i, col); // SE
  }
}

void drawCloud(int8_t cx, int8_t cy, const uint16_t* mask, uint16_t col) {
  for (int8_t r = 0; r < 7; r++) {
    uint16_t rowVal = pgm_read_word(&mask[r]);
    for (int8_t c = 0; c < 14; c++) {
      if ((rowVal >> (13 - c)) & 1) {
        fillPx(cx + c, cy + r, col);
      }
    }
  }
}

void drawMiniCloud(int8_t cx, int8_t cy, uint16_t col) {
  for (int8_t r = 0; r < 4; r++) {
    uint8_t rowVal = pgm_read_byte(&MINI_CLOUD_MASK[r]);
    for (int8_t c = 0; c < 8; c++) {
      if ((rowVal >> (7 - c)) & 1) {
        fillPx(cx + c, cy + r, col);
      }
    }
  }
}

void drawMiniClaude(int8_t cx, int8_t cy, int8_t dir, uint16_t bodyCol, uint16_t eyeCol) {
  // Ears
  fillPx(cx, cy, bodyCol);
  fillPx(cx + 4, cy, bodyCol);
  // Head top
  for (int8_t c = 0; c < 5; c++) {
    fillPx(cx + c, cy + 1, bodyCol);
  }
  // Eyes
  fillPx(cx, cy + 2, bodyCol);
  fillPx(cx + 4, cy + 2, bodyCol);
  if (dir == 1) {
    fillPx(cx + 1, cy + 2, bodyCol);
    fillPx(cx + 2, cy + 2, eyeCol);
    fillPx(cx + 3, cy + 2, eyeCol);
  } else {
    fillPx(cx + 1, cy + 2, eyeCol);
    fillPx(cx + 2, cy + 2, eyeCol);
    fillPx(cx + 3, cy + 2, bodyCol);
  }
  // Chin
  fillPx(cx + 1, cy + 3, bodyCol);
  fillPx(cx + 2, cy + 3, bodyCol);
  fillPx(cx + 3, cy + 3, bodyCol);
}

void drawLightning(int8_t sx, uint16_t col) {
  for (uint8_t i = 0; i < 28; i++) {
    int8_t dx = pgm_read_byte(&LIGHTNING_BOLT[i][0]);
    int8_t dy = pgm_read_byte(&LIGHTNING_BOLT[i][1]);
    fillPx(sx + dx, 2 + dy, col);
    fillPx(sx + dx + 1, 2 + dy, col);
  }
}

void initWeatherAnimation() {
  wxFrame = 0;
  wxLastFrame = millis();
  lightningActive = false;
  
  clearBirdX = -15;
  clearBirdY = 6;
  miniCloudX = 5;
  miniCloudY = 3;
  planeX = -40;
  planeY = 12;
  claudeX = 20;
  claudeDir = 1;
  
  if (wx.hasData) {
    if (wx.cond == WC_RAIN || wx.cond == WC_STORM || wx.cond == WC_SNOWY || wx.cond == WC_WINDY) {
      for (uint8_t i = 0; i < 30; i++) {
        drops[i].x = random(0, 60);
        drops[i].y = (wx.cond == WC_WINDY) ? random(2, 26) : random(-10, 30);
      }
    } else if (wx.cond == WC_CLOUDY) {
      cloudX[0] = 5;
      cloudX[1] = 35;
    }
  }
}

void drawWeatherView() {
  uint16_t skyCol = C_DARKBG;
  const char* condStr = "UNKNOWN";
  uint16_t accentCol = C_WHITE;
  
  if (wx.hasData) {
    switch (wx.cond) {
      case WC_CLEAR:
        skyCol = tft.color565(100, 180, 255);
        condStr = "CLEAR";
        accentCol = tft.color565(255, 220, 100);
        break;
      case WC_CLOUDY:
        skyCol = tft.color565(100, 120, 145);
        condStr = "CLOUDY";
        accentCol = tft.color565(200, 220, 240);
        break;
      case WC_FOG:
        skyCol = tft.color565(170, 170, 160);
        condStr = "FOGGY";
        accentCol = tft.color565(220, 220, 220);
        break;
      case WC_RAIN:
        skyCol = tft.color565(20, 30, 55);
        condStr = "RAINY";
        accentCol = tft.color565(100, 200, 255);
        break;
      case WC_STORM:
        skyCol = tft.color565(10, 15, 25);
        condStr = "STORM";
        accentCol = tft.color565(255, 255, 150);
        break;
      case WC_SNOWY:
        skyCol = tft.color565(140, 165, 185);
        condStr = "SNOWY";
        accentCol = tft.color565(245, 245, 255);
        break;
      case WC_WINDY:
        skyCol = tft.color565(115, 135, 155);
        condStr = "WINDY";
        accentCol = tft.color565(180, 210, 230);
        break;
    }
  } else {
    skyCol = C_DARKBG;
    condStr = "NO DATA";
    accentCol = C_MUTED;
  }
  
  // Fill the weather scene zone (y=0 to 120, i.e., 30 logical px)
  tft.fillRect(0, 0, DISP_W, 120, skyCol);
  
  // Clear the info zone below (y=120 to 240)
  tft.fillRect(0, 120, DISP_W, DISP_H - 120, C_DARKBG);
  
  tft.setTextColor(C_WHITE);
  
  if (wx.hasData) {
    // Temperature
    tft.setTextSize(5);
    tft.setCursor(12, 125);
    tft.print(wx.tempC);
    tft.print("C");
    
    // Feels-like and humidity
    tft.setTextSize(2);
    tft.setCursor(12, 165);
    tft.print("Feels ");
    tft.print(wx.feelsC);
    tft.print("C  ");
    tft.print(wx.humidity);
    tft.print("%");
    
    // Condition label
    tft.setTextSize(2);
    tft.setTextColor(accentCol);
    tft.setCursor(12, 185);
    if (wxCondStr.length() > 0) {
      tft.print(wxCondStr);
    } else {
      tft.print(condStr);
    }
    
    // Location label
    tft.setTextSize(2);
    tft.setTextColor(C_MUTED);
    tft.setCursor(12, 205);
    tft.print(wxLoc);
  } else {
    tft.setTextSize(5);
    tft.setCursor(12, 125);
    tft.print("--C");
    
    tft.setTextSize(2);
    tft.setCursor(12, 165);
    tft.setTextColor(C_MUTED);
    tft.print("Feels --C  --%");
    
    tft.setTextSize(2);
    tft.setTextColor(accentCol);
    tft.setCursor(12, 185);
    tft.print(condStr);
    
    // Location label
    tft.setTextSize(2);
    tft.setTextColor(C_MUTED);
    tft.setCursor(12, 205);
    tft.print(wxLoc);
  }
  
  initWeatherAnimation();
  
  // If clear/cloudy/snowy/windy, draw initial static elements
  if (wx.hasData) {
    if (wx.cond == WC_CLEAR) {
      tft.fillRect(0, 29 * PX, DISP_W, PX, tft.color565(34, 139, 34)); // draw grass
      drawSunBody(42, 2, tft.color565(255, 215, 0));
      drawSunRays(42, 2, 4, tft.color565(255, 215, 0));
    } else if (wx.cond == WC_CLOUDY) {
      drawCloud(cloudX[0], 6, CLOUD_MASK_1, C_WHITE);
      drawCloud(cloudX[1], 14, CLOUD_MASK_2, C_WHITE);
    } else if (wx.cond == WC_SNOWY) {
      tft.fillRect(0, 29 * PX, DISP_W, PX, C_WHITE); // draw snow ground
    } else if (wx.cond == WC_WINDY) {
      tft.fillRect(0, 29 * PX, DISP_W, PX, tft.color565(34, 139, 34)); // draw grass
    }
  }
}

void updateStormRainAnimation(uint8_t speed, uint16_t skyCol, uint16_t dropCol) {
  for (uint8_t i = 0; i < 30; i++) {
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x - dy, drops[i].y + dy, skyCol);
    }
    
    drops[i].y += speed;
    drops[i].x -= 1;
    
    if (drops[i].y >= 30 || drops[i].x < -2) {
      drops[i].y = -2;
      drops[i].x = random(5, 65);
    }
    
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x - dy, drops[i].y + dy, dropCol);
    }
  }
}

void updateRainAnimation(uint8_t speed, uint16_t skyCol, uint16_t dropCol) {
  for (uint8_t i = 0; i < 30; i++) {
    // Erase old drop (1x3 strip)
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x, drops[i].y + dy, skyCol);
    }
    
    // Advance drop
    drops[i].y += speed;
    
    // Wrap if off screen
    if (drops[i].y >= 30) {
      drops[i].y = -2;
      drops[i].x = random(0, 60);
    }
    
    // Draw new drop (1x3 strip)
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x, drops[i].y + dy, dropCol);
    }
  }
}

void fillSnowflake(int16_t x, int16_t y, uint16_t col) {
  if (x >= 0 && x < 60 && y >= 0 && y < 30) {
    int16_t cx = x * PX;
    int16_t cy = y * PX;
    tft.fillRect(cx + 1, cy + 1, 2, 2, col);
    tft.drawPixel(cx + 1, cy, col);
    tft.drawPixel(cx + 2, cy, col);
    tft.drawPixel(cx + 1, cy + 3, col);
    tft.drawPixel(cx + 2, cy + 3, col);
    tft.drawPixel(cx, cy + 1, col);
    tft.drawPixel(cx, cy + 2, col);
    tft.drawPixel(cx + 3, cy + 1, col);
    tft.drawPixel(cx + 3, cy + 2, col);
  }
}

void updateSnowAnimation(uint16_t skyCol, uint16_t snowCol) {
  for (uint8_t i = 0; i < 20; i++) {
    // 1. Erase at the exact previous position (where it was left in the previous frame)
    int8_t prevSway = (int8_t)(sin((wxFrame - 1 + i * 12) * 0.15) * 3.0);
    fillPx(drops[i].x + prevSway, drops[i].y, skyCol);
    
    // 2. Advance y-position (slow fall)
    if (wxFrame % 2 == 0) {
      drops[i].y += 1;
    }
    
    // Wrap if it hits the ground (y=29 is the ground)
    if (drops[i].y >= 29) {
      drops[i].y = -1;
      drops[i].x = random(-3, 63);
    }
    
    // 3. Draw at the new position (using current frame's sway)
    int8_t currSway = (int8_t)(sin((wxFrame + i * 12) * 0.15) * 3.0);
    fillSnowflake(drops[i].x + currSway, drops[i].y, snowCol);
  }
}

void updateWindAnimation(uint16_t skyCol) {
  // Vibrant blue breeze gradient
  uint16_t c1 = tft.color565(100, 220, 255); // bright cyan-blue lead
  uint16_t c2 = tft.color565(60, 160, 255);  // medium blue body
  uint16_t c3 = tft.color565(30, 110, 220);  // dark blue tail
  
  // Use drops[0..5] for wind lines
  for (uint8_t i = 0; i < 6; i++) {
    // 1. Erase old wavy wind line (length 6)
    int8_t prevX = drops[i].x - 3;
    for (int8_t dx = 0; dx < 6; dx++) {
      int8_t px = prevX - dx;
      int8_t py = drops[i].y + (int8_t)(sin(px * 0.25) * 1.8);
      fillPx(px, py, skyCol);
    }
    
    // 2. Advance wind particle
    drops[i].x += 3;
    
    // Wrap if off right edge
    if (drops[i].x >= 70) {
      drops[i].x = -6;
      drops[i].y = random(3, 23);
    }
    
    // 3. Draw new wavy wind line
    for (int8_t dx = 0; dx < 6; dx++) {
      int8_t px = drops[i].x - dx;
      int8_t py = drops[i].y + (int8_t)(sin(px * 0.25) * 1.8);
      
      uint16_t col;
      if (dx < 2)       col = c1;
      else if (dx < 4)  col = c2;
      else              col = c3;
      
      fillPx(px, py, col);
    }
  }

  // Use drops[6..14] for blowing leaves
  uint16_t leafColors[] = { tft.color565(34, 139, 34), tft.color565(218, 165, 32), tft.color565(139, 69, 19) };
  for (uint8_t i = 6; i < 15; i++) {
    // 1. Erase old leaf
    fillPx(drops[i].x, drops[i].y, skyCol);
    fillPx(drops[i].x - 1, drops[i].y + 1, skyCol);
    
    // 2. Advance leaf
    drops[i].x += 2;
    if (wxFrame % 2 == 0) {
      drops[i].y += (random(3) - 1);
    }
    
    // Wrap if off screen
    if (drops[i].x >= 62 || drops[i].y >= 28 || drops[i].y < 1) {
      drops[i].x = -2;
      drops[i].y = random(2, 22);
    }
    
    // 3. Draw leaf
    uint16_t leafCol = leafColors[i % 3];
    fillPx(drops[i].x, drops[i].y, leafCol);
    fillPx(drops[i].x - 1, drops[i].y + (wxFrame % 4 < 2 ? 1 : 0), leafCol);
  }
}

void updateWeatherView() {
  if (millis() - wxLastFrame < 100) return;
  wxLastFrame = millis();
  wxFrame++;
  
  if (!wx.hasData) return;
  
  uint16_t skyCol = C_DARKBG;
  switch (wx.cond) {
    case WC_CLEAR:  skyCol = tft.color565(100, 180, 255); break;
    case WC_CLOUDY: skyCol = tft.color565(100, 120, 145); break;
    case WC_FOG:    skyCol = tft.color565(170, 170, 160); break;
    case WC_RAIN:   skyCol = tft.color565(20, 30, 55);    break;
    case WC_STORM:  skyCol = tft.color565(10, 15, 25);    break;
    case WC_SNOWY:  skyCol = tft.color565(140, 165, 185); break;
    case WC_WINDY:  skyCol = tft.color565(115, 135, 155); break;
  }
  
  if (wx.cond == WC_RAIN) {
    updateRainAnimation(2, skyCol, tft.color565(100, 200, 255));
  }
  else if (wx.cond == WC_STORM) {
    updateStormRainAnimation(3, skyCol, tft.color565(150, 220, 255));
    
    if (lightningActive) {
      if (wxFrame == lightningFrame + 1) {
        // Frame 1: Return sky to normal, redraw slanted drops, and draw visible lightning bolt
        tft.fillRect(0, 0, DISP_W, 120, skyCol);
        for (uint8_t i = 0; i < 30; i++) {
          for (int8_t dy = 0; dy < 3; dy++) {
            fillPx(drops[i].x - dy, drops[i].y + dy, tft.color565(150, 220, 255));
          }
        }
        drawLightning(lightningX, tft.color565(255, 255, 150));
      }
      else if (wxFrame >= lightningFrame + 2 && wxFrame <= lightningFrame + 4) {
        // Frame 2, 3, 4: Keep lightning bolt visible (redraw to sit on top of rain drops)
        drawLightning(lightningX, tft.color565(255, 255, 150));
      }
      else if (wxFrame == lightningFrame + 5) {
        // Frame 5: Erase lightning bolt
        drawLightning(lightningX, skyCol);
        lightningActive = false;
      }
    }
    
    if (!lightningActive && random(0, 70) == 0) {
      lightningActive = true;
      lightningFrame = wxFrame;
      lightningX = random(5, 50);
      
      // Flash sky white (exactly 1 frame)
      tft.fillRect(0, 0, DISP_W, 120, C_WHITE);
    }
  }
  else if (wx.cond == WC_CLEAR) {
    uint16_t sunCol = tft.color565(255, 215, 0);
    uint16_t flowerCol = tft.color565(255, 60, 60);
    uint16_t stemCol = tft.color565(34, 100, 34);
    uint16_t birdCol = tft.color565(40, 70, 120);
    uint16_t planeCol = tft.color565(180, 180, 180);
    uint16_t trunkCol = tft.color565(139, 69, 19);
    uint16_t leafCol = tft.color565(34, 139, 34);
    uint16_t claudeBody = tft.color565(255, 180, 100);
    
    // 1. Erase old bird
    if (clearBirdX >= -3 && clearBirdX < 60) {
      fillPx(clearBirdX, clearBirdY, skyCol);
      fillPx(clearBirdX - 1, clearBirdY - 1, skyCol);
      fillPx(clearBirdX + 1, clearBirdY - 1, skyCol);
      fillPx(clearBirdX - 1, clearBirdY + 1, skyCol);
      fillPx(clearBirdX + 1, clearBirdY + 1, skyCol);
    }
    
    // 2. Erase old plane
    if (planeX >= 0 && planeX < 65) {
      fillPx(planeX, planeY, skyCol);
      fillPx(planeX - 1, planeY, skyCol);
      fillPx(planeX - 2, planeY, skyCol);
      fillPx(planeX - 3, planeY, skyCol);
      fillPx(planeX - 1, planeY - 1, skyCol);
      fillPx(planeX - 1, planeY + 1, skyCol);
      fillPx(planeX - 3, planeY - 1, skyCol);
    }
    
    // 3. Erase old mini cloud
    if (miniCloudX >= -8 && miniCloudX < 60) {
      drawMiniCloud(miniCloudX, miniCloudY, skyCol);
    }
    
    // 4. Erase old mini Claude
    int8_t claudeBob = (claudeX % 2 == 0) ? 0 : 1;
    int8_t claudeY = 25 - claudeBob;
    for (int8_t r = 0; r < 4; r++) {
      for (int8_t c = 0; c < 5; c++) {
        fillPx(claudeX + c, claudeY + r, skyCol);
      }
    }
    
    // 5. Update positions
    clearBirdX++;
    if (clearBirdX >= 60) {
      clearBirdX = -20;
      clearBirdY = random(3, 10);
    }
    
    if (wxFrame % 2 == 0) {
      planeX++;
      if (planeX >= 80) {
        planeX = -80;
        planeY = random(4, 15);
      }
    }
    
    // Cloud drifts 1px every frame for smoothness and speed
    miniCloudX++;
    if (miniCloudX >= 60) {
      miniCloudX = -10;
      miniCloudY = random(2, 13); // random height!
    }
    
    // Mini Claude walks back and forth
    if (wxFrame % 3 == 0) {
      claudeX += claudeDir;
      if (claudeX >= 50) {
        claudeDir = -1;
      } else if (claudeX <= 15) {
        claudeDir = 1;
      }
    }
    
    // 6. Draw sun (at side sx=42, sy=2)
    drawSunRays(42, 2, 5, skyCol);
    drawSunBody(42, 2, sunCol);
    int8_t rayLen = 4 + (int8_t)pgm_read_byte(&PULSE_LUT[wxFrame % 16]);
    drawSunRays(42, 2, rayLen, sunCol);
    
    // 7. Draw mini cloud
    if (miniCloudX >= -8 && miniCloudX < 60) {
      drawMiniCloud(miniCloudX, miniCloudY, C_WHITE);
    }
    
    // 8. Draw flying plane
    if (planeX >= 0 && planeX < 60) {
      fillPx(planeX, planeY, planeCol);
      fillPx(planeX - 1, planeY, planeCol);
      fillPx(planeX - 2, planeY, planeCol);
      fillPx(planeX - 3, planeY, planeCol);
      fillPx(planeX - 1, planeY - 1, planeCol);
      fillPx(planeX - 1, planeY + 1, planeCol);
      fillPx(planeX - 3, planeY - 1, planeCol);
    }
    
    // 9. Draw flying bird
    if (clearBirdX >= -3 && clearBirdX < 60) {
      fillPx(clearBirdX, clearBirdY, birdCol);
      if (wxFrame % 6 < 3) {
        fillPx(clearBirdX - 1, clearBirdY - 1, birdCol);
        fillPx(clearBirdX + 1, clearBirdY - 1, birdCol);
      } else {
        fillPx(clearBirdX - 1, clearBirdY + 1, birdCol);
        fillPx(clearBirdX + 1, clearBirdY + 1, birdCol);
      }
    }
    
    // 10. Draw swaying flowers
    int8_t sway = (wxFrame % 12 < 6) ? 0 : 1;
    int8_t flowerX[2] = {25, 48};
    for (uint8_t i = 0; i < 2; i++) {
      fillPx(flowerX[i], 27, skyCol);
      fillPx(flowerX[i] + 1, 27, skyCol);
      fillPx(flowerX[i] - 1, 27, skyCol);
      
      fillPx(flowerX[i], 28, stemCol);
      fillPx(flowerX[i] + sway, 27, flowerCol);
    }
    
    // 11. Draw mini Claude (bobbing walk)
    int8_t drawClaudeBob = (claudeX % 2 == 0) ? 0 : 1;
    int8_t drawClaudeY = 25 - drawClaudeBob;
    drawMiniClaude(claudeX, drawClaudeY, claudeDir, claudeBody, C_DARKBG);
    
    // 12. Draw small tree on the left (x=6)
    fillPx(6, 27, trunkCol);
    fillPx(6, 28, trunkCol);
    fillPx(6, 23, leafCol);
    
    fillPx(5, 24, leafCol);
    fillPx(6, 24, leafCol);
    fillPx(7, 24, leafCol);
    
    fillPx(4, 25, leafCol);
    fillPx(5, 25, leafCol);
    fillPx(6, 25, leafCol);
    fillPx(7, 25, leafCol);
    fillPx(8, 25, leafCol);
    
    fillPx(5, 26, leafCol);
    fillPx(6, 26, leafCol);
    fillPx(7, 26, leafCol);
  }
  else if (wx.cond == WC_CLOUDY) {
    if (wxFrame % 3 == 0) {
      for (uint8_t i = 0; i < 2; i++) {
        drawCloud(cloudX[i], 6 + i * 8, i == 0 ? CLOUD_MASK_1 : CLOUD_MASK_2, skyCol);
        cloudX[i]++;
        if (cloudX[i] >= 65) cloudX[i] = -14;
        drawCloud(cloudX[i], 6 + i * 8, i == 0 ? CLOUD_MASK_1 : CLOUD_MASK_2, C_WHITE);
      }
    }
  }
  else if (wx.cond == WC_FOG) {
    int8_t shift = (wxFrame / 2) % 60;
    int8_t bands[4] = {4, 11, 18, 25};
    for (uint8_t b = 0; b < 4; b++) {
      int8_t by = bands[b];
      for (int8_t y = by; y < by + 2; y++) {
        for (int8_t x = 0; x < 60; x++) {
          bool isWhite = ((x - shift) ^ y) & 1;
          uint16_t col = isWhite ? tft.color565(240, 240, 235) : tft.color565(190, 190, 185);
          fillPx(x, y, col);
        }
      }
    }
  }
  else if (wx.cond == WC_SNOWY) {
    updateSnowAnimation(skyCol, C_WHITE);
  }
  else if (wx.cond == WC_WINDY) {
    // Swaying tree on the left (x=6)
    uint16_t trunkCol = tft.color565(139, 69, 19);
    uint16_t leafCol = tft.color565(34, 139, 34);
    
    int8_t sway = (wxFrame % 8 < 4) ? 1 : 2;
    
    // Erase tree
    tft.fillRect(3 * PX, 22 * PX, 10 * PX, 7 * PX, skyCol);
    
    // Draw trunk
    fillPx(6, 27, trunkCol);
    fillPx(6, 28, trunkCol);
    
    // Draw leaves leaning
    fillPx(6 + sway, 23, leafCol);
    fillPx(5 + sway, 24, leafCol);
    fillPx(6 + sway, 24, leafCol);
    fillPx(7 + sway, 24, leafCol);
    fillPx(4 + sway, 25, leafCol);
    fillPx(5 + sway, 25, leafCol);
    fillPx(6 + sway, 25, leafCol);
    fillPx(7 + sway, 25, leafCol);
    fillPx(8 + sway, 25, leafCol);
    fillPx(5 + sway, 26, leafCol);
    fillPx(6 + sway, 26, leafCol);
    fillPx(7 + sway, 26, leafCol);
    
    // Animate wind lines and leaves
    updateWindAnimation(skyCol);
  }
}

