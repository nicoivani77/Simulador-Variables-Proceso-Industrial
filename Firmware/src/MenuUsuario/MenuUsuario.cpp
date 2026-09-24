/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de navegación, edición de parámetros y eventos del menú de usuario.
 */

#include "MenuUsuario.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

MenuUsuario::MenuUsuario()
: _screen(SCREEN_MAIN),
  _selected(ITEM_OUTPUT_ENABLE),
  _editingItem(ITEM_OUTPUT_ENABLE),
  _editValue(0.0f),
  _messageUntilMs(0) {
  copyText(_messageTitle, sizeof(_messageTitle), "INFO");
  copyText(_messageText, sizeof(_messageText), "");
}

void MenuUsuario::begin() {
  _screen = SCREEN_MAIN;
  _selected = ITEM_OUTPUT_ENABLE;
}

MenuUsuario::Event MenuUsuario::update(Teclado& teclado, Config& cfg) {
  switch (_screen) {
    case SCREEN_MENU:    return handleMenu(teclado, cfg);
    case SCREEN_EDIT:    return handleEdit(teclado, cfg);
    case SCREEN_MESSAGE: return handleMessage(teclado, cfg);
    case SCREEN_MAIN:
    default:             return handleMain(teclado, cfg);
  }
}

MenuUsuario::Screen MenuUsuario::screen() const { return _screen; }
bool MenuUsuario::isMain() const { return _screen == SCREEN_MAIN; }
bool MenuUsuario::isMenu() const { return _screen == SCREEN_MENU; }
bool MenuUsuario::isEdit() const { return _screen == SCREEN_EDIT; }
bool MenuUsuario::isMessage() const { return _screen == SCREEN_MESSAGE; }

void MenuUsuario::showMain() {
  _screen = SCREEN_MAIN;
}

void MenuUsuario::showMenu() {
  _screen = SCREEN_MENU;
}

void MenuUsuario::showMessage(const char* title, const char* message, uint32_t durationMs) {
  copyText(_messageTitle, sizeof(_messageTitle), title);
  copyText(_messageText, sizeof(_messageText), message);
  _messageUntilMs = millis() + durationMs;
  _screen = SCREEN_MESSAGE;
}

const char* MenuUsuario::menuTitle() const {
  return "MENU USUARIO";
}

void MenuUsuario::getVisibleMenuLines(const Config& cfg,
                                      char line0[22],
                                      char line1[22],
                                      char line2[22],
                                      char line3[22],
                                      uint8_t& cursorLine) const {
  char* lines[4] = {line0, line1, line2, line3};
  const uint8_t first = firstVisibleRow();
  const uint8_t selectedRow = rowForItem(_selected);

  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t row = first + i;

    if (row >= ROW_COUNT) {
      copyText(lines[i], 22, "");
    } else if (isHeaderRow(row)) {
      formatHeaderLine(row, lines[i], 22);
    } else {
      formatItemLine(itemForRow(row), cfg, lines[i], 22);
    }
  }

  if (selectedRow >= first && selectedRow < first + 4) {
    cursorLine = selectedRow - first;
  } else {
    cursorLine = 0;
  }
}

const char* MenuUsuario::editTitle() const {
  return itemName(_editingItem);
}

const char* MenuUsuario::editUnit() const {
  switch (_editingItem) {
    case ITEM_MANUAL_OUTPUT: return "%";
    case ITEM_PROCESS_K:     return "";
    case ITEM_PROCESS_TAU:   return "s";
    case ITEM_PROCESS_DEADTIME: return "s";
    case ITEM_SENSOR_NOISE:  return "%";
    case ITEM_SENSOR_OFFSET: return "%";
    case ITEM_CAL_OUTPUT_4:
    case ITEM_CAL_OUTPUT_20: return "raw";
    default:                 return "";
  }
}

float MenuUsuario::editValue() const {
  return _editValue;
}

const char* MenuUsuario::messageTitle() const { return _messageTitle; }
const char* MenuUsuario::messageText() const { return _messageText; }

MenuUsuario::Event MenuUsuario::handleMain(Teclado& teclado, Config& cfg) {
  if (teclado.fueClick(Teclado::BTN_MENU)) {
    _screen = SCREEN_MENU;
    return EV_NONE;
  }

  // Ajuste directo desde la pantalla principal: activo solo con salida habilitada y modo MANUAL.
  if (cfg.outputEnabled && cfg.manualOutputMode) {
    bool changed = false;

    if (teclado.fueAutoRepeat(Teclado::BTN_SUBIR)) {
      cfg.manualOutputPct += editStepFor(ITEM_MANUAL_OUTPUT);
      changed = true;
    }

    if (teclado.fueAutoRepeat(Teclado::BTN_BAJAR)) {
      cfg.manualOutputPct -= editStepFor(ITEM_MANUAL_OUTPUT);
      changed = true;
    }

    if (changed) {
      if (cfg.manualOutputPct < 0.0f) cfg.manualOutputPct = 0.0f;
      if (cfg.manualOutputPct > 100.0f) cfg.manualOutputPct = 100.0f;
      return EV_MANUAL_OUTPUT_TRIMMED;
    }
  }

  return EV_NONE;
}

MenuUsuario::Event MenuUsuario::handleMenu(Teclado& teclado, Config& cfg) {
  if (teclado.fueClick(Teclado::BTN_MENU)) {
    _screen = SCREEN_MAIN;
    return EV_EXIT_MENU;
  }

  if (teclado.fueAutoRepeat(Teclado::BTN_SUBIR)) {
    moveSelection(-1);
    return EV_NONE;
  }

  if (teclado.fueAutoRepeat(Teclado::BTN_BAJAR)) {
    moveSelection(1);
    return EV_NONE;
  }

  if (!teclado.fueClick(Teclado::BTN_ENTER)) {
    return EV_NONE;
  }

  switch (_selected) {
    case ITEM_OUTPUT_ENABLE:
      cfg.outputEnabled = !cfg.outputEnabled;
      return EV_OUTPUT_ENABLE_CHANGED;

    case ITEM_OUTPUT_MODE:
      cfg.manualOutputMode = !cfg.manualOutputMode;
      return EV_OUTPUT_MODE_CHANGED;

    case ITEM_MANUAL_OUTPUT:
      // En AUTO, Manual OUT queda en tracking para evitar saltos al pasar a MANUAL.
      if (!cfg.manualOutputMode) {
        return EV_NONE;
      }
      beginEdit(_selected, cfg);
      return EV_NONE;

    case ITEM_PROCESS_PRESET:
      cfg.processPreset = nextAutomaticProcessPreset(cfg.processPreset);
      return EV_PRESET_CHANGED;

    case ITEM_PROCESS_K:
    case ITEM_PROCESS_TAU:
    case ITEM_PROCESS_DEADTIME:
    case ITEM_SENSOR_NOISE:
    case ITEM_SENSOR_OFFSET:
      beginEdit(_selected, cfg);
      return EV_NONE;

    case ITEM_SENSOR_FREEZE:
      cfg.sensorFaultMode = (cfg.sensorFaultMode == SENSOR_FAULT_FROZEN) ? SENSOR_FAULT_NONE : SENSOR_FAULT_FROZEN;
      return EV_SENSOR_FREEZE_CHANGED;

    case ITEM_WIFI:
      cfg.wifiEnabled = !cfg.wifiEnabled;
      if (!cfg.wifiEnabled) {
        cfg.wifiConnected = false;
        cfg.wifiRuntime = WIFI_RT_OFF;
      } else {
        cfg.wifiRuntime = WIFI_RT_CONNECTING;
      }
      return EV_WIFI_CHANGED;

    case ITEM_WIFI_INFO:
      return EV_WIFI_INFO_REQUEST;

    case ITEM_WIFI_RESET:
      return EV_WIFI_RESET_REQUEST;

    case ITEM_AUDIO:
      cfg.audioEnabled = !cfg.audioEnabled;
      return EV_AUDIO_CHANGED;

    case ITEM_SD_DETECT:
      return EV_SD_DETECT_REQUEST;

    case ITEM_LOGGING:
      cfg.loggingEnabled = !cfg.loggingEnabled;
      return EV_LOGGING_CHANGED;

    case ITEM_SAVE_DATA:
      return EV_SAVE_DATA_REQUEST;

    case ITEM_CAL_LIVE_RAW:
      // Línea de diagnóstico únicamente informativa. Se actualiza en vivo desde el programa principal.
      return EV_NONE;

    case ITEM_CAL_INPUT_4:
      return EV_CAL_INPUT_4_REQUEST;

    case ITEM_CAL_INPUT_20:
      return EV_CAL_INPUT_20_REQUEST;

    case ITEM_CAL_OUTPUT_4:
      beginEdit(_selected, cfg);
      return EV_CAL_OUTPUT_4_TRIMMED;

    case ITEM_CAL_OUTPUT_20:
      beginEdit(_selected, cfg);
      return EV_CAL_OUTPUT_20_TRIMMED;

    case ITEM_EXIT:
    default:
      _screen = SCREEN_MAIN;
      return EV_EXIT_MENU;
  }
}

MenuUsuario::Event MenuUsuario::handleEdit(Teclado& teclado, Config& cfg) {
  if (teclado.fueClick(Teclado::BTN_MENU)) {
    _screen = SCREEN_MENU;

    if (_editingItem == ITEM_CAL_OUTPUT_4 || _editingItem == ITEM_CAL_OUTPUT_20) {
      return EV_CAL_OUTPUT_CANCELLED;
    }

    return EV_NONE;
  }

  bool valueChanged = false;

  if (teclado.fueAutoRepeat(Teclado::BTN_SUBIR)) {
    _editValue += editStepFor(_editingItem);
    valueChanged = true;
  }

  if (teclado.fueAutoRepeat(Teclado::BTN_BAJAR)) {
    _editValue -= editStepFor(_editingItem);
    valueChanged = true;
  }

  if (_editingItem == ITEM_MANUAL_OUTPUT) {
    if (_editValue < 0.0f) _editValue = 0.0f;
    if (_editValue > 100.0f) _editValue = 100.0f;
  } else if (_editingItem == ITEM_PROCESS_K) {
    if (_editValue < PROCESS_K_MIN) _editValue = PROCESS_K_MIN;
    if (_editValue > PROCESS_K_MAX) _editValue = PROCESS_K_MAX;
  } else if (_editingItem == ITEM_PROCESS_TAU) {
    if (_editValue < PROCESS_TAU_MIN_S) _editValue = PROCESS_TAU_MIN_S;
    if (_editValue > PROCESS_TAU_MAX_S) _editValue = PROCESS_TAU_MAX_S;
  } else if (_editingItem == ITEM_PROCESS_DEADTIME) {
    if (_editValue < 0.0f) _editValue = 0.0f;
    if (_editValue > 30.0f) _editValue = 30.0f;
  } else if (_editingItem == ITEM_SENSOR_NOISE) {
    if (_editValue < 0.0f) _editValue = 0.0f;
    if (_editValue > 10.0f) _editValue = 10.0f;
  } else if (_editingItem == ITEM_SENSOR_OFFSET) {
    if (_editValue < -25.0f) _editValue = -25.0f;
    if (_editValue > 25.0f) _editValue = 25.0f;
  } else if (_editingItem == ITEM_CAL_OUTPUT_4) {
    if (_editValue < 0.0f) _editValue = 0.0f;
    const float maxRaw4 = (cfg.outputRaw20mA > 0) ? (float)(cfg.outputRaw20mA - 1) : 0.0f;
    if (_editValue > maxRaw4) _editValue = maxRaw4;
  } else if (_editingItem == ITEM_CAL_OUTPUT_20) {
    const float minRaw20 = (cfg.outputRaw4mA < 4095) ? (float)(cfg.outputRaw4mA + 1) : 4095.0f;
    if (_editValue < minRaw20) _editValue = minRaw20;
    if (_editValue > 4095.0f) _editValue = 4095.0f;
  }

  if (teclado.fueClick(Teclado::BTN_ENTER)) {
    return commitEdit(cfg);
  }

  if (valueChanged) {
    if (_editingItem == ITEM_CAL_OUTPUT_4) {
      return EV_CAL_OUTPUT_4_TRIMMED;
    }

    if (_editingItem == ITEM_CAL_OUTPUT_20) {
      return EV_CAL_OUTPUT_20_TRIMMED;
    }
  }

  return EV_NONE;
}

MenuUsuario::Event MenuUsuario::handleMessage(Teclado& teclado, Config& cfg) {
  (void)cfg;

  if (teclado.fueClick(Teclado::BTN_MENU) || teclado.fueClick(Teclado::BTN_ENTER)) {
    _screen = SCREEN_MENU;
    return EV_NONE;
  }

  const uint32_t now = millis();
  if ((int32_t)(now - _messageUntilMs) >= 0) {
    _screen = SCREEN_MENU;
  }

  return EV_NONE;
}

void MenuUsuario::moveSelection(int8_t delta) {
  int16_t next = (int16_t)_selected + delta;
  if (next < 0) next = ITEM_COUNT - 1;
  if (next >= ITEM_COUNT) next = 0;
  _selected = static_cast<Item>(next);
}

void MenuUsuario::beginEdit(Item item, const Config& cfg) {
  _editingItem = item;

  switch (item) {
    case ITEM_MANUAL_OUTPUT: _editValue = cfg.manualOutputPct; break;
    case ITEM_PROCESS_K:     _editValue = cfg.processK; break;
    case ITEM_PROCESS_TAU:   _editValue = cfg.processTau; break;
    case ITEM_PROCESS_DEADTIME: _editValue = cfg.processDeadTime; break;
    case ITEM_SENSOR_NOISE:  _editValue = cfg.sensorNoisePct; break;
    case ITEM_SENSOR_OFFSET: _editValue = cfg.sensorOffsetPct; break;
    case ITEM_CAL_OUTPUT_4: _editValue = cfg.outputRaw4mA; break;
    case ITEM_CAL_OUTPUT_20: _editValue = cfg.outputRaw20mA; break;
    default:                 _editValue = 0.0f; break;
  }

  _screen = SCREEN_EDIT;
}

MenuUsuario::Event MenuUsuario::commitEdit(Config& cfg) {
  _screen = SCREEN_MENU;

  switch (_editingItem) {
    case ITEM_MANUAL_OUTPUT:
      cfg.manualOutputPct = _editValue;
      return EV_MANUAL_OUTPUT_CHANGED;

    case ITEM_PROCESS_K:
      cfg.processPreset = PROCESS_PRESET_CUSTOM;
      cfg.processK = _editValue;
      return EV_K_CHANGED;

    case ITEM_PROCESS_TAU:
      cfg.processPreset = PROCESS_PRESET_CUSTOM;
      cfg.processTau = _editValue;
      return EV_TAU_CHANGED;

    case ITEM_PROCESS_DEADTIME:
      cfg.processPreset = PROCESS_PRESET_CUSTOM;
      cfg.processDeadTime = _editValue;
      return EV_DEADTIME_CHANGED;

    case ITEM_SENSOR_NOISE:
      cfg.sensorNoisePct = _editValue;
      return EV_NOISE_CHANGED;

    case ITEM_SENSOR_OFFSET:
      cfg.sensorOffsetPct = _editValue;
      return EV_SENSOR_OFFSET_CHANGED;

    case ITEM_CAL_OUTPUT_4:
      cfg.outputRaw4mA = (uint16_t)lroundf(_editValue);
      return EV_CAL_OUTPUT_4_CHANGED;

    case ITEM_CAL_OUTPUT_20:
      cfg.outputRaw20mA = (uint16_t)lroundf(_editValue);
      return EV_CAL_OUTPUT_20_CHANGED;

    default:
      return EV_NONE;
  }
}

float MenuUsuario::editStepFor(Item item) const {
  switch (item) {
    case ITEM_MANUAL_OUTPUT: return 1.0f;
    case ITEM_PROCESS_K:     return 0.05f;
    case ITEM_PROCESS_TAU:   return 0.1f;
    case ITEM_PROCESS_DEADTIME: return 0.1f;
    case ITEM_SENSOR_NOISE:  return 0.1f;
    case ITEM_SENSOR_OFFSET: return 0.5f;
    case ITEM_CAL_OUTPUT_4:
    case ITEM_CAL_OUTPUT_20: return 5.0f;
    default:                 return 1.0f;
  }
}

uint8_t MenuUsuario::firstVisibleRow() const {
  const uint8_t selectedRow = rowForItem(_selected);
  const uint8_t headerRow = headerRowForItem(_selected);

  uint8_t first = 0;

  // Si el grupo entra completo en las 4 líneas visibles, se muestra su título.
  // En grupos largos, se prioriza que la opción seleccionada quede visible.
  if ((selectedRow - headerRow) <= 3) {
    first = headerRow;
  } else {
    first = selectedRow - 3;
  }

  if (ROW_COUNT <= 4) return 0;
  if (first > ROW_COUNT - 4) first = ROW_COUNT - 4;
  return first;
}

uint8_t MenuUsuario::rowForItem(Item item) const {
  switch (item) {
    case ITEM_OUTPUT_ENABLE:    return ROW_OUTPUT_ENABLE;
    case ITEM_OUTPUT_MODE:   return ROW_OUTPUT_MODE;
    case ITEM_MANUAL_OUTPUT: return ROW_MANUAL_OUTPUT;
    case ITEM_PROCESS_PRESET:return ROW_PROCESS_PRESET;
    case ITEM_PROCESS_K:     return ROW_PROCESS_K;
    case ITEM_PROCESS_TAU:   return ROW_PROCESS_TAU;
    case ITEM_PROCESS_DEADTIME: return ROW_PROCESS_DEADTIME;
    case ITEM_SENSOR_NOISE:  return ROW_SENSOR_NOISE;
    case ITEM_SENSOR_FREEZE:  return ROW_SENSOR_FREEZE;
    case ITEM_SENSOR_OFFSET: return ROW_SENSOR_OFFSET;
    case ITEM_WIFI:          return ROW_WIFI;
    case ITEM_WIFI_INFO:     return ROW_WIFI_INFO;
    case ITEM_WIFI_RESET:    return ROW_WIFI_RESET;
    case ITEM_AUDIO:         return ROW_AUDIO;
    case ITEM_SD_DETECT:     return ROW_SD_DETECT;
    case ITEM_LOGGING:       return ROW_LOGGING;
    case ITEM_SAVE_DATA:     return ROW_SAVE_DATA;
    case ITEM_CAL_LIVE_RAW:  return ROW_CAL_LIVE_RAW;
    case ITEM_CAL_INPUT_4:   return ROW_CAL_INPUT_4;
    case ITEM_CAL_INPUT_20:  return ROW_CAL_INPUT_20;
    case ITEM_CAL_OUTPUT_4: return ROW_CAL_OUTPUT_4;
    case ITEM_CAL_OUTPUT_20:return ROW_CAL_OUTPUT_20;
    case ITEM_EXIT:
    default:                 return ROW_EXIT;
  }
}

uint8_t MenuUsuario::headerRowForItem(Item item) const {
  switch (item) {
    case ITEM_OUTPUT_ENABLE:
    case ITEM_OUTPUT_MODE:
    case ITEM_MANUAL_OUTPUT:
      return ROW_CONTROL_HEADER;

    case ITEM_PROCESS_PRESET:
    case ITEM_PROCESS_K:
    case ITEM_PROCESS_TAU:
    case ITEM_PROCESS_DEADTIME:
    case ITEM_SENSOR_NOISE:
    case ITEM_SENSOR_OFFSET:
    case ITEM_SENSOR_FREEZE:
      return ROW_MODEL_HEADER;

    case ITEM_WIFI:
    case ITEM_WIFI_INFO:
    case ITEM_WIFI_RESET:
      return ROW_WIFI_HEADER;

    case ITEM_AUDIO:
    case ITEM_SD_DETECT:
      return ROW_SYSTEM_HEADER;

    case ITEM_LOGGING:
    case ITEM_SAVE_DATA:
      return ROW_DATA_HEADER;

    case ITEM_CAL_LIVE_RAW:
    case ITEM_CAL_INPUT_4:
    case ITEM_CAL_INPUT_20:
    case ITEM_CAL_OUTPUT_4:
    case ITEM_CAL_OUTPUT_20:
      return ROW_CALIBRATION_HEADER;

    case ITEM_EXIT:
    default:
      return ROW_GENERAL_HEADER;
  }
}

bool MenuUsuario::isHeaderRow(uint8_t row) const {
  switch (row) {
    case ROW_CONTROL_HEADER:
    case ROW_MODEL_HEADER:
    case ROW_WIFI_HEADER:
    case ROW_SYSTEM_HEADER:
    case ROW_DATA_HEADER:
    case ROW_CALIBRATION_HEADER:
    case ROW_GENERAL_HEADER:
      return true;
    default:
      return false;
  }
}

MenuUsuario::Item MenuUsuario::itemForRow(uint8_t row) const {
  switch (row) {
    case ROW_OUTPUT_ENABLE:    return ITEM_OUTPUT_ENABLE;
    case ROW_OUTPUT_MODE:   return ITEM_OUTPUT_MODE;
    case ROW_MANUAL_OUTPUT: return ITEM_MANUAL_OUTPUT;
    case ROW_PROCESS_PRESET:return ITEM_PROCESS_PRESET;
    case ROW_PROCESS_K:     return ITEM_PROCESS_K;
    case ROW_PROCESS_TAU:   return ITEM_PROCESS_TAU;
    case ROW_PROCESS_DEADTIME: return ITEM_PROCESS_DEADTIME;
    case ROW_SENSOR_NOISE:  return ITEM_SENSOR_NOISE;
    case ROW_SENSOR_FREEZE:  return ITEM_SENSOR_FREEZE;
    case ROW_SENSOR_OFFSET: return ITEM_SENSOR_OFFSET;
    case ROW_WIFI:          return ITEM_WIFI;
    case ROW_WIFI_INFO:     return ITEM_WIFI_INFO;
    case ROW_WIFI_RESET:    return ITEM_WIFI_RESET;
    case ROW_AUDIO:         return ITEM_AUDIO;
    case ROW_SD_DETECT:     return ITEM_SD_DETECT;
    case ROW_LOGGING:       return ITEM_LOGGING;
    case ROW_SAVE_DATA:     return ITEM_SAVE_DATA;
    case ROW_CAL_LIVE_RAW:  return ITEM_CAL_LIVE_RAW;
    case ROW_CAL_INPUT_4:   return ITEM_CAL_INPUT_4;
    case ROW_CAL_INPUT_20:  return ITEM_CAL_INPUT_20;
    case ROW_CAL_OUTPUT_4: return ITEM_CAL_OUTPUT_4;
    case ROW_CAL_OUTPUT_20:return ITEM_CAL_OUTPUT_20;
    case ROW_EXIT:
    default:                return ITEM_EXIT;
  }
}

void MenuUsuario::formatHeaderLine(uint8_t row, char* out, size_t outSize) const {
  switch (row) {
    case ROW_CONTROL_HEADER:
      snprintf(out, outSize, "[ CONTROL ]");
      break;
    case ROW_MODEL_HEADER:
      snprintf(out, outSize, "[ MODELO ]");
      break;
    case ROW_WIFI_HEADER:
      snprintf(out, outSize, "[ WIFI ]");
      break;
    case ROW_SYSTEM_HEADER:
      snprintf(out, outSize, "[ SISTEMA ]");
      break;
    case ROW_DATA_HEADER:
      snprintf(out, outSize, "[ DATOS ]");
      break;
    case ROW_CALIBRATION_HEADER:
      snprintf(out, outSize, "[ CALIBRACION ]");
      break;
    case ROW_GENERAL_HEADER:
      snprintf(out, outSize, "[ GENERAL ]");
      break;
    default:
      snprintf(out, outSize, "");
      break;
  }
}

void MenuUsuario::formatItemLine(Item item, const Config& cfg, char* out, size_t outSize) const {
  switch (item) {
    case ITEM_OUTPUT_ENABLE:
      snprintf(out, outSize, "Salida:%s", cfg.outputEnabled ? "ACTIVA" : "INACTIVA");
      break;
    case ITEM_OUTPUT_MODE:
      snprintf(out, outSize, "Modo salida:%s", cfg.manualOutputMode ? "MAN" : "AUTO");
      break;
    case ITEM_MANUAL_OUTPUT:
      if (cfg.manualOutputMode) {
        snprintf(out, outSize, "Manual OUT:%3.0f%%", cfg.manualOutputPct);
      } else {
        snprintf(out, outSize, "Manual OUT:TRACK");
      }
      break;
    case ITEM_PROCESS_PRESET:
      snprintf(out, outSize, "Preset:%s", presetName(cfg.processPreset));
      break;
    case ITEM_PROCESS_K:
      snprintf(out, outSize, "Ganancia K:%.2f", cfg.processK);
      break;
    case ITEM_PROCESS_TAU:
      snprintf(out, outSize, "Tau:%.1fs", cfg.processTau);
      break;
    case ITEM_PROCESS_DEADTIME:
      snprintf(out, outSize, "Retardo:%.1fs", cfg.processDeadTime);
      break;
    case ITEM_SENSOR_NOISE:
      snprintf(out, outSize, "Ruido:+/-%.1f%%", cfg.sensorNoisePct);
      break;
    case ITEM_SENSOR_FREEZE:
      snprintf(out, outSize, "Congelar:%s", sensorFreezeName(cfg.sensorFaultMode));
      break;
    case ITEM_SENSOR_OFFSET:
      snprintf(out, outSize, "Offset:%+.1f%%", cfg.sensorOffsetPct);
      break;
    case ITEM_WIFI: {
      const char* modeText = "OFF";
      if (cfg.wifiEnabled) {
        switch (cfg.wifiRuntime) {
          case WIFI_RT_STA:        modeText = "STA"; break;
          case WIFI_RT_AP:         modeText = "AP"; break;
          case WIFI_RT_CONNECTING: modeText = "CONN"; break;
          case WIFI_RT_OFF:
          default:                 modeText = cfg.wifiConnected ? "STA" : "ON"; break;
        }
      }
      snprintf(out, outSize, "WiFi:%s", modeText);
      break;
    }
    case ITEM_WIFI_INFO:
      snprintf(out, outSize, "Info WiFi");
      break;
    case ITEM_WIFI_RESET:
      snprintf(out, outSize, "Borrar WiFi guard.");
      break;
    case ITEM_AUDIO:
      snprintf(out, outSize, "Audio:%s", cfg.audioEnabled ? "SI" : "NO");
      break;
    case ITEM_SD_DETECT:
      snprintf(out, outSize, "Detectar SD: %s", cfg.sdReady ? "OK" : "FALLA");
      break;
    case ITEM_LOGGING:
      snprintf(out, outSize, "Capturar datos:%s", cfg.loggingEnabled ? "SI" : "NO");
      break;
    case ITEM_SAVE_DATA:
      snprintf(out, outSize, "Guardar datos");
      break;
    case ITEM_CAL_LIVE_RAW:
      snprintf(out, outSize, "ADC:%u DAC:%u", cfg.inputRawCurrent, cfg.outputRawCurrent);
      break;
    case ITEM_CAL_INPUT_4:
      snprintf(out, outSize, "Cal entrada 4mA");
      break;
    case ITEM_CAL_INPUT_20:
      snprintf(out, outSize, "Cal entrada 20mA");
      break;
    case ITEM_CAL_OUTPUT_4:
      snprintf(out, outSize, "Cal salida 4:%u", cfg.outputRaw4mA);
      break;
    case ITEM_CAL_OUTPUT_20:
      snprintf(out, outSize, "Cal salida20:%u", cfg.outputRaw20mA);
      break;
    case ITEM_EXIT:
    default:
      snprintf(out, outSize, "Salir");
      break;
  }
}

const char* MenuUsuario::itemName(Item item) const {
  switch (item) {
    case ITEM_OUTPUT_ENABLE:    return "SALIDA";
    case ITEM_OUTPUT_MODE:   return "MODO SALIDA";
    case ITEM_MANUAL_OUTPUT: return "MANUAL OUT";
    case ITEM_PROCESS_PRESET:return "PRESET";
    case ITEM_PROCESS_K:     return "GANANCIA K";
    case ITEM_PROCESS_TAU:   return "TAU [s]";
    case ITEM_PROCESS_DEADTIME: return "RETARDO[s]";
    case ITEM_SENSOR_NOISE:  return "RUIDO [%]";
    case ITEM_SENSOR_FREEZE:  return "CONGELAR";
    case ITEM_SENSOR_OFFSET: return "OFFSET [%]";
    case ITEM_WIFI:          return "WIFI";
    case ITEM_WIFI_INFO:     return "INFO WIFI";
    case ITEM_WIFI_RESET:    return "BORRAR WIFI";
    case ITEM_AUDIO:         return "AUDIO";
    case ITEM_SD_DETECT:     return "DETECTAR SD";
    case ITEM_LOGGING:       return "CAPTURAR DATOS";
    case ITEM_SAVE_DATA:     return "GUARDAR DATOS";
    case ITEM_CAL_LIVE_RAW:  return "RAW ACTUAL";
    case ITEM_CAL_INPUT_4:   return "CAL ENTRADA 4mA";
    case ITEM_CAL_INPUT_20:  return "CAL ENTRADA 20mA";
    case ITEM_CAL_OUTPUT_4: return "CAL SALIDA 4mA";
    case ITEM_CAL_OUTPUT_20:return "CAL SALIDA 20mA";
    case ITEM_EXIT:
    default:                 return "SALIR";
  }
}

const char* MenuUsuario::sensorFreezeName(SensorFaultMode mode) {
  return (mode == SENSOR_FAULT_FROZEN) ? "SI" : "NO";
}

const char* MenuUsuario::presetName(ProcessPreset preset) {
  return processPresetShortName(preset);
}

void MenuUsuario::copyText(char* dst, size_t dstSize, const char* src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize);
  dst[dstSize - 1] = '\0';
}
