#include "qrcodegen.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// ===================== KONFIGURASI HARDWARE =====================
#define BTN_PIN         22
#define BTN_ACTIVE_LOW  1

// ===================== SETTING APP =====================
#define QR_VERSION           6
#define ACTIVE_SECONDS       300
#define AREA_NAME            "Area Smoking"
#define LONG_PRESS_MS        5000
#define LONG_PRESS_VISUAL_MS 500

static const char *DEVICE_KEY = "Masukan random key device kalian";

// ===================== NETWORK WIFI =====================
const char* ssid     = "Ssid Wifi";
const char* password = "Password wifi";
const char* apiUrl   = "Masukan api yang diambil dari website";

// ===================== STATE & BUFFERS =====================
TFT_eSPI tft = TFT_eSPI();
enum AppState { IDLE, ACTIVE };
AppState appState = IDLE;

uint32_t activeStartMs = 0;
int      qrDrawY0 = 0, qrDrawH = 0;

unsigned long btnPressStart      = 0;
bool          btnWasPressed      = false;
bool          longPressTriggered = false;

String activeToken = "";

static uint8_t qrbuf[1400];
static uint8_t tempbuf[1400];

// ===================== BUTTON HELPERS =====================
bool isButtonPressed() {
#if BTN_ACTIVE_LOW
  return digitalRead(BTN_PIN) == LOW;
#else
  return digitalRead(BTN_PIN) == HIGH;
#endif
}

// ===================== UI: IDLE =====================
void showIdle() {
  tft.fillScreen(TFT_BLACK);

  int centerX = tft.width() / 2;

  // 1. Teks paling atas: Security Patrol
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  String topText = "Security Patrol";
  int topTextX = centerX - ((topText.length() * 6 * 1) / 2); 
  tft.setCursor(topTextX, 15); // 10 px dari atas
  tft.print(topText);

  // 2. Teks PGNCOM lebih besar
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(4); // diperbesar
  String pgncomText = "PGNCOM";
  int pgncomX = centerX - ((pgncomText.length() * 6 * 4) / 2); 
  int pgncomY = 130;
  tft.setCursor(pgncomX, pgncomY);
  tft.print(pgncomText);

  // 3. Teks RO JABATIM di bawah PGNCOM
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  String roText = "RO JABATIM";
  int roX = centerX - ((roText.length() * 6 * 3) / 2);
  tft.setCursor(roX, pgncomY + 45);
  tft.print(roText);

  // 4. Teks ONE MANAGE kapital
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(3);
  String oneManageText = "ONE MANAGE";
  int oneX = centerX - ((oneManageText.length() * 6 * 3) / 2);
  tft.setCursor(oneX, pgncomY + 240);
  tft.print(oneManageText);

  // 5. Teks petunjuk dengan BARCODE
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(1);
  String instructionText = "Tekan Tombol 1 Kali Untuk Menampilkan Barcode";
  int instrX = centerX - ((instructionText.length() * 6 * 1) / 2);
  tft.setCursor(instrX, pgncomY + 285);
  tft.print(instructionText);

  // 6. Teks reset di bawah layar
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(1);
  String resetText = "Tahan Tombol 5 Detik Untuk Melakukan Reset";
  int resetX = centerX - ((resetText.length() * 6 * 1) / 2);
  tft.setCursor(resetX, tft.height() - 50); 
  tft.print(resetText);

  qrDrawH = 0;
}

// ===================== UI: BANNER =====================
void showBanner(const char *msg, uint16_t bg = TFT_RED, uint16_t fg = TFT_WHITE) {
  tft.fillRect(0, 0, 320, 26, bg);
  tft.setTextColor(fg, bg);
  tft.setTextSize(1);
  tft.setCursor(4, 8);
  tft.print(msg);
}

// ===================== UI: RESET PROGRESS =====================
void showResetProgress(unsigned long heldMs) {
  tft.fillRect(0, 0, 320, 26, TFT_ORANGE);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(4, 8);
  tft.print("Tahan untuk Reset...  Lepas = Batal");

  int barW = (int)((heldMs * 310UL) / LONG_PRESS_MS);
  if (barW > 310) barW = 310;

  tft.fillRect(5, 28, barW, 8, TFT_ORANGE);
  tft.fillRect(5 + barW, 28, 310 - barW, 8, TFT_DARKGREY);
}

// ===================== UI: COUNTDOWN BAR =====================
void updateCountdown(uint32_t s) {
  if (qrDrawH <= 0) return;

  int centerX = 160;

  // Text area turun lebih bawah
  int areaTextY   = qrDrawY0 + qrDrawH + 40;
  
  // Bar countdown di bagian bawah
  int barH = 18;
  int barY = tft.height() - barH - 40; // 40 px dari bawah

  int barX        = 30;
  int barW        = 260;
  int labelTextY  = barY - 20; // teks di atas bar

  // Bersihkan area bawah QR
  tft.fillRect(0, areaTextY, 320, tft.height() - areaTextY, TFT_BLACK);

  // 1. Teks area di tengah
  tft.setTextSize(3); // teks lebih besar
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String areaText = String(AREA_NAME);
  int areaTextX = centerX - ((areaText.length() * 6 * 3) / 2); // hitung ulang untuk size 3
  tft.setCursor(areaTextX, areaTextY);
  tft.print(areaText);

  // 2. Outline bar
  tft.drawRect(barX, barY, barW, barH, TFT_WHITE);

  // Hitung progress
  int fillW = (s * (barW - 2)) / ACTIVE_SECONDS;
  if (fillW < 0) fillW = 0;
  if (fillW > (barW - 2)) fillW = barW - 2;

  uint16_t barColor = s > 10 ? TFT_GREEN : (s > 5 ? TFT_YELLOW : TFT_RED);

  // Isi bar
  tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, barColor);
  tft.fillRect(barX + 1 + fillW, barY + 1, (barW - 2) - fillW, barH - 2, TFT_DARKGREY);

  // 3. Teks label di atas bar
  tft.setTextSize(1); // teks lebih kecil
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String labelText = "Waktu Pengisian Tersisa";
  int labelTextX = centerX - ((labelText.length() * 6 * 1) / 2); // hitung ulang untuk ukuran 1
  tft.setCursor(labelTextX, labelTextY);
  tft.print(labelText);
}

// ===================== UI: DRAW QR =====================
void drawQR(const String &text, uint32_t sisa) {
  bool ok = qrcodegen_encodeText(
    text.c_str(), tempbuf, qrbuf,
    qrcodegen_Ecc_LOW, QR_VERSION, QR_VERSION, qrcodegen_Mask_AUTO, true
  );
  if (!ok) {
    tft.fillScreen(TFT_BLACK);
    showBanner("QR encode gagal", TFT_RED, TFT_WHITE);
    return;
  }

  int size  = qrcodegen_getSize(qrbuf);
  int scale = 6;
  int x0    = (tft.width() - (size * scale)) / 2;
  int y0    = 26;
  qrDrawY0  = y0;
  qrDrawH   = size * scale;

  tft.fillScreen(TFT_BLACK);
  tft.fillRect(x0-4, y0-4, (size*scale)+8, (size*scale)+8, TFT_WHITE);

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      if (qrcodegen_getModule(qrbuf, x, y)) {
        tft.fillRect(x0 + (x*scale), y0 + (y*scale), scale, scale, TFT_BLACK);
      }
    }
  }

  updateCountdown(sisa);
}

// ===================== SERVER COMMS =====================
String fetchTokenFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi terputus! Alat tidak tersambung internet.");
    return "";
  }

  unsigned long t0 = millis();
  Serial.print("Menghubungkan ke API... ");

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  http.begin(secureClient, apiUrl);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_KEY);

  String payload = "{\"area\":\"" + String(AREA_NAME) + "\"}";
  int httpResponseCode = http.POST(payload);

  Serial.print("Selesai diproses dalam ");
  Serial.print(millis() - t0);
  Serial.println(" ms");

  String token = "";

  if (httpResponseCode > 0) {
    String response = http.getString();

    Serial.println("--- RAW RESPONSE ---");
    Serial.println(response);
    Serial.println("--------------------");

    int idx = response.indexOf("\"token\":\"");
    if (idx >= 0) {
      int s = idx + 9;
      int e = response.indexOf("\"", s);
      if (e > s) token = response.substring(s, e);
    }
  } else {
    Serial.print("Koneksi ke server gagal. Error code (dari Library): ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return token;
}

// ===================== START ACTIVE =====================
void startActive() {
  tft.fillScreen(TFT_BLACK);
  showBanner("Loading Token...", TFT_BLUE, TFT_WHITE);

  String token = fetchTokenFromServer();

  if (token.length() > 0 && token.startsWith("TOKEN:")) {
    activeToken   = token;
    activeStartMs = millis();
    appState      = ACTIVE;
    drawQR(token, ACTIVE_SECONDS);
    Serial.println("Token diterima: " + token);
  } else {
    showBanner("Gagal Request Token", TFT_RED, TFT_WHITE);
    delay(3000);
    showIdle();
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);

#if BTN_ACTIVE_LOW
  pinMode(BTN_PIN, INPUT_PULLUP);
#else
  pinMode(BTN_PIN, INPUT_PULLDOWN);
#endif

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  tft.println("Starting system...");

  tft.print("Memompa Sinyal WiFi...");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    tft.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi Terhubung!");
    Serial.print("IP Address ESP32: ");
    Serial.println(WiFi.localIP());

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 50);
    tft.print("WiFi Connected!");
    tft.setCursor(10, 80);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
    delay(1500);
  } else {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setCursor(10, 100);
    tft.println("WIFI ERROR / TIDAK KETEMU!");
    Serial.println("\nGagal menyambung ke WiFi. Cek password/nama hotspot.");
    while (true) {
      delay(1);
    }
  }

  showIdle();
  Serial.println("Ready. Short press = QR | Tahan 5s = Reset");
}

// ===================== LOOP =====================
void loop() {
  bool btnNow = isButtonPressed();

  if (btnNow && !btnWasPressed) {
    btnPressStart      = millis();
    longPressTriggered = false;
    btnWasPressed      = true;
  }
  else if (btnNow && btnWasPressed) {
    unsigned long held = millis() - btnPressStart;

    if (held >= LONG_PRESS_MS && !longPressTriggered) {
      longPressTriggered = true;
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setTextSize(2);
      tft.setCursor(30, 100);
      tft.print("Restarting...");
      Serial.println("Long press -> ESP.restart()");
      delay(800);
      ESP.restart();
    }
    else if (held >= LONG_PRESS_VISUAL_MS && !longPressTriggered) {
      showResetProgress(held);
    }
  }
  else if (!btnNow && btnWasPressed) {
    btnWasPressed = false;

    if (!longPressTriggered) {
      if (appState == IDLE) {
        startActive();
      } else if (appState == ACTIVE) {
        uint32_t elapsed = (millis() - activeStartMs) / 1000;
        if (elapsed < ACTIVE_SECONDS && activeToken.length() > 0) {
          drawQR(activeToken, ACTIVE_SECONDS - elapsed);
        }
      }
    }
  }

  if (appState == ACTIVE && !btnWasPressed) {
    static unsigned long lastTick = 0;
    if (millis() - lastTick >= 1000) {
      lastTick = millis();
      uint32_t elapsed = (millis() - activeStartMs) / 1000;

      if (elapsed >= ACTIVE_SECONDS) {
        appState    = IDLE;
        activeToken = "";
        showIdle();
      } else {
        updateCountdown(ACTIVE_SECONDS - elapsed);
      }
    }
  }
}
