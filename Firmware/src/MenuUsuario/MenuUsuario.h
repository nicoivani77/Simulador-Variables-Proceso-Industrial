/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Interfaz del menú de usuario, estructura de configuración y eventos de operación.
 */

#ifndef MENUUSUARIO_H
#define MENUUSUARIO_H

#include <Arduino.h>
#include "../Teclado/Teclado.h"
#include "../ProcessPresets/ProcessPresets.h"

class MenuUsuario {
public:
  enum Screen : uint8_t {
    SCREEN_MAIN = 0,
    SCREEN_MENU,
    SCREEN_EDIT,
    SCREEN_MESSAGE
  };

  enum Event : uint8_t {
    EV_NONE = 0,
    EV_OUTPUT_ENABLE_CHANGED,
    EV_PRESET_CHANGED,
    EV_OUTPUT_MODE_CHANGED,
    EV_MANUAL_OUTPUT_CHANGED,
    EV_MANUAL_OUTPUT_TRIMMED,
    EV_K_CHANGED,
    EV_TAU_CHANGED,
    EV_DEADTIME_CHANGED,
    EV_NOISE_CHANGED,
    EV_SENSOR_FREEZE_CHANGED,
    EV_SENSOR_OFFSET_CHANGED,
    EV_WIFI_CHANGED,
    EV_WIFI_INFO_REQUEST,
    EV_WIFI_RESET_REQUEST,
    EV_AUDIO_CHANGED,
    EV_SD_DETECT_REQUEST,
    EV_LOGGING_CHANGED,
    EV_SAVE_DATA_REQUEST,
    EV_CAL_INPUT_4_REQUEST,
    EV_CAL_INPUT_20_REQUEST,
    EV_CAL_OUTPUT_4_TRIMMED,
    EV_CAL_OUTPUT_20_TRIMMED,
    EV_CAL_OUTPUT_4_CHANGED,
    EV_CAL_OUTPUT_20_CHANGED,
    EV_CAL_OUTPUT_CANCELLED,
    EV_EXIT_MENU
  };

  enum WiFiRuntime : uint8_t {
    WIFI_RT_OFF = 0,
    WIFI_RT_CONNECTING,
    WIFI_RT_STA,
    WIFI_RT_AP
  };

  enum SensorFaultMode : uint8_t {
    SENSOR_FAULT_NONE = 0,
    SENSOR_FAULT_FROZEN,
    SENSOR_FAULT_COUNT
  };

  struct Config {
    bool outputEnabled = false;
    bool manualOutputMode = false;
    float manualOutputPct = 0.0f;
    ProcessPreset processPreset = PROCESS_PRESET_CUSTOM;
    float processK = 1.0f;
    float processTau = 2.0f;
    float processDeadTime = 0.0f;
    float sensorNoisePct = 0.0f;
    SensorFaultMode sensorFaultMode = SENSOR_FAULT_NONE;
    float sensorOffsetPct = 0.0f;
    bool wifiEnabled = false;
    bool wifiConnected = false;
    WiFiRuntime wifiRuntime = WIFI_RT_OFF;
    bool audioEnabled = true;
    bool sdReady = false;
    bool loggingEnabled = false;

    // Calibración de salida: valores RAW del MCP4725 medidos para 4 mA y 20 mA.
    uint16_t outputRaw4mA = 440;
    uint16_t outputRaw20mA = 4000;

    // Diagnóstico en vivo para la sección de calibración.
    // inputRawCurrent es el RAW ADC filtrado realmente usado para convertir a mA/%.
    // outputRawCurrent es la última cuenta escrita al DAC.
    uint16_t inputRawCurrent = 0;
    uint16_t outputRawCurrent = 0;
  };

  MenuUsuario();

  void begin();
  Event update(Teclado& teclado, Config& cfg);

  Screen screen() const;
  bool isMain() const;
  bool isMenu() const;
  bool isEdit() const;
  bool isMessage() const;

  void showMain();
  void showMenu();
  void showMessage(const char* title, const char* message, uint32_t durationMs = 1200);

  // Datos visibles del menú para la OLED.
  const char* menuTitle() const;
  void getVisibleMenuLines(const Config& cfg,
                           char line0[22],
                           char line1[22],
                           char line2[22],
                           char line3[22],
                           uint8_t& cursorLine) const;

  const char* editTitle() const;
  const char* editUnit() const;
  float editValue() const;

  const char* messageTitle() const;
  const char* messageText() const;

private:
  enum Item : uint8_t {
    ITEM_OUTPUT_ENABLE = 0,
    ITEM_OUTPUT_MODE,
    ITEM_MANUAL_OUTPUT,
    ITEM_PROCESS_PRESET,
    ITEM_PROCESS_K,
    ITEM_PROCESS_TAU,
    ITEM_PROCESS_DEADTIME,
    ITEM_SENSOR_NOISE,
    ITEM_SENSOR_OFFSET,
    ITEM_SENSOR_FREEZE,
    ITEM_WIFI,
    ITEM_WIFI_INFO,
    ITEM_WIFI_RESET,
    ITEM_AUDIO,
    ITEM_SD_DETECT,
    ITEM_LOGGING,
    ITEM_SAVE_DATA,
    ITEM_CAL_LIVE_RAW,
    ITEM_CAL_INPUT_4,
    ITEM_CAL_INPUT_20,
    ITEM_CAL_OUTPUT_4,
    ITEM_CAL_OUTPUT_20,
    ITEM_EXIT,
    ITEM_COUNT
  };

  enum Row : uint8_t {
    ROW_CONTROL_HEADER = 0,
    ROW_OUTPUT_ENABLE,
    ROW_OUTPUT_MODE,
    ROW_MANUAL_OUTPUT,
    ROW_MODEL_HEADER,
    ROW_PROCESS_PRESET,
    ROW_PROCESS_K,
    ROW_PROCESS_TAU,
    ROW_PROCESS_DEADTIME,
    ROW_SENSOR_NOISE,
    ROW_SENSOR_OFFSET,
    ROW_SENSOR_FREEZE,
    ROW_WIFI_HEADER,
    ROW_WIFI,
    ROW_WIFI_INFO,
    ROW_WIFI_RESET,
    ROW_SYSTEM_HEADER,
    ROW_AUDIO,
    ROW_SD_DETECT,
    ROW_DATA_HEADER,
    ROW_LOGGING,
    ROW_SAVE_DATA,
    ROW_CALIBRATION_HEADER,
    ROW_CAL_LIVE_RAW,
    ROW_CAL_INPUT_4,
    ROW_CAL_INPUT_20,
    ROW_CAL_OUTPUT_4,
    ROW_CAL_OUTPUT_20,
    ROW_GENERAL_HEADER,
    ROW_EXIT,
    ROW_COUNT
  };

  Screen _screen;
  Item _selected;
  Item _editingItem;
  float _editValue;
  uint32_t _messageUntilMs;
  char _messageTitle[22];
  char _messageText[70];

  Event handleMain(Teclado& teclado, Config& cfg);
  Event handleMenu(Teclado& teclado, Config& cfg);
  Event handleEdit(Teclado& teclado, Config& cfg);
  Event handleMessage(Teclado& teclado, Config& cfg);

  void moveSelection(int8_t delta);
  void beginEdit(Item item, const Config& cfg);
  Event commitEdit(Config& cfg);
  float editStepFor(Item item) const;

  uint8_t firstVisibleRow() const;
  uint8_t rowForItem(Item item) const;
  uint8_t headerRowForItem(Item item) const;
  bool isHeaderRow(uint8_t row) const;
  Item itemForRow(uint8_t row) const;
  void formatHeaderLine(uint8_t row, char* out, size_t outSize) const;
  void formatItemLine(Item item, const Config& cfg, char* out, size_t outSize) const;
  const char* itemName(Item item) const;
  static const char* sensorFreezeName(SensorFaultMode mode);
  static const char* presetName(ProcessPreset preset);
  static void copyText(char* dst, size_t dstSize, const char* src);
};

#endif
