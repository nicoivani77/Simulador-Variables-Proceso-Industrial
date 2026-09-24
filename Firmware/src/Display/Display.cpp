/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de la interfaz OLED: pantalla principal, menú, mensajes, estados e indicadores gráficos.
 */

#include "Display.h"
#include <cstring>
#include <stdio.h>

static const uint8_t WIFI_ICON_16X10[] PROGMEM = {
  0x00, 0x00,
  0x0F, 0xF0,
  0x3F, 0xFC,
  0x70, 0x0E,
  0x07, 0xE0,
  0x0F, 0xF0,
  0x10, 0x08,
  0x03, 0xC0,
  0x01, 0x80,
  0x00, 0x00
};

static const uint8_t SPEAKER_ON_16X10[] PROGMEM = {
  0x00, 0x00,
  0x0F, 0x10,
  0x1F, 0x24,
  0xFF, 0x12,
  0xFF, 0x12,
  0xFF, 0x12,
  0xFF, 0x12,
  0x1F, 0x24,
  0x0F, 0x10,
  0x00, 0x00
};

static const uint8_t SPEAKER_OFF_16X10[] PROGMEM = {
  0x00, 0x00,
  0x0F, 0x00,
  0x1F, 0x42,
  0xFF, 0x24,
  0xFF, 0x18,
  0xFF, 0x18,
  0xFF, 0x24,
  0x1F, 0x42,
  0x0F, 0x00,
  0x00, 0x00
};


// Logotipo personalizado "SIMULADOR" para la pantalla de presentación.
// Cada glifo usa una matriz de 9 x 15 píxeles. El trazo es deliberadamente
// grueso y con esquinas recortadas para que la palabra se vea más redondeada
// y con más masa que la fuente estándar de Adafruit_GFX.
//
// Los nueve glifos corresponden, en orden, a:
// S I M U L A D O R
static const uint16_t ROUNDED_SIMULADOR[9][15] PROGMEM = {
  // S
  {
    0b001111100,
    0b011111110,
    0b111000111,
    0b110000011,
    0b110000000,
    0b111000000,
    0b011110000,
    0b001111100,
    0b000111110,
    0b000001111,
    0b000000011,
    0b110000011,
    0b111000111,
    0b011111110,
    0b001111100
  },

  // I
  {
    0b111111111,
    0b111111111,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b000111000,
    0b111111111,
    0b111111111
  },

  // M
  {
    0b110000011,
    0b111000111,
    0b111101111,
    0b110111011,
    0b110010011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011
  },

  // U
  {
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b111000111,
    0b011111110,
    0b001111100
  },

  // L
  {
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000000,
    0b110000011,
    0b111111111,
    0b111111111
  },

  // A
  {
    0b001111100,
    0b011111110,
    0b111000111,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b111111111,
    0b111111111,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011
  },

  // D
  {
    0b111111000,
    0b111111100,
    0b110001110,
    0b110000111,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000111,
    0b110001110,
    0b111111100,
    0b111111000
  },

  // O
  {
    0b001111100,
    0b011111110,
    0b111000111,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b110000011,
    0b111000111,
    0b011111110,
    0b001111100
  },

  // R
  {
    0b111111000,
    0b111111100,
    0b110001110,
    0b110000111,
    0b110000111,
    0b110001110,
    0b111111100,
    0b111111000,
    0b110110000,
    0b110011000,
    0b110001100,
    0b110000110,
    0b110000011,
    0b110000011,
    0b110000011
  }
};

Display::Display()
: _oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
  _initialized(false),
  _dirty(true),
  _screenMode(SCREEN_MAIN),
  _sdaPin(8),
  _sclPin(9),
  _i2cAddress(0x3C),
  _wifiStatus(DISPLAY_WIFI_OFF),
  _buzzerEnabled(true),
  _captureActive(false),
  _captureDotVisible(false),
  _channelEnabled(true),
  _alarmHigh(false),
  _alarmLow(false),
  _readingPercent(0.0f),
  _outputPercent(0.0f),
  _manualModeIndicator(false),
  _faultActive(false),
  _menuCursorLine(0),
  _editValue(0.0f),
  _splashShutdown(false),
  _splashStartMs(0),
  _splashDurationMs(2600),
  _lastSplashFrameMs(0),
  _specialStartMs(0),
  _specialDurationMs(7000),
  _lastSpecialFrameMs(0),
  _lastRenderMs(0),
  _lastTrendSampleMs(0),
  _lastCaptureBlinkMs(0) {

  copyText(_channelName, sizeof(_channelName), "CH1");
  copyText(_channelMode, sizeof(_channelMode), "AUTO");
  copyText(_readingLabel, sizeof(_readingLabel), "IN");
  copyText(_outputLabel, sizeof(_outputLabel), "OUT");
  copyText(_faultText, sizeof(_faultText), "OK");
  copyText(_menuTitle, sizeof(_menuTitle), "MENU");
  for (uint8_t i = 0; i < 4; i++) copyText(_menuLines[i], sizeof(_menuLines[i]), "");
  copyText(_editTitle, sizeof(_editTitle), "EDITAR");
  copyText(_editUnit, sizeof(_editUnit), "");
  copyText(_messageTitle, sizeof(_messageTitle), "INFO");
  copyText(_messageText, sizeof(_messageText), "");
  copyText(_splashVersion, sizeof(_splashVersion), "v1.5");
  clearTrend(0, 0);
}

bool Display::begin(int sdaPin, int sclPin, uint8_t i2cAddress) {
  _sdaPin = sdaPin;
  _sclPin = sclPin;
  _i2cAddress = i2cAddress;

  Wire.begin(_sdaPin, _sclPin);
  Wire.setClock(400000);

  _initialized = _oled.begin(_i2cAddress, true);
  if (!_initialized) return false;

  _oled.clearDisplay();
  _oled.setTextColor(SH110X_WHITE);
  _oled.setTextSize(1);
  _oled.setRotation(0);

  _lastRenderMs = millis();
  _lastTrendSampleMs = millis();
  _lastCaptureBlinkMs = millis();

  forceRefresh();
  return true;
}

void Display::update() {
  if (!_initialized) return;

  const uint32_t now = millis();

  // El splash se anima a ~14 FPS. No se usa delay dentro de Display:
  // el programa principal puede seguir inicializando/atendiendo periféricos.
  if (_screenMode == SCREEN_SPLASH &&
      (uint32_t)(now - _lastSplashFrameMs) >= 70U) {
    _lastSplashFrameMs = now;
    _dirty = true;
  }

  if (_screenMode == SCREEN_SPECIAL &&
      isSpecialScreenActive() &&
      (uint32_t)(now - _lastSpecialFrameMs) >= SPECIAL_FRAME_INTERVAL_MS) {
    _lastSpecialFrameMs = now;
    _dirty = true;
  }

  if ((uint32_t)(now - _lastTrendSampleMs) >= TREND_INTERVAL_MS) {
    _lastTrendSampleMs = now;
    sampleTrend();

    // El buffer de tendencia debe actualizarse siempre,
    // pero solo hace falta redibujar si estamos viendo la pantalla principal.
    if (_screenMode == SCREEN_MAIN) {
      _dirty = true;
    }
  }

  // Cuando la captura está activa, solo parpadea el círculo central del icono.
  // Las esquinas quedan fijas para que el usuario no pierda la referencia visual.
  if (_captureActive && (uint32_t)(now - _lastCaptureBlinkMs) >= CAPTURE_BLINK_INTERVAL_MS) {
    _lastCaptureBlinkMs = now;
    _captureDotVisible = !_captureDotVisible;
    _dirty = true;
  }

  if (_dirty && (uint32_t)(now - _lastRenderMs) >= RENDER_INTERVAL_MS) {
    renderNow();
  }
}

void Display::forceRefresh() {
  if (!_initialized) return;
  _dirty = true;
  renderNow();
}

void Display::setStartupSplash(const char* version, uint32_t durationMs) {
  _splashShutdown = false;
  copyText(_splashVersion, sizeof(_splashVersion), version ? version : "");
  _splashDurationMs = (durationMs < 2000U) ? 2000U : durationMs;
  _splashStartMs = millis();
  _lastSplashFrameMs = 0;
  setScreenMode(SCREEN_SPLASH);
  _dirty = true;
}

void Display::setShutdownSplash(const char* version, uint32_t durationMs) {
  _splashShutdown = true;
  copyText(_splashVersion, sizeof(_splashVersion), version ? version : "");
  _splashDurationMs = (durationMs < 2000U) ? 2000U : durationMs;
  _splashStartMs = millis();
  _lastSplashFrameMs = 0;
  setScreenMode(SCREEN_SPLASH);
  _dirty = true;
}

void Display::setSpecialScreen(uint32_t durationMs) {
  _specialDurationMs = (durationMs < 1500U) ? 1500U : durationMs;
  _specialStartMs = millis();
  _lastSpecialFrameMs = 0;
  setScreenMode(SCREEN_SPECIAL);
  _dirty = true;
}

bool Display::isSpecialScreenActive() const {
  if (_screenMode != SCREEN_SPECIAL) return false;
  return (uint32_t)(millis() - _specialStartMs) < _specialDurationMs;
}

void Display::sleep() {
  if (!_initialized) return;

  // Limpia primero la RAM visible para que al próximo encendido no quede
  // retenida la última imagen de apagado.
  _oled.clearDisplay();
  _oled.display();

  // Orden nativa del SH1106 para apagar el panel.
  _oled.oled_command(SH110X_DISPLAYOFF);

  // begin() volverá a inicializar el panel y enviará DISPLAYON.
  _initialized = false;
  _dirty = true;
}

void Display::setScreenMode(ScreenMode mode) {
  if (_screenMode != mode) {
    _screenMode = mode;
    _dirty = true;
  }
}

void Display::setWiFiStatus(WiFiStatus status) {
  if (_wifiStatus != status) {
    _wifiStatus = status;
    _dirty = true;
  }
}

void Display::setWiFiEnabled(bool enabled) {
  setWiFiStatus(enabled ? DISPLAY_WIFI_ON : DISPLAY_WIFI_OFF);
}

void Display::setWiFiConnected(bool connected) {
  setWiFiStatus(connected ? DISPLAY_WIFI_CONNECTED : DISPLAY_WIFI_OFF);
}

void Display::setBuzzerEnabled(bool enabled) {
  if (_buzzerEnabled != enabled) {
    _buzzerEnabled = enabled;
    _dirty = true;
  }
}

void Display::setCaptureActive(bool active) {
  if (_captureActive != active) {
    _captureActive = active;
    _captureDotVisible = active;
    _lastCaptureBlinkMs = millis();
    _dirty = true;
  }
}

void Display::setChannelName(const char *name) {
  char tmp[sizeof(_channelName)];
  copyText(tmp, sizeof(tmp), name);
  if (strcmp(_channelName, tmp) != 0) {
    copyText(_channelName, sizeof(_channelName), tmp);
    _dirty = true;
  }
}

void Display::setChannelEnabled(bool enabled) {
  if (_channelEnabled != enabled) {
    _channelEnabled = enabled;
    _dirty = true;
  }
}

void Display::setChannelMode(const char *mode) {
  char tmp[sizeof(_channelMode)];
  copyText(tmp, sizeof(tmp), mode);
  if (strcmp(_channelMode, tmp) != 0) {
    copyText(_channelMode, sizeof(_channelMode), tmp);
    _dirty = true;
  }
}

void Display::setReadingPercent(float percent) {
  const float clipped = clampPercentFloat(percent);
  if (_readingPercent != clipped) {
    _readingPercent = clipped;

    // Solo se redibuja inmediatamente si el dato está visible.
    // Si estamos en menú, el valor se guarda igual para que la tendencia siga viva.
    if (_screenMode == SCREEN_MAIN) {
      _dirty = true;
    }
  }
}

void Display::setOutputPercent(float percent) {
  const float clipped = clampPercentFloat(percent);
  if (_outputPercent != clipped) {
    _outputPercent = clipped;

    // Solo se redibuja inmediatamente si el dato está visible.
    if (_screenMode == SCREEN_MAIN) {
      _dirty = true;
    }
  }
}

void Display::setManualModeIndicator(bool enabled) {
  if (_manualModeIndicator != enabled) {
    _manualModeIndicator = enabled;
    _dirty = true;
  }
}

void Display::setAlarms(bool high, bool low) {
  if (_alarmHigh != high || _alarmLow != low) {
    _alarmHigh = high;
    _alarmLow = low;
    _dirty = true;
  }
}

void Display::setReadingLabel(const char *label) {
  char tmp[sizeof(_readingLabel)];
  copyText(tmp, sizeof(tmp), label);
  if (strcmp(_readingLabel, tmp) != 0) {
    copyText(_readingLabel, sizeof(_readingLabel), tmp);
    _dirty = true;
  }
}

void Display::setOutputLabel(const char *label) {
  char tmp[sizeof(_outputLabel)];
  copyText(tmp, sizeof(tmp), label);
  if (strcmp(_outputLabel, tmp) != 0) {
    copyText(_outputLabel, sizeof(_outputLabel), tmp);
    _dirty = true;
  }
}

void Display::setFaultStatus(const char* text, bool active) {
  char tmp[sizeof(_faultText)];
  copyText(tmp, sizeof(tmp), text);

  if (_faultActive != active || strcmp(_faultText, tmp) != 0) {
    _faultActive = active;
    copyText(_faultText, sizeof(_faultText), tmp);
    _dirty = true;
  }
}

void Display::setMenuScreen(const char* title,
                            const char* line0,
                            const char* line1,
                            const char* line2,
                            const char* line3,
                            uint8_t cursorLine) {
  setScreenMode(SCREEN_MENU);

  char newTitle[sizeof(_menuTitle)];
  char newLines[4][sizeof(_menuLines[0])];
  copyText(newTitle, sizeof(newTitle), title);
  copyText(newLines[0], sizeof(newLines[0]), line0);
  copyText(newLines[1], sizeof(newLines[1]), line1);
  copyText(newLines[2], sizeof(newLines[2]), line2);
  copyText(newLines[3], sizeof(newLines[3]), line3);

  const uint8_t newCursor = (cursorLine > 3) ? 3 : cursorLine;
  bool changed = (_menuCursorLine != newCursor) || (strcmp(_menuTitle, newTitle) != 0);
  for (uint8_t i = 0; i < 4; i++) {
    if (strcmp(_menuLines[i], newLines[i]) != 0) changed = true;
  }

  if (changed) {
    copyText(_menuTitle, sizeof(_menuTitle), newTitle);
    for (uint8_t i = 0; i < 4; i++) copyText(_menuLines[i], sizeof(_menuLines[i]), newLines[i]);
    _menuCursorLine = newCursor;
    _dirty = true;
  }
}

void Display::setEditScreen(const char* title, float value, const char* unit) {
  setScreenMode(SCREEN_EDIT);
  char newTitle[sizeof(_editTitle)];
  char newUnit[sizeof(_editUnit)];
  copyText(newTitle, sizeof(newTitle), title);
  copyText(newUnit, sizeof(newUnit), unit);
  if (strcmp(_editTitle, newTitle) != 0 || strcmp(_editUnit, newUnit) != 0 || _editValue != value) {
    copyText(_editTitle, sizeof(_editTitle), newTitle);
    copyText(_editUnit, sizeof(_editUnit), newUnit);
    _editValue = value;
    _dirty = true;
  }
}

void Display::setMessageScreen(const char* title, const char* message) {
  setScreenMode(SCREEN_MESSAGE);
  char newTitle[sizeof(_messageTitle)];
  char newText[sizeof(_messageText)];
  copyText(newTitle, sizeof(newTitle), title);
  copyText(newText, sizeof(newText), message);
  if (strcmp(_messageTitle, newTitle) != 0 || strcmp(_messageText, newText) != 0) {
    copyText(_messageTitle, sizeof(_messageTitle), newTitle);
    copyText(_messageText, sizeof(_messageText), newText);
    _dirty = true;
  }
}

void Display::renderNow() {
  _oled.clearDisplay();
  _oled.setTextWrap(false);
  _oled.setTextColor(SH110X_WHITE);
  _oled.setTextSize(1);

  // Las pantallas de presentación y la pantalla especial utilizan el área completa.
  if (_screenMode != SCREEN_SPLASH && _screenMode != SCREEN_SPECIAL) {
    drawTopBar();
  }

  switch (_screenMode) {
    case SCREEN_MENU:    drawMenuLayout(); break;
    case SCREEN_EDIT:    drawEditLayout(); break;
    case SCREEN_MESSAGE: drawMessageLayout(); break;
    case SCREEN_SPLASH:  drawSplashLayout(); break;
    case SCREEN_SPECIAL: drawSpecialLayout(); break;
    case SCREEN_MAIN:
    default:             drawMainLayout(); break;
  }

  _oled.display();
  _dirty = false;
  _lastRenderMs = millis();
}

void Display::sampleTrend() {
  memmove(_readingTrend, _readingTrend + 1, TREND_SAMPLES - 1);
  memmove(_outputTrend,  _outputTrend  + 1, TREND_SAMPLES - 1);

  _readingTrend[TREND_SAMPLES - 1] = clampPercent(_readingPercent);
  _outputTrend[TREND_SAMPLES - 1]  = clampPercent(_outputPercent);
}

void Display::clearTrend(uint8_t readingValue, uint8_t outputValue) {
  for (uint8_t i = 0; i < TREND_SAMPLES; i++) {
    _readingTrend[i] = readingValue;
    _outputTrend[i]  = outputValue;
  }
}

void Display::drawTopBar() {
  drawWifiIcon(2, 1, _wifiStatus);
  drawSpeakerIcon(21, 1, _buzzerEnabled);

  // Zona derecha de la barra superior:
  // - Captura/SD a la izquierda.
  // - Estado PLAY/STOP de salida a su derecha.
  // Los indicadores no invaden la fila de IN/OUT.
  const int16_t captureX = 98;
  const int16_t runX = 116;

  if (_faultActive) {
    // Los errores se dibujan como letras independientes, una al lado de la otra,
    // y se limitan antes de la zona de iconos de captura/PLAY para evitar solapes.
    drawFaultTags(42, 1, captureX - 3);
  }

  // Indicador de captura de datos en la barra superior.
  if (_captureActive) {
    drawCaptureIcon(captureX, 1, _captureDotVisible);
  }

  // PLAY/STOP se muestra únicamente en la pantalla principal.
  if (_screenMode == SCREEN_MAIN) {
    drawRunStatusIcon(runX, 1, _channelEnabled);
  }

  _oled.drawFastHLine(0, 11, SCREEN_WIDTH, SH110X_WHITE);
}

void Display::drawMainLayout() {
  drawInfoRow();
  drawTrendRow(26, _readingLabel, _readingTrend);
  drawTrendRow(46, _outputLabel, _outputTrend);
  drawManualModeIndicator();
}

void Display::drawInfoRow() {
  _oled.setTextSize(1);

  char readBuf[16];
  char outBuf[16];

  snprintf(readBuf, sizeof(readBuf), "%s:%u", _readingLabel, clampPercent(_readingPercent));
  snprintf(outBuf,  sizeof(outBuf),  "%s:%u", _outputLabel,  clampPercent(_outputPercent));

  const int16_t charW = 6;
  const int16_t y = 14;

  const int16_t outWidth  = (int16_t)strlen(outBuf) * charW;
  const int16_t readWidth = (int16_t)strlen(readBuf) * charW;

  int16_t outX  = SCREEN_WIDTH - 3 - outWidth;
  int16_t readX = outX - 6 - readWidth;

  if (readX < 3) readX = 3;

  _oled.setCursor(readX, y);
  _oled.print(readBuf);

  _oled.setCursor(outX, y);
  _oled.print(outBuf);
}

void Display::drawManualModeIndicator() {
  // El indicador se activa por flag explícito o por texto de modo para mantener consistencia visual.
  if (!_manualModeIndicator && strcmp(_channelMode, "MAN") != 0 && strcmp(_channelMode, "MANUAL") != 0) {
    return;
  }

  // Indicador MANUAL debajo de OUT.
  // La letra usa trazo de 1 px, como las letras comunes del display.
  // Alrededor se deja un recuadro con 2 px de margen desde la M.
  static const uint8_t M_4X5[4] = {
    0b10001,  // #...#
    0b11011,  // ##.##
    0b10101,  // #.#.#
    0b10001,  // #...#
  };

  const int16_t mW = 5;
  const int16_t mH = 4;
  const int16_t margin = 1;
  const int16_t boxW = mW + (2 * margin);  // 7 px
  const int16_t boxH = mH + (2 * margin);  // 6 px

  // Centrado debajo de la etiqueta OUT del gráfico inferior.
  const int16_t boxX = 8;
  const int16_t boxY = 57;
  const int16_t mX = boxX + margin;
  const int16_t mY = boxY + margin;

  _oled.fillRect(boxX, boxY, boxW, boxH, SH110X_WHITE);

  for (uint8_t row = 0; row < mH; row++) {
    for (uint8_t col = 0; col < mW; col++) {
      if (M_4X5[row] & (1 << (mW - 1 - col))) {
        _oled.drawPixel(mX + col, mY + row, SH110X_BLACK);
      }
    }
  }
}

void Display::drawTrendRow(int16_t y, const char *label, const uint8_t *trend) {
  const int16_t labelX = 3;
  const int16_t labelY = y + 3;
  const int16_t boxX = 24;
  const int16_t boxY = y;
  const int16_t boxW = 101;
  const int16_t boxH = 16;

  _oled.setCursor(labelX, labelY);
  _oled.print(label);

  _oled.drawRect(boxX, boxY, boxW, boxH, SH110X_WHITE);

  const int16_t plotX = boxX + 2;
  const int16_t plotY = boxY + 2;
  const int16_t plotH = boxH - 4;

  for (uint8_t i = 1; i < TREND_SAMPLES; i++) {
    const int16_t x0 = plotX + (i - 1);
    const int16_t x1 = plotX + i;
    const int16_t y0 = mapTrendY(trend[i - 1], plotY, plotH);
    const int16_t y1 = mapTrendY(trend[i],     plotY, plotH);
    _oled.drawLine(x0, y0, x1, y1, SH110X_WHITE);
  }
}

void Display::drawMenuLayout() {
  _oled.setTextSize(1);
  _oled.setCursor(3, 14);
  _oled.print(_menuTitle);
  _oled.drawFastHLine(0, 23, SCREEN_WIDTH, SH110X_WHITE);

  for (uint8_t i = 0; i < 4; i++) {
    const int16_t y = 27 + (i * 9);
    if (i == _menuCursorLine) {
      _oled.fillRect(0, y - 1, SCREEN_WIDTH, 9, SH110X_WHITE);
      _oled.setTextColor(SH110X_BLACK);
      _oled.setCursor(2, y);
      _oled.print(">");
      _oled.setCursor(10, y);
      _oled.print(_menuLines[i]);
      _oled.setTextColor(SH110X_WHITE);
    } else {
      _oled.setCursor(10, y);
      _oled.print(_menuLines[i]);
    }
  }
}

void Display::drawEditLayout() {
  _oled.setTextSize(1);
  _oled.setCursor(3, 14);
  _oled.print(_editTitle);
  _oled.drawFastHLine(0, 23, SCREEN_WIDTH, SH110X_WHITE);

  char valueBuf[18];
  snprintf(valueBuf, sizeof(valueBuf), "%.2f %s", _editValue, _editUnit);

  _oled.setTextSize(1);
  _oled.setCursor(10, 34);
  _oled.print(valueBuf);

  _oled.setTextSize(1);
  _oled.setCursor(4, 56);
  _oled.print("ENTER=OK MENU=BACK");
}

void Display::drawMessageLayout() {
  _oled.setTextSize(1);
  _oled.setCursor(3, 14);
  _oled.print(_messageTitle);
  _oled.drawFastHLine(0, 23, SCREEN_WIDTH, SH110X_WHITE);

  char lines[3][22];
  memset(lines, 0, sizeof(lines));

  const char* src = _messageText;
  for (uint8_t line = 0; line < 3 && src && *src; line++) {
    uint8_t idx = 0;

    while (*src && *src != '\n' && idx < 21) {
      lines[line][idx++] = *src++;
    }
    lines[line][idx] = '\0';

    if (*src == '\n') {
      src++;
    }
  }

  const int16_t y0 = 30;
  for (uint8_t i = 0; i < 3; i++) {
    _oled.setCursor(4, y0 + (i * 10));
    _oled.print(lines[i]);
  }
}


void Display::drawCenteredText(const char* text, int16_t y, uint8_t textSize) {
  if (!text) text = "";

  _oled.setTextSize(textSize);

  // La fuente clásica de Adafruit_GFX ocupa 6 px de ancho por carácter
  // y por factor de escala.
  const int16_t width = (int16_t)strlen(text) * 6 * textSize;
  int16_t x = (SCREEN_WIDTH - width) / 2;
  if (x < 0) x = 0;

  _oled.setCursor(x, y);
  _oled.print(text);
}

void Display::drawRoundedSimulador(uint8_t visiblePercent, int16_t shineX) {
  if (visiblePercent > 100) visiblePercent = 100;

  static const uint8_t GLYPH_COUNT = 9;
  static const int16_t GLYPH_W = 9;
  static const int16_t GLYPH_H = 15;
  static const int16_t GAP = 4;

  const int16_t totalW = (GLYPH_COUNT * GLYPH_W) + ((GLYPH_COUNT - 1) * GAP);
  const int16_t startX = (SCREEN_WIDTH - totalW) / 2;
  const int16_t startY = 5;

  // El frente de revelado no es perfectamente vertical. Se desplaza algunos
  // píxeles según la fila para producir una onda suave atravesando la palabra.
  const int16_t revealBase =
      startX + ((int32_t)totalW * visiblePercent) / 100;

  for (uint8_t glyph = 0; glyph < GLYPH_COUNT; ++glyph) {
    const int16_t glyphX = startX + glyph * (GLYPH_W + GAP);

    for (uint8_t row = 0; row < GLYPH_H; ++row) {
      const uint16_t bits = pgm_read_word(&ROUNDED_SIMULADOR[glyph][row]);

      // Perfil de onda: centro adelantado y extremos ligeramente retrasados.
      // Da la sensación de que el logo "aparece" de forma orgánica.
      const int16_t dy = (int16_t)row - 7;
      const int16_t wave =
          (dy >= -2 && dy <= 2) ? 3 :
          (dy >= -5 && dy <= 5) ? 1 : -1;

      const int16_t revealX = revealBase + wave;

      for (uint8_t col = 0; col < GLYPH_W; ++col) {
        if ((bits & (1U << (GLYPH_W - 1 - col))) == 0) continue;

        const int16_t px = glyphX + col;
        const int16_t py = startY + row;

        // Recorta el logotipo con el frente animado.
        if (px > revealX) continue;

        // Una vez completado el revelado, una pequeña "luz" atraviesa las
        // letras. Al ser un OLED monocromático, el brillo se representa como
        // una fina abertura negra que se desplaza por el interior del logo.
        if (shineX >= 0 && abs(px - shineX) <= 1) {
          continue;
        }

        _oled.drawPixel(px, py, SH110X_WHITE);
      }
    }
  }

  // Algunos píxeles sueltos acompañan el borde de la onda durante el revelado.
  // No forman un símbolo separado: son parte del efecto de aparición del logo.
  if (visiblePercent > 2 && visiblePercent < 98) {
    const int16_t fx = revealBase;
    if (fx >= 2 && fx < SCREEN_WIDTH - 2) {
      _oled.drawPixel(fx + 1, startY + 2, SH110X_WHITE);
      _oled.drawPixel(fx + 2, startY + 7, SH110X_WHITE);
      _oled.drawPixel(fx,     startY + 12, SH110X_WHITE);
    }
  }
}

void Display::drawSpecialLayout() {
  const uint32_t elapsed = (uint32_t)(millis() - _specialStartMs);
  const uint8_t frame = (uint8_t)((elapsed / SPECIAL_FRAME_INTERVAL_MS) & 0xFFU);

  struct StarPoint {
    uint8_t x;
    uint8_t y;
    uint8_t phase;
  };

  static const StarPoint stars[] = {
    {  4,  2, 0}, { 19,  7, 2}, { 36,  1, 4}, { 57,  8, 1},
    { 78,  3, 5}, { 98,  9, 3}, {117,  2, 1}, {123, 15, 4},
    {  2, 22, 3}, {116, 28, 0}, {  8, 39, 5}, {121, 43, 2},
    {  3, 54, 1}, { 22, 59, 4}, { 43, 50, 0}, { 67, 57, 3},
    { 88, 51, 5}, {106, 60, 2}, {122, 55, 0}
  };

  _oled.setTextSize(1);
  for (uint8_t i = 0; i < (sizeof(stars) / sizeof(stars[0])); ++i) {
    const uint8_t state = (uint8_t)((frame + stars[i].phase) % 7U);

    if (state == 6U) {
      continue;
    }

    _oled.setCursor(stars[i].x, stars[i].y);
    _oled.print((state == 0U || state == 1U) ? "*" : ".");
  }

  drawCenteredText("MAY THE FORCE", 21, 1);
  drawCenteredText("BE WITH YOU...", 36, 1);
}


void Display::drawSplashLayout() {
  const uint32_t now = millis();
  const uint32_t elapsed = (uint32_t)(now - _splashStartMs);
  const uint32_t duration = (_splashDurationMs == 0U) ? 1U : _splashDurationMs;

  const uint8_t progress =
      (elapsed >= duration)
      ? 100
      : (uint8_t)((elapsed * 100UL) / duration);

  uint8_t logoVisible = 100;
  bool showRange = true;
  bool showSignature = true;
  int16_t shineX = -100;

  if (!_splashShutdown) {
    // Inicio:
    // 0...58 % del tiempo -> la palabra se revela de izquierda a derecha.
    // Después queda completa y una pequeña luz la atraviesa una vez.
    if (progress < 58U) {
      logoVisible = (uint8_t)((progress * 100UL) / 58UL);
    } else {
      logoVisible = 100;

      if (progress >= 62U && progress <= 88U) {
        const uint8_t local =
            (uint8_t)(((progress - 62U) * 100UL) / (88UL - 62UL));
        shineX = -4 + ((int16_t)local * (SCREEN_WIDTH + 8)) / 100;
      }
    }

    // Los textos secundarios aparecen después del logotipo.
    showRange = (progress >= 34U);
    showSignature = (progress >= 50U);

  } else {
    // Apagado:
    // Primero permanece completo brevemente y luego la misma onda borra
    // el logotipo desde la derecha hacia la izquierda.
    if (progress < 18U) {
      logoVisible = 100;
    } else {
      const uint8_t local =
          (uint8_t)(((progress - 18U) * 100UL) / (100UL - 18UL));
      logoVisible = (local >= 100U) ? 0U : (uint8_t)(100U - local);
    }

    // La firma y el rango desaparecen progresivamente antes del OLED OFF.
    showSignature = (progress < 62U);
    showRange = (progress < 78U);
  }

  drawRoundedSimulador(logoVisible, shineX);

  // Únicos textos de la presentación.
  if (showRange) {
    drawCenteredText("4-20mA", 29, 2);
  }

  if (showSignature) {
    drawCenteredText("v1.5 - IVANI", 53, 1);
  }
}

void Display::drawFaultTags(int16_t x, int16_t y, int16_t maxRightX) {
  // Indicadores compactos de falla: una letra por error.
  // Ejemplos: O=Offset, C=Congelado, R=Ruido.
  // Si llega un texto completo como "Congelamiento", mostramos solo C.
  // Si llega un texto compuesto como "Offset+Congelamiento" o "O+C", mostramos O y C.
  const int16_t boxW = 10;
  const int16_t boxH = 10;
  const int16_t gap = 2;
  const uint8_t maxTags = 5;

  char tags[maxTags];
  uint8_t tagCount = 0;
  bool hasSeparator = false;

  for (uint8_t i = 0; _faultText[i] != '\0'; i++) {
    const char c = _faultText[i];
    if (c == ' ' || c == '+' || c == ',' || c == ';' || c == '/' || c == '|') {
      hasSeparator = true;
      break;
    }
  }

  if (hasSeparator) {
    bool takeNext = true;
    for (uint8_t i = 0; _faultText[i] != '\0' && tagCount < maxTags; i++) {
      char c = _faultText[i];
      const bool isSeparator = (c == ' ' || c == '+' || c == ',' || c == ';' || c == '/' || c == '|');
      if (isSeparator) {
        takeNext = true;
        continue;
      }
      if (takeNext) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        tags[tagCount++] = c;
        takeNext = false;
      }
    }
  } else {
    uint8_t len = 0;
    bool compactCode = true;

    for (uint8_t i = 0; _faultText[i] != '\0'; i++) {
      const char c = _faultText[i];
      len++;
      if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
        compactCode = false;
      }
    }

    if (compactCode && len > 1 && len <= 4) {
      for (uint8_t i = 0; _faultText[i] != '\0' && tagCount < maxTags; i++) {
        char c = _faultText[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        tags[tagCount++] = c;
      }
    } else {
      char c = _faultText[0] ? _faultText[0] : 'F';
      if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
      tags[tagCount++] = c;
    }
  }

  int16_t cursorX = x;
  for (uint8_t i = 0; i < tagCount; i++) {
    if (cursorX + boxW > maxRightX) break;

    _oled.fillRect(cursorX, y, boxW, boxH, SH110X_WHITE);
    _oled.setTextColor(SH110X_BLACK);
    _oled.setCursor(cursorX + 2, y + 1);
    _oled.print(tags[i]);
    _oled.setTextColor(SH110X_WHITE);

    cursorX += boxW + gap;
  }
}

void Display::drawRunStatusIcon(int16_t x, int16_t y, bool running) {
  // Iconos compactos de 10 x 9 px para estado de salida.
  if (running) {
    _oled.drawFastHLine(x + 2, y + 2, 1, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 3, 3, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 4, 5, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 5, 7, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 6, 5, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 7, 3, SH110X_WHITE);
    _oled.drawFastHLine(x + 2, y + 8, 1, SH110X_WHITE);
  } else {
    _oled.fillRect(x + 2, y + 3, 6, 5, SH110X_WHITE);
  }
}

void Display::drawWifiIcon(int16_t x, int16_t y, WiFiStatus status) {
  _oled.drawBitmap(x, y, WIFI_ICON_16X10, 16, 10, SH110X_WHITE);

  if (status == DISPLAY_WIFI_OFF) {
    // WiFi deshabilitado: icono tachado.
    _oled.drawLine(x + 1, y + 9, x + 14, y + 0, SH110X_WHITE);
    _oled.drawLine(x + 2, y + 9, x + 15, y + 0, SH110X_WHITE);
  } else if (status == DISPLAY_WIFI_ON) {
    // WiFi habilitado, pero todavia sin conexion real: mini indicador tipo "!".
    _oled.drawFastVLine(x + 14, y + 2, 5, SH110X_WHITE);
    _oled.drawPixel(x + 14, y + 8, SH110X_WHITE);
  }
}

void Display::drawSpeakerIcon(int16_t x, int16_t y, bool on) {
  _oled.drawBitmap(x, y, on ? SPEAKER_ON_16X10 : SPEAKER_OFF_16X10, 16, 10, SH110X_WHITE);
}

void Display::drawCaptureIcon(int16_t x, int16_t y, bool showCircle) {
  // Icono de captura: encuadre de 14 x 10 px y círculo central intermitente.
  _oled.drawFastHLine(x, y, 5, SH110X_WHITE);
  _oled.drawFastVLine(x, y, 4, SH110X_WHITE);
  _oled.drawFastHLine(x + 9, y, 5, SH110X_WHITE);
  _oled.drawFastVLine(x + 13, y, 4, SH110X_WHITE);
  _oled.drawFastHLine(x, y + 9, 5, SH110X_WHITE);
  _oled.drawFastVLine(x, y + 6, 4, SH110X_WHITE);
  _oled.drawFastHLine(x + 9, y + 9, 5, SH110X_WHITE);
  _oled.drawFastVLine(x + 13, y + 6, 4, SH110X_WHITE);
  if (showCircle) {
    _oled.drawCircle(x + 7, y + 5, 2, SH110X_WHITE);
    _oled.drawPixel(x + 7, y + 5, SH110X_WHITE);
  }
}

uint8_t Display::clampPercent(float value) {
  if (value < 0.0f) return 0;
  if (value > 100.0f) return 100;
  return (uint8_t)(value + 0.5f);
}

float Display::clampPercentFloat(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 100.0f) return 100.0f;
  return value;
}

void Display::copyText(char *dst, size_t dstSize, const char *src) {
  if (dst == nullptr || dstSize == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dstSize);
  dst[dstSize - 1] = '\0';
}

int16_t Display::mapTrendY(uint8_t value, int16_t plotY, int16_t plotH) {
  return plotY + plotH - 1 - ((value * (plotH - 1)) / 100);
}
