/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Interfaz de control para la pantalla OLED y sus estados visuales.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

class Display {
public:
  static const uint8_t SCREEN_WIDTH  = 128;
  static const uint8_t SCREEN_HEIGHT = 64;

  enum ScreenMode : uint8_t {
    SCREEN_MAIN = 0,
    SCREEN_MENU,
    SCREEN_EDIT,
    SCREEN_MESSAGE,
    SCREEN_SPLASH,
    SCREEN_SPECIAL
  };

  // Estado visual del WiFi.
  enum WiFiStatus : uint8_t {
    DISPLAY_WIFI_OFF = 0,       // WiFi deshabilitado
    DISPLAY_WIFI_ON,            // WiFi habilitado, sin conexion real todavia
    DISPLAY_WIFI_CONNECTED      // Estado de conexión WiFi establecida
  };

  Display();

  bool begin(int sdaPin = 8, int sclPin = 9, uint8_t i2cAddress = 0x3C);

  void update();
  void forceRefresh();

  // Pantallas animadas de inicio y apagado. Ocupan los 128x64 px completos,
  // sin barra superior, y utilizan una estética técnica/industrial.
  void setStartupSplash(const char* version, uint32_t durationMs = 2600);
  void setShutdownSplash(const char* version, uint32_t durationMs = 2200);
  void setSpecialScreen(uint32_t durationMs = 7000);
  bool isSpecialScreenActive() const;

  // Apaga el panel OLED y deja la clase preparada para ser reinicializada con begin().
  void sleep();

  void setScreenMode(ScreenMode mode);

  // Barra superior
  void setWiFiStatus(WiFiStatus status);
  void setWiFiEnabled(bool enabled);
  void setWiFiConnected(bool connected);
  void setBuzzerEnabled(bool enabled);
  void setCaptureActive(bool active);

  // Pantalla principal
  void setChannelName(const char *name);
  void setChannelEnabled(bool enabled);
  void setChannelMode(const char *mode);
  void setReadingPercent(float percent);
  void setOutputPercent(float percent);
  void setManualModeIndicator(bool enabled);
  void setAlarms(bool high, bool low);
  void setReadingLabel(const char *label);
  void setOutputLabel(const char *label);
  void setFaultStatus(const char* text, bool active);

  // Pantalla de menú / edición / mensajes
  void setMenuScreen(const char* title,
                     const char* line0,
                     const char* line1,
                     const char* line2,
                     const char* line3,
                     uint8_t cursorLine);

  void setEditScreen(const char* title, float value, const char* unit);
  void setMessageScreen(const char* title, const char* message);

private:
  static const int8_t OLED_RESET = -1;
  static const uint16_t RENDER_INTERVAL_MS = 100;
  static const uint16_t TREND_INTERVAL_MS  = 250;
  static const uint16_t CAPTURE_BLINK_INTERVAL_MS = 500;
  static const uint16_t SPECIAL_FRAME_INTERVAL_MS = 120;
  static const uint8_t TREND_SAMPLES       = 96;

  Adafruit_SH1106G _oled;

  bool _initialized;
  bool _dirty;
  ScreenMode _screenMode;

  int _sdaPin;
  int _sclPin;
  uint8_t _i2cAddress;

  // Barra superior
  WiFiStatus _wifiStatus;
  bool _buzzerEnabled;
  bool _captureActive;
  bool _captureDotVisible;

  // Canal
  char _channelName[8];
  bool _channelEnabled;
  char _channelMode[10];
  bool _alarmHigh;
  bool _alarmLow;
  float _readingPercent;
  float _outputPercent;
  bool _manualModeIndicator;
  bool _faultActive;
  char _faultText[32];

  // Etiquetas
  char _readingLabel[10];
  char _outputLabel[10];

  // Menú
  char _menuTitle[22];
  char _menuLines[4][22];
  uint8_t _menuCursorLine;

  // Edición
  char _editTitle[22];
  char _editUnit[8];
  float _editValue;

  // Mensaje
  char _messageTitle[22];
  char _messageText[70];

  // Splash animado de inicio/apagado.
  bool _splashShutdown;
  char _splashVersion[12];
  uint32_t _splashStartMs;
  uint32_t _splashDurationMs;
  uint32_t _lastSplashFrameMs;

  // Pantalla especial animada.
  uint32_t _specialStartMs;
  uint32_t _specialDurationMs;
  uint32_t _lastSpecialFrameMs;

  // Tendencias
  uint8_t _readingTrend[TREND_SAMPLES];
  uint8_t _outputTrend[TREND_SAMPLES];

  uint32_t _lastRenderMs;
  uint32_t _lastTrendSampleMs;
  uint32_t _lastCaptureBlinkMs;

  void renderNow();
  void sampleTrend();
  void clearTrend(uint8_t readingValue = 0, uint8_t outputValue = 0);

  void drawTopBar();
  void drawFaultTags(int16_t x, int16_t y, int16_t maxRightX);
  void drawRunStatusIcon(int16_t x, int16_t y, bool running);
  void drawMainLayout();
  void drawInfoRow();
  void drawTrendRow(int16_t y, const char *label, const uint8_t *trend);
  void drawManualModeIndicator();
  void drawMenuLayout();
  void drawEditLayout();
  void drawMessageLayout();
  void drawSplashLayout();
  void drawSpecialLayout();
  void drawRoundedSimulador(uint8_t visiblePercent, int16_t shineX = -100);
  void drawCenteredText(const char* text, int16_t y, uint8_t textSize);

  void drawWifiIcon(int16_t x, int16_t y, WiFiStatus status);
  void drawSpeakerIcon(int16_t x, int16_t y, bool on);
  void drawCaptureIcon(int16_t x, int16_t y, bool showCircle);

  static uint8_t clampPercent(float value);
  static float clampPercentFloat(float value);
  static void copyText(char *dst, size_t dstSize, const char *src);
  static int16_t mapTrendY(uint8_t value, int16_t plotY, int16_t plotH);
};

#endif
