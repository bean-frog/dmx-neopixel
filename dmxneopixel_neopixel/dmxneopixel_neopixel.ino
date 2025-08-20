#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>

// --- Pin & LED Settings ---
#define LED_PIN     5
#define LED_COUNT   300

// --- Serial Communication ---
#define RX_PIN      10
#define TX_PIN      11
#define START_BYTE  0xA1

// --- Objects ---
SoftwareSerial inputSerial(RX_PIN, TX_PIN);  // RX from Uno
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- Frame Buffer ---
uint8_t data[9];
uint8_t buffer[10];
uint8_t bufferIndex = 0;

// --- State ---
uint8_t prevColorMode = 255;
uint8_t prevColor1 = 255;
uint8_t prevColor2 = 255;
bool baseColorsInitialized = false;
uint32_t baseColors[LED_COUNT];

unsigned long lastUpdate = 0;
uint16_t animStep = 0;

void setup() {
  inputSerial.begin(57600);
  strip.begin();
  strip.show();
}

void loop() {
  // === Receive Data Packet ===
  while (inputSerial.available()) {
    uint8_t b = inputSerial.read();
    if (bufferIndex == 0 && b != START_BYTE) continue;
    buffer[bufferIndex++] = b;
    if (bufferIndex == 10) {
      memcpy(data, &buffer[1], 9);
      bufferIndex = 0;
    }
  }

  // === Extract Channels ===
  uint8_t brightness = data[0];
  uint8_t colorMode  = data[1];
  uint8_t color1     = data[2];
  uint8_t color2     = data[3];
  uint8_t effect     = data[4];
  uint8_t speed      = data[5];
  uint8_t direction  = data[6];
  uint8_t segment    = max(data[7], 1);
  uint8_t sparkleAmt = data[8];

  strip.setBrightness(brightness);

  // === Base Color Layout Update ===
  if (!baseColorsInitialized || colorMode != prevColorMode || color1 != prevColor1 || color2 != prevColor2) {
    generateBaseColors(colorMode, color1, color2);
    prevColorMode = colorMode;
    prevColor1 = color1;
    prevColor2 = color2;
  }

  // === Animation Step Timing ===
  unsigned long interval = map(speed, 0, 255, 100, 5);
  if (millis() - lastUpdate >= interval) {
    animStep++;
    lastUpdate = millis();

    switch (effect) {
      case 0 ... 42:
        showBaseColors();
        break;
      case 43 ... 84:
        effectChase(animStep, segment, direction);
        break;
      case 85 ... 126:
        effectPulse(animStep);
        break;
      case 127 ... 168:
        effectStrobe(animStep);
        break;
      case 169 ... 210:
        effectSparkle(sparkleAmt);
        break;
      case 211 ... 255:
        effectWave(animStep, direction);
        break;
    }

    strip.show();
  }
}

// === Base Color Generation ===
void generateBaseColors(uint8_t mode, uint8_t c1, uint8_t c2) {
  for (int i = 0; i < LED_COUNT; i++) {
    switch (mode) {
      case 0 ... 63:
        baseColors[i] = colorFromWheel(c1);
        break;
      case 64 ... 127: {
        uint32_t colA = colorFromWheel(c1);
        uint32_t colB = colorFromWheel(c2);
        float t = float(i) / (LED_COUNT - 1);
        uint8_t r = (1 - t) * ((colA >> 16) & 0xFF) + t * ((colB >> 16) & 0xFF);
        uint8_t g = (1 - t) * ((colA >> 8) & 0xFF) + t * ((colB >> 8) & 0xFF);
        uint8_t b = (1 - t) * (colA & 0xFF) + t * (colB & 0xFF);
        baseColors[i] = strip.Color(r, g, b);
        break;
      }
      case 128 ... 191:
        baseColors[i] = colorFromWheel(map(i, 0, LED_COUNT - 1, 0, 155));
        break;
      case 192 ... 255:
        baseColors[i] = colorFromWheel(random(156));
        break;
    }
  }

  baseColorsInitialized = true;
}

void showBaseColors() {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, baseColors[i]);
  }
}

// === Effects ===

void effectChase(uint16_t step, uint8_t segment, uint8_t dir) {
  for (int i = 0; i < LED_COUNT; i++) {
    uint16_t index = dir < 129 ? i : (LED_COUNT - 1 - i);
    if (((index + step) / segment) % 2 == 0) {
      strip.setPixelColor(i, baseColors[i]);
    } else {
      strip.setPixelColor(i, 0);
    }
  }
}

void effectPulse(uint16_t step) {
  float pulse = (sin(step * 0.1) + 1) / 2;
  for (int i = 0; i < LED_COUNT; i++) {
    uint32_t color = baseColors[i];
    uint8_t r = ((color >> 16) & 0xFF) * pulse;
    uint8_t g = ((color >> 8) & 0xFF) * pulse;
    uint8_t b = (color & 0xFF) * pulse;
    strip.setPixelColor(i, r, g, b);
  }
}

void effectStrobe(uint16_t step) {
  bool on = (step % 10) < 5;
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, on ? baseColors[i] : 0);
  }
}

void effectSparkle(uint8_t density) {
  for (int i = 0; i < LED_COUNT; i++) {
    if (random(255) < density) {
      strip.setPixelColor(i, baseColors[i]);
    } else {
      strip.setPixelColor(i, 0);
    }
  }
}

void effectWave(uint16_t step, uint8_t dir) {
  for (int i = 0; i < LED_COUNT; i++) {
    float phase = (2 * PI * (dir < 129 ? i : (LED_COUNT - 1 - i))) / LED_COUNT;
    float wave = (sin(phase + step * 0.1) + 1) / 2;
    uint32_t color = baseColors[i];
    uint8_t r = ((color >> 16) & 0xFF) * wave;
    uint8_t g = ((color >> 8) & 0xFF) * wave;
    uint8_t b = (color & 0xFF) * wave;
    strip.setPixelColor(i, r, g, b);
  }
}

// === Color Wheel ===

uint32_t colorFromWheel(uint8_t pos) {
  pos %= 156;
  if (pos < 52) return strip.Color(255 - pos * 5, pos * 5, 0);         // Red to Green
  if (pos < 104) {
    pos -= 52;
    return strip.Color(0, 255 - pos * 5, pos * 5);                     // Green to Blue
  }
  pos -= 104;
  return strip.Color(pos * 5, 0, 255 - pos * 5);                       // Blue to Red
}
