/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Programa principal: inicializa periféricos y coordina entrada, salida, menú, display, WiFi y captura en SD.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#ifdef ESP32
#include <esp_system.h>
#include <esp_sleep.h>
#endif

#include "src/BuzzerLib/BuzzerLib.h"
#include "src/Display/Display.h"
#include "src/Generar_4_20mA/Generar_4_20mA.h"
#include "src/InterfaceWiFi/InterfaceWiFi.h"
#include "src/Leer_4_20mA/Leer_4_20mA.h"
#include "src/MenuUsuario/MenuUsuario.h"
#include "src/ProcessPresets/ProcessPresets.h"
#include "src/SDLogger/SDLogger.h"
#include "src/Teclado/Teclado.h"

// ============================================================
// IDENTIFICACIÓN / PRESENTACIÓN
// ============================================================
static constexpr const char* PROJECT_VERSION = "v1.5";
static constexpr uint32_t STARTUP_SPLASH_MIN_MS  = 2600;
static constexpr uint32_t SHUTDOWN_SPLASH_MIN_MS = 2200;
static constexpr uint32_t SPECIAL_SCREEN_MS = 5600;

// ============================================================
// PINOUT PLACA VERSION 1.5
// ============================================================
static constexpr uint8_t PIN_PWR_BTN      = 15;
static constexpr uint8_t PIN_AO_PLC_ADC   = 6;   // ADC1_CH5
static constexpr uint8_t PIN_I2C_SDA      = 8;
static constexpr uint8_t PIN_I2C_SCL      = 9;
static constexpr uint8_t PIN_SD_CS        = 10;
static constexpr uint8_t PIN_SD_MOSI      = 11;
static constexpr uint8_t PIN_SD_CLK       = 12;
static constexpr uint8_t PIN_SD_MISO      = 13;
static constexpr uint8_t PIN_BUZZER       = 21;

// ============================================================
// CALIBRACIONES INICIALES
// ============================================================
static constexpr uint16_t DEFAULT_INPUT_RAW_4MA  = 820;
static constexpr uint16_t DEFAULT_INPUT_RAW_20MA = 3270;

// Span mínimo aceptado para evitar calibraciones inválidas o demasiado sensibles al ruido.
static constexpr uint16_t MIN_INPUT_CAL_SPAN = 300;

// Trazas periódicas por Serial para diagnóstico de ADC, corriente y porcentaje.
static constexpr bool DEBUG_INPUT_ADC = false;
static constexpr uint32_t DEBUG_INPUT_PERIOD_MS = 500;

static constexpr uint16_t DEFAULT_OUTPUT_RAW_4MA  = 440;
static constexpr uint16_t DEFAULT_OUTPUT_RAW_20MA = 4000;
static constexpr uint16_t MIN_OUTPUT_CAL_SPAN = 100;

// ============================================================
// CAPTURA DE DATOS EN SD
// ============================================================
static constexpr uint32_t SD_SPI_FREQUENCY_HZ = 1000000UL;   // 1 MHz: robusto para cables/jumpers.
static constexpr uint32_t CAPTURE_PERIOD_MS   = 1000;        // 1 muestra por segundo.
static constexpr uint32_t SD_STATUS_CHECK_PERIOD_MS = 10000; // Chequeo lento de presencia SD.
static constexpr uint8_t  MAX_HISTORY_FILES   = 99;

// Guardado diferido del ajuste manual desde pantalla principal.
// Reduce escrituras en flash/NVS durante auto-repeat de las flechas.
static constexpr uint32_t MANUAL_TRIM_SAVE_DELAY_MS = 1200;

// ============================================================
// WIFI / INTERFAZ WEB
// ============================================================
static constexpr const char* WIFI_HOSTNAME = "simulador-pid";
static constexpr const char* WIFI_AP_SSID  = "Simulador420_Wifi";
static constexpr const char* WIFI_AP_PASS  = "1234";  // Aviso: WPA/WPA2 exige 8 caracteres; InterfaceWiFi levanta AP abierto si la clave es menor.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
static constexpr uint32_t WIFI_STATUS_SYNC_MS = 1000;

// ============================================================
// OBJETOS GLOBALES
// ============================================================
BuzzerLib buzzer(PIN_BUZZER);
Display display;
Teclado teclado(25);
Leer_4_20mA entrada(PIN_AO_PLC_ADC, DEFAULT_INPUT_RAW_4MA, DEFAULT_INPUT_RAW_20MA, 2000, 3, 4);

Generar_4_20mALib::Config outputCfg;
Generar_4_20mALib salida(outputCfg);

SDLogger sdLogger;
InterfaceWiFiClass wifiInterface;
MenuUsuario menu;
MenuUsuario::Config menuCfg;
Preferences prefs;

// ============================================================
// ESTADO DE SISTEMA
// ============================================================
static bool peripheralsOn = true;
static bool displayReady = false;
static bool outputReady = false;
static bool sdReady = false;
static bool wifiActive = false;

static uint16_t inputRaw4mA = DEFAULT_INPUT_RAW_4MA;
static uint16_t inputRaw20mA = DEFAULT_INPUT_RAW_20MA;

static float inputmA = 4.0f;
static float inputPct = 0.0f;

static uint32_t lastControlMs = 0;
static uint32_t lastDisplaySyncMs = 0;
static uint32_t lastLogMs = 0;
static uint32_t lastWiFiSyncMs = 0;
static uint32_t lastSdStatusCheckMs = 0;

static char capturePath[32] = "";
static char lastCsvPath[32] = "";
static uint8_t nextHistoryFileIndex = 1;
static bool historyIndexCacheValid = false;
static uint32_t captureStartMs = 0;

static bool pendingManualOutputSave = false;
static uint32_t lastManualOutputTrimMs = 0;

// Retención de salida para pruebas de calibración en 0% o 100%.
static bool outputCalibrationHoldActive = false;
static float outputCalibrationHoldPct = 0.0f;
static uint16_t outputCalibrationHoldRaw4mA = DEFAULT_OUTPUT_RAW_4MA;
static uint16_t outputCalibrationHoldRaw20mA = DEFAULT_OUTPUT_RAW_20MA;
static uint32_t outputCalibrationHoldUntilMs = 0;

// Antirrebote simple para PWR_BTN
static bool pwrRaw = false;
static bool pwrStable = false;
static bool ignoreNextPwrRelease = false;
static uint32_t pwrLastChangeMs = 0;

// ============================================================
// PROTOTIPOS / UTILIDADES COMPARTIDAS
// ============================================================
static float clampFloat(float x, float lo, float hi);
MenuUsuario::SensorFaultMode clampMenuSensorFaultMode(uint8_t raw);
Generar_4_20mALib::SensorFaultMode mapSensorFaultToOutput(MenuUsuario::SensorFaultMode mode);
void applyOutputModelConfig();
void applyProcessPresetToMenuConfig(ProcessPreset preset);
void handleWiFiProcessConfigRequests();
void handleWiFiLogConfigRequests();
void handleWiFiUserConfigRequests();
void handleWiFiUserActionRequests();
void syncUserConfigSnapshotToWiFi();
uint16_t loadCsvLogMaskFromPreferences();
bool applyCsvLogMask(uint16_t mask, bool restartActiveCapture);
bool detectSdCard();
bool probeMountedSdCard();
void markSdUnavailable();
void updateSdStatusPeriodic();
bool parseHistoryDataIndex(const char* path, uint8_t& indexOut);
void rememberLastCsvPath(const char* path, bool persist);
void clearRuntimeLastCsvPath();
bool startDataCapture();
void stopDataCapture();
void buildCsvErrorText(char* out, size_t outSize);
bool outputIsFromCalibration();
const char* displayCaptureFileName();
bool isOutputCalibrationValid(uint16_t raw4, uint16_t raw20);
void resetOutputCalibrationToDefaults();
bool saveOutputCalibration();
void applyOutputCalibrationToDac();
bool holdOutputCalibrationPoint(float percent);
bool holdOutputCalibrationPoint(float percent, uint16_t raw4, uint16_t raw20);
void cancelOutputCalibrationHold();
void syncDisplay();
void serviceSplashUntil(uint32_t startMs, uint32_t minimumMs);
void enterDeepSleep();

// ============================================================
// CONFIGURACIÓN PERSISTENTE
// ============================================================
bool isInputCalibrationValid(uint16_t raw4, uint16_t raw20) {
  if (raw4 > 4095 || raw20 > 4095) return false;
  if (raw20 <= raw4) return false;
  if ((raw20 - raw4) < MIN_INPUT_CAL_SPAN) return false;
  return true;
}

void resetInputCalibrationToDefaults() {
  inputRaw4mA = DEFAULT_INPUT_RAW_4MA;
  inputRaw20mA = DEFAULT_INPUT_RAW_20MA;

  prefs.putUShort("in4", inputRaw4mA);
  prefs.putUShort("in20", inputRaw20mA);

  entrada.setCalibration(inputRaw4mA, inputRaw20mA);
}

bool isOutputCalibrationValid(uint16_t raw4, uint16_t raw20) {
  if (raw4 > 4095 || raw20 > 4095) return false;
  if (raw20 <= raw4) return false;
  if ((raw20 - raw4) < MIN_OUTPUT_CAL_SPAN) return false;
  return true;
}

void resetOutputCalibrationToDefaults() {
  menuCfg.outputRaw4mA = DEFAULT_OUTPUT_RAW_4MA;
  menuCfg.outputRaw20mA = DEFAULT_OUTPUT_RAW_20MA;

  prefs.putUShort("out4", menuCfg.outputRaw4mA);
  prefs.putUShort("out20", menuCfg.outputRaw20mA);
}

void loadSettings() {
  prefs.begin("simcfg", false);

  inputRaw4mA = prefs.getUShort("in4", DEFAULT_INPUT_RAW_4MA);
  inputRaw20mA = prefs.getUShort("in20", DEFAULT_INPUT_RAW_20MA);

  if (!isInputCalibrationValid(inputRaw4mA, inputRaw20mA)) {
    resetInputCalibrationToDefaults();
  } else {
    entrada.setCalibration(inputRaw4mA, inputRaw20mA);
  }

  menuCfg.outputRaw4mA = prefs.getUShort("out4", DEFAULT_OUTPUT_RAW_4MA);
  menuCfg.outputRaw20mA = prefs.getUShort("out20", DEFAULT_OUTPUT_RAW_20MA);

  if (!isOutputCalibrationValid(menuCfg.outputRaw4mA, menuCfg.outputRaw20mA)) {
    resetOutputCalibrationToDefaults();
  }

  // El equipo arranca siempre en AUTO; el último valor manual queda disponible para tracking.
  menuCfg.manualOutputMode = false;
  prefs.remove("manual");
  menuCfg.manualOutputPct = prefs.getFloat("manPct", 0.0f);
  if (menuCfg.manualOutputPct < 0.0f) menuCfg.manualOutputPct = 0.0f;
  if (menuCfg.manualOutputPct > 100.0f) menuCfg.manualOutputPct = 100.0f;
  menuCfg.processK = prefs.getFloat("k", 1.0f);
  menuCfg.processTau = prefs.getFloat("tau", 2.0f);
  menuCfg.processDeadTime = clampFloat(prefs.getFloat("td", 0.0f), 0.0f, 30.0f);
  menuCfg.processPreset = sanitizeProcessPreset(prefs.getUChar("preset", PROCESS_PRESET_CUSTOM));
  if (processPresetIsAutomatic(menuCfg.processPreset)) {
    applyProcessPresetToMenuConfig(menuCfg.processPreset);
  }
  menuCfg.sensorNoisePct = clampFloat(prefs.getFloat("noise", 0.0f), 0.0f, 10.0f);
  const bool freezeEnabled = prefs.getBool("freeze", prefs.getUChar("fault", MenuUsuario::SENSOR_FAULT_NONE) == 3);
  menuCfg.sensorFaultMode = freezeEnabled ? MenuUsuario::SENSOR_FAULT_FROZEN : MenuUsuario::SENSOR_FAULT_NONE;
  menuCfg.sensorOffsetPct = clampFloat(prefs.getFloat("offset", 0.0f), -25.0f, 25.0f);
  menuCfg.audioEnabled = prefs.getBool("audio", true);
  menuCfg.wifiEnabled = prefs.getBool("wifi", false);
  menuCfg.wifiConnected = false;
  menuCfg.wifiRuntime = menuCfg.wifiEnabled ? MenuUsuario::WIFI_RT_CONNECTING : MenuUsuario::WIFI_RT_OFF;

  // La captura de datos arranca apagada para evitar archivos no solicitados.
  menuCfg.loggingEnabled = false;
  menuCfg.outputEnabled = false; // Estado seguro de arranque.

  // La selección de columnas del CSV se conserva entre reinicios.
  sdLogger.setFieldMask(loadCsvLogMaskFromPreferences(), false);

  nextHistoryFileIndex = prefs.getUChar("nextHist", 1);
  if (nextHistoryFileIndex < 1 || nextHistoryFileIndex > MAX_HISTORY_FILES) {
    nextHistoryFileIndex = 1;
  }
  historyIndexCacheValid = prefs.isKey("nextHist");
}

void saveSettings() {
  // El modo manual no se persiste; el arranque operativo se fuerza en AUTO.
  prefs.putFloat("manPct", menuCfg.manualOutputPct);
  prefs.putUChar("preset", static_cast<uint8_t>(menuCfg.processPreset));
  prefs.putFloat("k", menuCfg.processK);
  prefs.putFloat("tau", menuCfg.processTau);
  prefs.putFloat("td", menuCfg.processDeadTime);
  prefs.putFloat("noise", menuCfg.sensorNoisePct);
  prefs.putBool("freeze", menuCfg.sensorFaultMode == MenuUsuario::SENSOR_FAULT_FROZEN);
  prefs.remove("fault");
  prefs.putFloat("offset", menuCfg.sensorOffsetPct);
  prefs.putBool("audio", menuCfg.audioEnabled);
  prefs.putBool("wifi", menuCfg.wifiEnabled);
}

bool saveInputCalibration() {
  if (!isInputCalibrationValid(inputRaw4mA, inputRaw20mA)) {
    entrada.setCalibration(DEFAULT_INPUT_RAW_4MA, DEFAULT_INPUT_RAW_20MA);
    return false;
  }

  prefs.putUShort("in4", inputRaw4mA);
  prefs.putUShort("in20", inputRaw20mA);
  entrada.setCalibration(inputRaw4mA, inputRaw20mA);

  return true;
}

void applyOutputCalibrationToDac() {
  if (!outputReady) return;
  salida.setCalibrationRaw(menuCfg.outputRaw4mA, menuCfg.outputRaw20mA);
}

bool saveOutputCalibration() {
  if (!isOutputCalibrationValid(menuCfg.outputRaw4mA, menuCfg.outputRaw20mA)) {
    resetOutputCalibrationToDefaults();
    applyOutputCalibrationToDac();
    return false;
  }

  prefs.putUShort("out4", menuCfg.outputRaw4mA);
  prefs.putUShort("out20", menuCfg.outputRaw20mA);
  applyOutputCalibrationToDac();
  return true;
}

bool holdOutputCalibrationPoint(float percent) {
  return holdOutputCalibrationPoint(percent, menuCfg.outputRaw4mA, menuCfg.outputRaw20mA);
}

bool holdOutputCalibrationPoint(float percent, uint16_t raw4, uint16_t raw20) {
  if (!outputReady) return false;
  if (!isOutputCalibrationValid(raw4, raw20)) return false;

  outputCalibrationHoldPct = clampFloat(percent, 0.0f, 100.0f);
  outputCalibrationHoldRaw4mA = raw4;
  outputCalibrationHoldRaw20mA = raw20;
  outputCalibrationHoldActive = true;
  outputCalibrationHoldUntilMs = millis() + 3000;

  salida.setCalibrationRaw(outputCalibrationHoldRaw4mA, outputCalibrationHoldRaw20mA);
  salida.setOutputPercentDirect(outputCalibrationHoldPct);
  return true;
}

void cancelOutputCalibrationHold() {
  if (!outputCalibrationHoldActive) return;

  outputCalibrationHoldActive = false;
  applyOutputCalibrationToDac();

  if (!menuCfg.outputEnabled && outputReady) {
    salida.setOutputOff();
  }
}

// ============================================================
// UTILIDADES
// ============================================================
static float clampFloat(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}


MenuUsuario::SensorFaultMode clampMenuSensorFaultMode(uint8_t raw) {
  // Acepta el valor persistido 3 como congelamiento para leer configuraciones guardadas.
  if (raw == MenuUsuario::SENSOR_FAULT_FROZEN || raw == 3) {
    return MenuUsuario::SENSOR_FAULT_FROZEN;
  }
  return MenuUsuario::SENSOR_FAULT_NONE;
}

Generar_4_20mALib::SensorFaultMode mapSensorFaultToOutput(MenuUsuario::SensorFaultMode mode) {
  return (mode == MenuUsuario::SENSOR_FAULT_FROZEN)
           ? Generar_4_20mALib::SENSOR_FAULT_FROZEN
           : Generar_4_20mALib::SENSOR_FAULT_NONE;
}

void appendUniqueFaultTag(char* out, size_t outSize, char tag) {
  if (out == nullptr || outSize < 2) return;

  size_t len = strlen(out);
  for (size_t i = 0; i < len; i++) {
    if (out[i] == tag) return;
  }

  if (len < outSize - 1) {
    out[len] = tag;
    out[len + 1] = '\0';
  }
}

void buildDisplayFaultTags(char* out, size_t outSize) {
  if (out == nullptr || outSize == 0) return;
  out[0] = '\0';

  // O = Offset aplicado a la medición del modelo.
  if (menuCfg.sensorOffsetPct > 0.05f || menuCfg.sensorOffsetPct < -0.05f) {
    appendUniqueFaultTag(out, outSize, 'O');
  }

  // C = Congelamiento de la señal.
  // La etiqueta C depende solo del congelamiento configurado; offset y ruido tienen etiquetas propias.
  if (menuCfg.sensorFaultMode == MenuUsuario::SENSOR_FAULT_FROZEN) {
    appendUniqueFaultTag(out, outSize, 'C');
  }

  // R = Ruido configurado sobre la señal.
  if (menuCfg.sensorNoisePct > 0.05f) {
    appendUniqueFaultTag(out, outSize, 'R');
  }
}

void appendCsvErrorItem(char* out, size_t outSize, const char* item) {
  if (!out || outSize == 0 || !item || !item[0]) return;

  const size_t len = strlen(out);
  if (len >= outSize - 1) return;

  const char* separator = (len > 0) ? "; " : "";
  snprintf(out + len, outSize - len, "%s%s", separator, item);
}

void buildCsvErrorText(char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';

  if (entrada.underrange()) {
    appendCsvErrorItem(out, outSize, "Entrada bajo rango");
  }

  if (entrada.overrange()) {
    appendCsvErrorItem(out, outSize, "Entrada sobre rango");
  }

  if (menuCfg.sensorFaultMode == MenuUsuario::SENSOR_FAULT_FROZEN) {
    appendCsvErrorItem(out, outSize, "Congelamiento");
  }

  if (menuCfg.sensorNoisePct > 0.05f) {
    appendCsvErrorItem(out, outSize, "Ruido");
  }

  if (menuCfg.sensorOffsetPct > 0.05f || menuCfg.sensorOffsetPct < -0.05f) {
    appendCsvErrorItem(out, outSize, "Offset");
  }

  if (!out[0]) {
    snprintf(out, outSize, "Sin errores");
  }
}

bool outputIsFromCalibration() {
  if (!outputCalibrationHoldActive) return false;
  if (menu.isEdit()) return true;
  return (int32_t)(millis() - outputCalibrationHoldUntilMs) < 0;
}

void applyProcessPresetToMenuConfig(ProcessPreset preset) {
  menuCfg.processPreset = sanitizeProcessPreset(static_cast<uint8_t>(preset));
  if (!processPresetIsAutomatic(menuCfg.processPreset)) {
    return;
  }

  const ProcessPresetDefinition& def = processPresetDefinition(menuCfg.processPreset);
  menuCfg.processK = def.K;
  menuCfg.processTau = def.tau_s;
  menuCfg.processDeadTime = def.deadTime_s;
}

void applyOutputModelConfig() {
  if (!outputReady) return;

  salida.setTransferFunction(menuCfg.processK, menuCfg.processTau, 50);
  salida.setDeadTime(menuCfg.processDeadTime);
  salida.setOutputNoise(menuCfg.sensorNoisePct);
  salida.setSensorOffset(menuCfg.sensorOffsetPct);
  salida.setSensorFaultMode(mapSensorFaultToOutput(menuCfg.sensorFaultMode));
}

bool powerButtonClicked() {
  const bool currentRaw = (digitalRead(PIN_PWR_BTN) == HIGH);
  const uint32_t now = millis();

  if (currentRaw != pwrRaw) {
    pwrRaw = currentRaw;
    pwrLastChangeMs = now;
  }

  if ((uint32_t)(now - pwrLastChangeMs) < 40) {
    return false;
  }

  if (pwrStable != pwrRaw) {
    const bool previous = pwrStable;
    pwrStable = pwrRaw;

    // Click al soltar para evitar eventos repetidos por pulsación mantenida.
    if (previous == true && pwrStable == false) {
      // Si el ESP32 acaba de despertar por este mismo pulsador, la primera
      // liberación pertenece al gesto de encendido y no debe apagarlo otra vez.
      if (ignoreNextPwrRelease) {
        ignoreNextPwrRelease = false;
        return false;
      }
      return true;
    }
  }

  return false;
}

void runBuzzerFor(uint32_t durationMs) {
  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < durationMs) {
    buzzer.update();
    delay(1);
  }
}

void serviceSplashUntil(uint32_t startMs, uint32_t minimumMs) {
  while ((uint32_t)(millis() - startMs) < minimumMs) {
    // Mantiene vivos el buzzer y la animación sin introducir un delay largo.
    buzzer.update();
    if (displayReady) {
      display.update();
    }
    delay(2);
  }
}

void enterDeepSleep() {
#ifdef ESP32
  // GPIO15 es el pulsador físico de encendido. El apagado se ejecuta al
  // soltarlo, por lo que aquí el pin ya está LOW. El próximo nivel HIGH
  // despierta al ESP32-S3 desde Deep-sleep.
  esp_err_t err =
      esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PIN_PWR_BTN), 1);

  if (err != ESP_OK) {
    Serial.print("ERROR wake-up GPIO15: ");
    Serial.println((int)err);
    return;
  }

  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
#else
  peripheralsOn = false;
#endif
}

void configureOutputDefaults() {
  outputCfg.wire = &Wire;
  outputCfg.i2cAddr = 0x60;
  outputCfg.sdaPin = PIN_I2C_SDA;
  outputCfg.sclPin = PIN_I2C_SCL;
  outputCfg.i2cClock = 400000;
  outputCfg.rawAtOutputMin = menuCfg.outputRaw4mA;
  outputCfg.rawAtOutputMax = menuCfg.outputRaw20mA;
  outputCfg.K = menuCfg.processK;
  outputCfg.tau_s = menuCfg.processTau;
  outputCfg.sampleTimeMs = 50;
  outputCfg.deadTime_s = menuCfg.processDeadTime;
  outputCfg.outputNoisePct = menuCfg.sensorNoisePct;
  outputCfg.sensorOffsetPct = menuCfg.sensorOffsetPct;
  outputCfg.sensorFaultMode = mapSensorFaultToOutput(menuCfg.sensorFaultMode);
  outputCfg.sensorDisconnectmA = 0.0f;
  outputCfg.y0Pct = 0.0f;
}

// ============================================================
// WIFI / INTERFAZ WEB
// ============================================================
MenuUsuario::WiFiRuntime currentWiFiRuntime() {
  if (!menuCfg.wifiEnabled || !wifiActive || !wifiInterface.isActive()) {
    return MenuUsuario::WIFI_RT_OFF;
  }

  if (wifiInterface.isStationConnected()) {
    return MenuUsuario::WIFI_RT_STA;
  }

  if (wifiInterface.isAccessPointMode()) {
    return MenuUsuario::WIFI_RT_AP;
  }

  if (wifiInterface.isConnecting()) {
    return MenuUsuario::WIFI_RT_CONNECTING;
  }

  return MenuUsuario::WIFI_RT_OFF;
}

Display::WiFiStatus currentDisplayWiFiStatus() {
  if (!menuCfg.wifiEnabled || !wifiActive) {
    return Display::DISPLAY_WIFI_OFF;
  }

  return wifiInterface.isStationConnected() ? Display::DISPLAY_WIFI_CONNECTED : Display::DISPLAY_WIFI_ON;
}

void syncWiFiRuntimeToMenu() {
  menuCfg.wifiRuntime = currentWiFiRuntime();
  menuCfg.wifiConnected = wifiInterface.isStationConnected();
}

void buildWiFiInfoMessage(char* out, size_t outSize) {
  if (!out || outSize == 0) return;

  if (!menuCfg.wifiEnabled || !wifiActive) {
    snprintf(out, outSize, "WiFi NO");
    return;
  }

  snprintf(out,
           outSize,
           "%.21s\n%.21s\n%.21s",
           wifiInterface.getModeString().c_str(),
           wifiInterface.getSSIDString().c_str(),
           wifiInterface.getIpString().c_str());
}

bool startWiFiInterface() {
  if (!menuCfg.wifiEnabled) {
    syncWiFiRuntimeToMenu();
    return false;
  }

  if (!wifiActive) {
    wifiInterface.setDeviceName("Simulador de Variables de Proceso para Lazos de Control Industrial");
    wifiInterface.begin(WIFI_HOSTNAME,
                        WIFI_AP_SSID,
                        WIFI_AP_PASS,
                        WIFI_CONNECT_TIMEOUT_MS);
    sdLogger.setFieldMask(wifiInterface.getCsvLogMask(), false);
    wifiActive = true;
  }

  syncWiFiRuntimeToMenu();
  return wifiInterface.isActive();
}

void stopWiFiInterface() {
  wifiInterface.end();
  wifiActive = false;
  menuCfg.wifiConnected = false;
  menuCfg.wifiRuntime = MenuUsuario::WIFI_RT_OFF;
}

void updateWiFiInterface() {
  if (!menuCfg.wifiEnabled) {
    if (wifiActive) stopWiFiInterface();
    return;
  }

  if (!wifiActive) {
    startWiFiInterface();
  }

  wifiInterface.handle();
  syncWiFiRuntimeToMenu();
  handleWiFiProcessConfigRequests();
  handleWiFiLogConfigRequests();
  handleWiFiUserConfigRequests();
  handleWiFiUserActionRequests();

  const uint32_t now = millis();
  if ((uint32_t)(now - lastWiFiSyncMs) < WIFI_STATUS_SYNC_MS) {
    return;
  }
  lastWiFiSyncMs = now;

  String state = menuCfg.outputEnabled ? "Salida activa" : "Salida inactiva";
  if (menuCfg.loggingEnabled) state += " / Captura activa";

  wifiInterface.setProjectState(state);
  wifiInterface.setProcessPercent(menuCfg.outputEnabled,
                                  menuCfg.manualOutputMode ? "MANUAL" : "AUTO",
                                  outputReady ? salida.getLastOutputPct() : 0.0f,
                                  inputPct,
                                  sdReady ? "SD OK" : "SD FALLA");

  wifiInterface.setProcessDiagnostics(menuCfg.processPreset,
                                      menuCfg.processK,
                                      menuCfg.processTau,
                                      menuCfg.processDeadTime,
                                      menuCfg.sensorNoisePct,
                                      outputReady ? salida.getSensorFaultText() : "OK",
                                      menuCfg.sensorOffsetPct,
                                      outputReady ? salida.hasSensorFault() : false);

  syncUserConfigSnapshotToWiFi();
}

void handleWiFiProcessConfigRequests() {
  InterfaceWiFiProcessConfig request;
  if (!wifiInterface.consumeProcessConfigRequest(request)) {
    return;
  }

  if (processPresetIsAutomatic(request.preset)) {
    applyProcessPresetToMenuConfig(request.preset);
  } else {
    menuCfg.processPreset = PROCESS_PRESET_CUSTOM;
    menuCfg.processK = clampFloat(request.K, PROCESS_K_MIN, PROCESS_K_MAX);
    menuCfg.processTau = clampFloat(request.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    menuCfg.processDeadTime = clampFloat(request.deadTime, 0.0f, 30.0f);
  }

  saveSettings();
  applyOutputModelConfig();

  if (menuCfg.audioEnabled) {
    buzzer.ok();
  }
}

uint16_t loadCsvLogMaskFromPreferences() {
  Preferences p;
  p.begin("ifwifi", true);
  const uint16_t mask = static_cast<uint16_t>(p.getUInt("csvMask", SDLogger::LOG_FIELD_DEFAULT));
  p.end();
  return mask;
}

bool applyCsvLogMask(uint16_t mask, bool restartActiveCapture) {
  const bool wasLogging = menuCfg.loggingEnabled;

  if (wasLogging && restartActiveCapture) {
    stopDataCapture();
  }

  // La nueva máscara de columnas se aplica al próximo archivo de captura.
  const bool ok = sdLogger.setFieldMask(mask, false);

  if (wasLogging && restartActiveCapture) {
    const bool restarted = startDataCapture();
    if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);

    if (displayReady || restarted) {
      menu.showMessage("CSV ACTUALIZADO", restarted ? displayCaptureFileName() : "NO REINICIA", 1800);
    }

    if (menuCfg.audioEnabled) {
      restarted ? buzzer.ok() : buzzer.warning();
    }

    return ok && restarted;
  }

  if (menuCfg.audioEnabled) {
    ok ? buzzer.ok() : buzzer.warning();
  }

  return ok;
}

void handleWiFiLogConfigRequests() {
  InterfaceWiFiLogConfig request;
  if (!wifiInterface.consumeLogConfigRequest(request)) {
    return;
  }

  applyCsvLogMask(request.csvMask, true);
}

void syncUserConfigSnapshotToWiFi() {
  wifiInterface.setUserConfigSnapshot(menuCfg.outputEnabled,
                                      menuCfg.manualOutputMode,
                                      menuCfg.manualOutputPct,
                                      menuCfg.wifiEnabled,
                                      menuCfg.audioEnabled,
                                      sdReady,
                                      menuCfg.loggingEnabled,
                                      inputRaw4mA,
                                      inputRaw20mA,
                                      entrada.filteredRaw(),
                                      outputReady ? salida.getLastOutputRaw() : 0,
                                      menuCfg.outputRaw4mA,
                                      menuCfg.outputRaw20mA,
                                      static_cast<uint8_t>(menuCfg.sensorFaultMode));
  wifiInterface.setLastCsvPath(lastCsvPath);
}

void handleWiFiUserConfigRequests() {
  InterfaceWiFiUserConfig request;
  if (!wifiInterface.consumeUserConfigRequest(request)) {
    return;
  }

  const bool previousWifi = menuCfg.wifiEnabled;
  const bool previousLogging = menuCfg.loggingEnabled;
  const bool previousManualMode = menuCfg.manualOutputMode;

  menuCfg.outputEnabled = request.outputEnabled;
  if (!menuCfg.outputEnabled && outputReady) {
    salida.setOutputOff();
  }

  menuCfg.manualOutputMode = request.manualOutputMode;
  if (menuCfg.manualOutputMode && !previousManualMode && outputReady && menuCfg.outputEnabled) {
    // Transferencia suave AUTO -> MANUAL: la salida interna del proceso pasa a ser el primer valor manual.
    menuCfg.manualOutputPct = clampFloat(salida.getLastProcessPct(), 0.0f, 100.0f);
  } else {
    menuCfg.manualOutputPct = clampFloat(request.manualOutputPct, 0.0f, 100.0f);
  }

  if (processPresetIsAutomatic(request.preset)) {
    applyProcessPresetToMenuConfig(request.preset);
  } else {
    menuCfg.processPreset = PROCESS_PRESET_CUSTOM;
    menuCfg.processK = clampFloat(request.K, PROCESS_K_MIN, PROCESS_K_MAX);
    menuCfg.processTau = clampFloat(request.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    menuCfg.processDeadTime = clampFloat(request.deadTime, 0.0f, 30.0f);
  }

  menuCfg.sensorNoisePct = clampFloat(request.sensorNoisePct, 0.0f, 10.0f);
  menuCfg.sensorFaultMode = clampMenuSensorFaultMode(request.sensorFaultMode);
  menuCfg.sensorOffsetPct = clampFloat(request.sensorOffsetPct, -25.0f, 25.0f);

  menuCfg.audioEnabled = request.audioEnabled;
  buzzer.setEnabled(menuCfg.audioEnabled);
  if (displayReady) display.setBuzzerEnabled(menuCfg.audioEnabled);

  if (request.outputCalibrationUpdate &&
      isOutputCalibrationValid(request.outputRaw4mA, request.outputRaw20mA)) {
    menuCfg.outputRaw4mA = request.outputRaw4mA;
    menuCfg.outputRaw20mA = request.outputRaw20mA;
    saveOutputCalibration();
  }

  saveSettings();
  applyOutputModelConfig();
  applyOutputCalibrationToDac();

  if (outputReady) {
    if (!menuCfg.outputEnabled) {
      salida.setOutputOff();
    } else if (menuCfg.manualOutputMode) {
      salida.setOutputPercent(menuCfg.manualOutputPct);
    }
  }

  menuCfg.wifiEnabled = request.wifiEnabled;
  if (menuCfg.wifiEnabled != previousWifi) {
    saveSettings();
    if (menuCfg.wifiEnabled) {
      startWiFiInterface();
    } else {
      stopWiFiInterface();
      return;
    }
  }

  if (request.loggingEnabled != previousLogging) {
    if (request.loggingEnabled) {
      menuCfg.loggingEnabled = true;
      startDataCapture();
    } else {
      stopDataCapture();
    }
    if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);
  }

  syncUserConfigSnapshotToWiFi();
  if (menuCfg.audioEnabled) buzzer.ok();
}

void handleWiFiUserActionRequests() {
  InterfaceWiFiUserActionRequest request;
  if (!wifiInterface.consumeUserActionRequest(request)) {
    return;
  }

  switch (request.action) {
    case IFW_ACTION_WIFI_RESET:
      wifiInterface.clearCredentials();
      if (menuCfg.wifiEnabled) {
        stopWiFiInterface();
        startWiFiInterface();
      }
      saveSettings();
      if (displayReady) menu.showMessage("WIFI", "Credenciales\nborradas", 1800);
      if (menuCfg.audioEnabled) buzzer.warning();
      break;

    case IFW_ACTION_SD_DETECT: {
      const bool ok = detectSdCard();
      if (displayReady) {
        display.setCaptureActive(menuCfg.loggingEnabled);
        menu.showMessage("DETECTAR SD", ok ? "SD OK" : "SD FALLA", 1400);
      }
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case IFW_ACTION_SAVE_DATA:
      if (menuCfg.loggingEnabled) {
        stopDataCapture();
        if (displayReady) {
          display.setCaptureActive(menuCfg.loggingEnabled);
          char msg[44];
          snprintf(msg, sizeof(msg), "Datos guardados en:\n%s", displayCaptureFileName());
          menu.showMessage("CAPTURA DETENIDA", msg, 4000);
        }
        if (menuCfg.audioEnabled) buzzer.ok();
      } else {
        if (displayReady) menu.showMessage("CAPTURA", "NO ACTIVA", 1200);
        if (menuCfg.audioEnabled) buzzer.warning();
      }
      break;

    case IFW_ACTION_CAL_INPUT_4: {
      inputRaw4mA = entrada.captureRaw(128);
      const bool ok = saveInputCalibration();
      if (displayReady) {
        char msg[44];
        snprintf(msg, sizeof(msg), ok ? "RAW 4mA=%u" : "CAL INVALIDA 4=%u", inputRaw4mA);
        menu.showMessage("CAL ENTRADA", msg, 1600);
      }
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case IFW_ACTION_CAL_INPUT_20: {
      inputRaw20mA = entrada.captureRaw(128);
      const bool ok = saveInputCalibration();
      if (displayReady) {
        char msg[44];
        snprintf(msg, sizeof(msg), ok ? "RAW 20mA=%u" : "CAL INVALIDA 20=%u", inputRaw20mA);
        menu.showMessage("CAL ENTRADA", msg, 1600);
      }
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case IFW_ACTION_TEST_OUTPUT_4: {
      const bool ok = holdOutputCalibrationPoint(0.0f, request.outputRaw4mA, request.outputRaw20mA);
      if (displayReady) menu.showMessage("CAL SALIDA", ok ? "Forzando 4 mA" : "CAL INVALIDA", 1400);
      if (menuCfg.audioEnabled) ok ? buzzer.click() : buzzer.warning();
      break;
    }

    case IFW_ACTION_TEST_OUTPUT_20: {
      const bool ok = holdOutputCalibrationPoint(100.0f, request.outputRaw4mA, request.outputRaw20mA);
      if (displayReady) menu.showMessage("CAL SALIDA", ok ? "Forzando 20 mA" : "CAL INVALIDA", 1400);
      if (menuCfg.audioEnabled) ok ? buzzer.click() : buzzer.warning();
      break;
    }

    case IFW_ACTION_SPECIAL:
      if (displayReady) {
        display.setSpecialScreen(SPECIAL_SCREEN_MS);
        display.forceRefresh();
      }
      if (menuCfg.audioEnabled) {
        buzzer.special();
      }
      break;

    case IFW_ACTION_NONE:
    default:
      break;
  }

  syncUserConfigSnapshotToWiFi();
}

// ============================================================
// SD / CAPTURA DE DATOS
// ============================================================
bool mountSdCard() {
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  SPI.begin(PIN_SD_CLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

  if (!SD.begin(PIN_SD_CS, SPI, SD_SPI_FREQUENCY_HZ)) {
    SD.end();
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    SD.end();
    return false;
  }

  return true;
}

bool probeMountedSdCard() {
  // Chequeo liviano de presencia: no remonta ni escribe archivos.
  if (!sdReady) {
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    return false;
  }

  File root = SD.open("/");
  if (!root) {
    return false;
  }

  root.close();
  return true;
}

void markSdUnavailable() {
  if (sdLogger.isReady()) {
    sdLogger.end();
  }

  SD.end();
  SPI.end();

  sdReady = false;
  menuCfg.sdReady = false;
  refreshLastCsvFromSd();
}

bool detectSdCard() {
  // Permite detectar la tarjeta aunque el equipo haya arrancado sin SD.
  // Con captura activa se evita reiniciar el bus; solo se realiza un probe liviano.
  if (menuCfg.loggingEnabled) {
    const bool stillPresent = probeMountedSdCard();
    if (!stillPresent) {
      menuCfg.loggingEnabled = false;
      markSdUnavailable();
    }
    return stillPresent;
  }

  if (sdLogger.isReady()) {
    sdLogger.end();
  }

  SD.end();
  SPI.end();
  delay(60);

  sdReady = mountSdCard();
  menuCfg.sdReady = sdReady;
  refreshLastCsvFromSd();
  return sdReady;
}

void updateSdStatusPeriodic() {
  if (!peripheralsOn) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastSdStatusCheckMs) < SD_STATUS_CHECK_PERIOD_MS) {
    return;
  }
  lastSdStatusCheckMs = now;

  // Durante captura, la escritura periódica ya es el mejor detector real.
  // No se remonta el bus mientras podría haber una operación de archivo en curso.
  if (menuCfg.loggingEnabled) {
    if (!probeMountedSdCard()) {
      menuCfg.loggingEnabled = false;
      markSdUnavailable();
      if (displayReady) display.setCaptureActive(false);
      menu.showMessage("CAPTURA", "SD DESCONECTADA", 1400);
      if (menuCfg.audioEnabled) buzzer.warning();
    }
    return;
  }

  if (sdReady) {
    if (!probeMountedSdCard()) {
      markSdUnavailable();
    }
    return;
  }

  // Si la SD no estaba disponible, se evita remontar automáticamente para no bloquear la interfaz.
}

bool parseHistoryDataIndex(const char* path, uint8_t& indexOut) {
  if (!path || !path[0]) return false;

  // Acepta tanto "/HistoryData_03.csv" como "HistoryData_03.csv".
  const char* name = path;
  if (name[0] == '/') name++;

  static constexpr const char* PREFIX = "HistoryData_";
  static constexpr const char* SUFFIX = ".csv";
  const size_t prefixLen = strlen(PREFIX);
  const size_t nameLen = strlen(name);
  const size_t suffixLen = strlen(SUFFIX);

  if (nameLen != prefixLen + 2 + suffixLen) return false;
  if (strncmp(name, PREFIX, prefixLen) != 0) return false;
  if (strcmp(name + prefixLen + 2, SUFFIX) != 0) return false;
  if (name[prefixLen] < '0' || name[prefixLen] > '9') return false;
  if (name[prefixLen + 1] < '0' || name[prefixLen + 1] > '9') return false;

  const uint8_t index = (name[prefixLen] - '0') * 10 + (name[prefixLen + 1] - '0');
  if (index < 1 || index > MAX_HISTORY_FILES) return false;

  indexOut = index;
  return true;
}

void clearRuntimeLastCsvPath() {
  lastCsvPath[0] = '\0';
  wifiInterface.setLastCsvPath("");
}

void rememberLastCsvPath(const char* path, bool persist) {
  if (!path || !path[0]) {
    clearRuntimeLastCsvPath();
    if (persist) {
      prefs.remove("lastCsv");
      prefs.remove("nextHist");
      historyIndexCacheValid = false;
      nextHistoryFileIndex = 1;
    }
    return;
  }

  snprintf(lastCsvPath, sizeof(lastCsvPath), "%s", path);
  wifiInterface.setLastCsvPath(lastCsvPath);

  uint8_t index = 0;
  if (parseHistoryDataIndex(lastCsvPath, index)) {
    nextHistoryFileIndex = (index < MAX_HISTORY_FILES) ? (index + 1) : 1;
    historyIndexCacheValid = true;
  }

  if (persist) {
    prefs.putString("lastCsv", lastCsvPath);
    if (historyIndexCacheValid) {
      prefs.putUChar("nextHist", nextHistoryFileIndex);
    }
  }
}

bool buildNextCapturePath(char* out, size_t outSize) {
  if (!out || outSize == 0) return false;

  uint8_t startIndex = historyIndexCacheValid ? nextHistoryFileIndex : prefs.getUChar("nextHist", 1);
  if (startIndex < 1 || startIndex > MAX_HISTORY_FILES) {
    startIndex = 1;
  }

  // Primera pasada: desde el próximo índice esperado hasta 99.
  // Caso normal: una sola consulta si el índice guardado sigue siendo válido.
  for (uint8_t index = startIndex; index <= MAX_HISTORY_FILES; index++) {
    snprintf(out, outSize, "/HistoryData_%02u.csv", index);
    if (!SD.exists(out)) {
      nextHistoryFileIndex = index;
      historyIndexCacheValid = true;
      return true;
    }
  }

  // Segunda pasada: wrap-around por si se borraron archivos bajos.
  for (uint8_t index = 1; index < startIndex; index++) {
    snprintf(out, outSize, "/HistoryData_%02u.csv", index);
    if (!SD.exists(out)) {
      nextHistoryFileIndex = index;
      historyIndexCacheValid = true;
      return true;
    }
  }

  out[0] = '\0';
  return false;
}

const char* displayCaptureFileName() {
  if (capturePath[0] == '/') {
    return capturePath + 1;
  }
  return capturePath;
}

bool findLatestCapturePath(char* out, size_t outSize) {
  if (!out || outSize == 0) return false;
  out[0] = '\0';

  if (!sdReady) {
    return false;
  }

  // El último CSV guardado en NVS evita recorrer la FAT en cada reinicio.
  String savedPath = prefs.getString("lastCsv", "");
  if (savedPath.length() > 0 && savedPath.length() < outSize && SD.exists(savedPath.c_str())) {
    snprintf(out, outSize, "%s", savedPath.c_str());

    uint8_t savedIndex = 0;
    if (parseHistoryDataIndex(out, savedIndex)) {
      nextHistoryFileIndex = (savedIndex < MAX_HISTORY_FILES) ? (savedIndex + 1) : 1;
      historyIndexCacheValid = true;
      prefs.putUChar("nextHist", nextHistoryFileIndex);
    }
    return true;
  }

  // Si no hay historial conocido, se evita repetir el barrido completo con una SD vacía.
  if (savedPath.length() == 0 && historyIndexCacheValid) {
    return false;
  }

  // Respaldo para tarjetas con archivos previos al índice guardado en memoria.
  char candidate[32];
  for (int index = MAX_HISTORY_FILES; index >= 1; index--) {
    snprintf(candidate, sizeof(candidate), "/HistoryData_%02u.csv", index);
    if (SD.exists(candidate)) {
      snprintf(out, outSize, "%s", candidate);
      nextHistoryFileIndex = (index < MAX_HISTORY_FILES) ? (index + 1) : 1;
      historyIndexCacheValid = true;
      prefs.putString("lastCsv", out);
      prefs.putUChar("nextHist", nextHistoryFileIndex);
      return true;
    }
  }

  // Se guarda la ausencia de historial para evitar barridos completos en SD vacía.
  nextHistoryFileIndex = 1;
  historyIndexCacheValid = true;
  prefs.remove("lastCsv");
  prefs.putUChar("nextHist", nextHistoryFileIndex);
  return false;
}

void refreshLastCsvFromSd() {
  if (!sdReady) {
    clearRuntimeLastCsvPath();
    return;
  }

  if (capturePath[0] != '\0') {
    rememberLastCsvPath(capturePath, false);
    return;
  }

  char latest[32];
  if (findLatestCapturePath(latest, sizeof(latest))) {
    rememberLastCsvPath(latest, false);
  } else {
    clearRuntimeLastCsvPath();
  }
}

bool startDataCapture() {
  // Se separa la solicitud de captura del estado real para validar la SD antes de iniciar.
  menuCfg.loggingEnabled = false;

  if (!detectSdCard()) {
    menuCfg.loggingEnabled = false;
    return false;
  }

  if (!buildNextCapturePath(capturePath, sizeof(capturePath))) {
    menuCfg.loggingEnabled = false;
    return false;
  }

  if (!sdLogger.begin(PIN_SD_CLK,
                      PIN_SD_MISO,
                      PIN_SD_MOSI,
                      PIN_SD_CS,
                      capturePath,
                      SD_SPI_FREQUENCY_HZ)) {
    menuCfg.loggingEnabled = false;
    capturePath[0] = '\0';
    return false;
  }

  rememberLastCsvPath(capturePath, true);

  captureStartMs = millis();
  lastLogMs = 0;
  menuCfg.loggingEnabled = true;
  return true;
}

bool captureOneSample() {
  if (!sdReady || !menuCfg.loggingEnabled || !sdLogger.isReady()) {
    return false;
  }

  const uint32_t now = millis();
  const uint32_t elapsedMs = now - captureStartMs;

  char errorText[96];
  buildCsvErrorText(errorText, sizeof(errorText));

  return sdLogger.logSample(elapsedMs,
                            inputmA,
                            inputPct,
                            outputReady ? salida.getLastOutputmA() : 4.0f,
                            outputReady ? salida.getLastOutputPct() : 0.0f,
                            menuCfg.outputEnabled,
                            menuCfg.manualOutputMode,
                            processPresetName(menuCfg.processPreset),
                            menuCfg.processK,
                            menuCfg.processTau,
                            menuCfg.processDeadTime,
                            menuCfg.sensorNoisePct,
                            menuCfg.sensorOffsetPct,
                            errorText,
                            outputIsFromCalibration());
}

void stopDataCapture() {
  if (!menuCfg.loggingEnabled) {
    return;
  }

  // Última muestra antes de cerrar la captura.
  captureOneSample();
  menuCfg.loggingEnabled = false;

  // SD.end() no se invoca: cada muestra abre/cierra archivo y la SD queda montada.
  if (capturePath[0] != '\0') {
    rememberLastCsvPath(capturePath, true);
  }
}

// ============================================================
// INICIALIZACIÓN / APAGADO DE PERIFÉRICOS
// ============================================================
void initPeripherals() {
  peripheralsOn = true;
  delay(80);

  // El OLED arranca primero: desde este momento acompaña visualmente toda la
  // puesta en servicio del equipo.
  displayReady = display.begin(PIN_I2C_SDA, PIN_I2C_SCL, 0x3C);
  const uint32_t splashStartMs = millis();

  if (displayReady) {
    display.setStartupSplash(PROJECT_VERSION, STARTUP_SPLASH_MIN_MS);
    display.forceRefresh();
  }

  entrada.begin();
  if (displayReady) display.update();

  configureOutputDefaults();
  salida = Generar_4_20mALib(outputCfg);
  outputReady = salida.begin();

  if (outputReady) {
    applyOutputModelConfig();
    salida.setOutputOff(); // RAW=0: estado seguro con salida desactivada
  }

  if (displayReady) display.update();

  sdReady = mountSdCard();
  menuCfg.sdReady = sdReady;
  menuCfg.loggingEnabled = false;
  lastSdStatusCheckMs = millis();
  refreshLastCsvFromSd();

  if (displayReady) display.update();

  if (menuCfg.wifiEnabled) {
    startWiFiInterface();
  } else {
    stopWiFiInterface();
  }

  if (displayReady) display.update();

  if (menuCfg.audioEnabled) {
    buzzer.startup();
  }

  // El splash dura como mínimo 2,6 s. Durante ese tiempo se anima
  // exclusivamente el logotipo SIMULADOR y aparecen 4-20mA / v1.5 - IVANI.
  // Si la inicialización tarda más, permanece visible hasta finalizar.
  serviceSplashUntil(splashStartMs, STARTUP_SPLASH_MIN_MS);

  // Tras el splash se informa únicamente una anomalía real. Con DAC y SD OK
  // se pasa directamente a la pantalla principal.
  if (!outputReady && !sdReady) {
    menu.showMessage("ALERTA", "DAC Y SD FALLAN", 1800);
  } else if (!outputReady) {
    menu.showMessage("ALERTA", "DAC NO DETECTADO", 1800);
  } else if (!sdReady) {
    menu.showMessage("ALERTA", "SD NO DETECTADA", 1800);
  } else {
    menu.showMain();
  }

  lastDisplaySyncMs = 0;
  syncDisplay();

  if (displayReady) {
    display.forceRefresh();
  }
}

void shutdownPeripherals() {
  // Se muestra inmediatamente el splash de apagado antes de cerrar servicios.
  const uint32_t splashStartMs = millis();

  if (displayReady) {
    display.setShutdownSplash(PROJECT_VERSION, SHUTDOWN_SPLASH_MIN_MS);
    display.forceRefresh();
  }

  stopDataCapture();
  stopWiFiInterface();

  menuCfg.outputEnabled = false;
  saveSettings();

  if (outputReady) {
    salida.setOutputOff(); // OFF físico del canal analógico: DAC RAW=0
  }

  if (sdReady) {
    SD.end();
    SPI.end();
  }

  if (menuCfg.audioEnabled) {
    buzzer.shutdown();
  } else {
    buzzer.stop();
  }

  // La animación de cierre permanece visible al menos 2,2 s.
  serviceSplashUntil(splashStartMs, SHUTDOWN_SPLASH_MIN_MS);
  buzzer.stop();

  if (displayReady) {
    display.sleep();
  }

  peripheralsOn = false;
  displayReady = false;
  outputReady = false;
  sdReady = false;
  menuCfg.sdReady = false;
  lastSdStatusCheckMs = 0;

  // Desde aquí el ESP32 deja de ejecutar el loop. Solo GPIO15 queda como
  // fuente de wake-up; al pulsarlo nuevamente se reinicia desde setup().
  enterDeepSleep();

  // Fallback por si la configuración de wake-up fallara.
  peripheralsOn = true;
}

void togglePeripheralsPower() {
  if (peripheralsOn) {
    shutdownPeripherals();
  } else {
    initPeripherals();
    menu.showMain();
  }
}

// ============================================================
// CONTROL / MENÚ / DISPLAY / LOGGING
// ============================================================
void updateInput() {
  if (!peripheralsOn) return;

  if (entrada.update()) {
    const float measuredmA = entrada.currentmA();
    const float measuredPct = entrada.percent();

    inputmA = isnan(measuredmA) ? 4.0f : measuredmA;
    inputPct = isnan(measuredPct) ? 0.0f : measuredPct;

    if (DEBUG_INPUT_ADC) {
      static uint32_t lastDebugMs = 0;
      const uint32_t now = millis();

      if ((uint32_t)(now - lastDebugMs) >= DEBUG_INPUT_PERIOD_MS) {
        lastDebugMs = now;

        Serial.print("ADC GPIO");
        Serial.print(PIN_AO_PLC_ADC);
        Serial.print(" RAW=");
        Serial.print(entrada.raw());
        Serial.print(" FILT=");
        Serial.print(entrada.filteredRaw());
        Serial.print(" mA=");
        Serial.print(inputmA, 2);
        Serial.print(" IN=");
        Serial.print(inputPct, 1);
        Serial.print(" Cal4=");
        Serial.print(inputRaw4mA);
        Serial.print(" Cal20=");
        Serial.print(inputRaw20mA);
        Serial.print(" CalOK=");
        Serial.println(isInputCalibrationValid(inputRaw4mA, inputRaw20mA) ? "SI" : "NO");
      }
    }
  }
}

void updateControlOutput() {
  if (!peripheralsOn || !outputReady) return;

  if (outputCalibrationHoldActive) {
    const bool holdFromMenuEdit = menu.isEdit();
    const bool holdFromWebAction = (int32_t)(millis() - outputCalibrationHoldUntilMs) < 0;

    if (holdFromMenuEdit || holdFromWebAction) {
      salida.setCalibrationRaw(outputCalibrationHoldRaw4mA, outputCalibrationHoldRaw20mA);
      salida.setOutputPercentDirect(outputCalibrationHoldPct);
      return;
    }

    outputCalibrationHoldActive = false;
    applyOutputCalibrationToDac();
  }

  if (!menuCfg.outputEnabled) {
    // OFF real: 0 cuentas. No aplicar ruido, offset, congelamiento ni calibración 4-20 mA.
    // Se escribe solo al entrar en OFF para evitar tráfico I2C innecesario.
    if (salida.getLastOutputRaw() != 0) {
      salida.setOutputOff();
    }
    return;
  }

  if (menuCfg.manualOutputMode) {
    // En MANUAL se limita la escritura al DAC a una actualización cada 50 ms.
    // El modo automático NO usa este temporizador externo: Generar_4_20mA
    // ya controla internamente su sampleTimeMs.
    const uint32_t now = millis();
    if ((uint32_t)(now - lastControlMs) < 50) return;
    lastControlMs = now;

    menuCfg.manualOutputPct = clampFloat(menuCfg.manualOutputPct, 0.0f, 100.0f);
    salida.setOutputPercent(menuCfg.manualOutputPct);
  } else {
    // En AUTO, la temporización queda exclusivamente a cargo de updateFromInputmA().
    // Esto evita aplicar dos temporizadores independientes de 50 ms sobre el mismo modelo.
    if (entrada.hasValidCalibration()) {
      salida.updateFromInputmA(inputmA);
    } else {
      salida.setOutputPercent(0.0f);
    }

    // Tracking para una transferencia sin salto al pasar posteriormente a MANUAL.
    menuCfg.manualOutputPct = clampFloat(salida.getLastProcessPct(), 0.0f, 100.0f);
  }
}


void scheduleManualOutputSave() {
  pendingManualOutputSave = true;
  lastManualOutputTrimMs = millis();
}

void commitPendingManualOutputSave() {
  if (!pendingManualOutputSave) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastManualOutputTrimMs) < MANUAL_TRIM_SAVE_DELAY_MS) {
    return;
  }

  pendingManualOutputSave = false;
  saveSettings();
}

void handleMenuEvent(MenuUsuario::Event ev) {
  if (ev == MenuUsuario::EV_NONE) return;

  switch (ev) {
    case MenuUsuario::EV_OUTPUT_ENABLE_CHANGED:
      if (!menuCfg.outputEnabled && outputReady) {
        salida.setOutputOff();
      }
      saveSettings();
      if (menuCfg.audioEnabled) {
        menuCfg.outputEnabled ? buzzer.ok() : buzzer.shutdown();
      }
      break;

    case MenuUsuario::EV_PRESET_CHANGED:
      applyProcessPresetToMenuConfig(menuCfg.processPreset);
      saveSettings();
      applyOutputModelConfig();
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_OUTPUT_MODE_CHANGED:
      // Transferencia bumpless: al pasar a MANUAL, el valor manual toma la salida interna del proceso.
      if (outputReady && menuCfg.outputEnabled && menuCfg.manualOutputMode) {
        menuCfg.manualOutputPct = clampFloat(salida.getLastProcessPct(), 0.0f, 100.0f);
        salida.setOutputPercent(menuCfg.manualOutputPct);
      }
      saveSettings();
      if (menuCfg.audioEnabled) buzzer.click();
      break;

    case MenuUsuario::EV_MANUAL_OUTPUT_CHANGED:
      menuCfg.manualOutputPct = clampFloat(menuCfg.manualOutputPct, 0.0f, 100.0f);
      saveSettings();
      if (outputReady && menuCfg.outputEnabled && menuCfg.manualOutputMode) {
        salida.setOutputPercent(menuCfg.manualOutputPct);
      }
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_MANUAL_OUTPUT_TRIMMED:
      menuCfg.manualOutputPct = clampFloat(menuCfg.manualOutputPct, 0.0f, 100.0f);
      if (outputReady && menuCfg.outputEnabled && menuCfg.manualOutputMode) {
        salida.setOutputPercent(menuCfg.manualOutputPct);
      }
      scheduleManualOutputSave();
      break;

    case MenuUsuario::EV_K_CHANGED:
    case MenuUsuario::EV_TAU_CHANGED:
      saveSettings();
      applyOutputModelConfig();
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_DEADTIME_CHANGED:
      menuCfg.processDeadTime = clampFloat(menuCfg.processDeadTime, 0.0f, 30.0f);
      saveSettings();
      if (outputReady) {
        salida.setDeadTime(menuCfg.processDeadTime);
      }
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_NOISE_CHANGED:
      menuCfg.sensorNoisePct = clampFloat(menuCfg.sensorNoisePct, 0.0f, 10.0f);
      saveSettings();
      if (outputReady) {
        salida.setOutputNoise(menuCfg.sensorNoisePct);
      }
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_SENSOR_FREEZE_CHANGED:
      saveSettings();
      if (outputReady) {
        salida.setSensorFaultMode(mapSensorFaultToOutput(menuCfg.sensorFaultMode));
      }
      if (menuCfg.audioEnabled) buzzer.warning();
      break;

    case MenuUsuario::EV_SENSOR_OFFSET_CHANGED:
      menuCfg.sensorOffsetPct = clampFloat(menuCfg.sensorOffsetPct, -25.0f, 25.0f);
      saveSettings();
      if (outputReady) {
        salida.setSensorOffset(menuCfg.sensorOffsetPct);
      }
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_WIFI_CHANGED: {
      saveSettings();

      if (menuCfg.wifiEnabled) {
        startWiFiInterface();
        char msg[70];
        buildWiFiInfoMessage(msg, sizeof(msg));
        menu.showMessage("WIFI", msg, 3000);
        if (menuCfg.audioEnabled) buzzer.ok();
      } else {
        stopWiFiInterface();
        menu.showMessage("WIFI", "WiFi NO", 1200);
        if (menuCfg.audioEnabled) buzzer.click();
      }
      break;
    }

    case MenuUsuario::EV_WIFI_INFO_REQUEST: {
      char msg[70];
      buildWiFiInfoMessage(msg, sizeof(msg));
      menu.showMessage("INFO WIFI", msg, 3500);
      if (menuCfg.audioEnabled) buzzer.click();
      break;
    }

    case MenuUsuario::EV_WIFI_RESET_REQUEST: {
      wifiInterface.clearCredentials();
      if (menuCfg.wifiEnabled) {
        stopWiFiInterface();
        startWiFiInterface();
      }
      saveSettings();
      menu.showMessage("WIFI", "Credenciales\nborradas", 1800);
      if (menuCfg.audioEnabled) buzzer.warning();
      break;
    }

    case MenuUsuario::EV_AUDIO_CHANGED:
      buzzer.setEnabled(menuCfg.audioEnabled);
      display.setBuzzerEnabled(menuCfg.audioEnabled);
      saveSettings();
      if (menuCfg.audioEnabled) buzzer.ok();
      break;

    case MenuUsuario::EV_SD_DETECT_REQUEST: {
      const bool ok = detectSdCard();
      menu.showMessage("DETECTAR SD", ok ? "SD OK" : "SD FALLA", 1400);
      if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case MenuUsuario::EV_LOGGING_CHANGED: {
      if (menuCfg.loggingEnabled) {
        // Captura iniciada desde el display: se registran todas las columnas disponibles.
        sdLogger.setFieldMask(SDLogger::LOG_FIELD_ALL, false);
        const bool ok = startDataCapture();
        if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);
        menu.showMessage("CAPTURA", ok ? displayCaptureFileName() : "NO INICIA", 1400);
        if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      } else {
        stopDataCapture();
        if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);
        char msg[44];
        snprintf(msg, sizeof(msg), "Datos guardados en:\n%s", displayCaptureFileName());
        menu.showMessage("CAPTURA DETENIDA", msg, 4000);
        if (menuCfg.audioEnabled) buzzer.click();
      }
      break;
    }

    case MenuUsuario::EV_SAVE_DATA_REQUEST: {
      if (menuCfg.loggingEnabled) {
        stopDataCapture();
        if (displayReady) display.setCaptureActive(menuCfg.loggingEnabled);
        char msg[44];
        snprintf(msg, sizeof(msg), "Datos guardados en:\n%s", displayCaptureFileName());
        menu.showMessage("CAPTURA DETENIDA", msg, 4000);
        if (menuCfg.audioEnabled) buzzer.ok();
      } else {
        menu.showMessage("CAPTURA", "NO ACTIVA", 1200);
        if (menuCfg.audioEnabled) buzzer.warning();
      }
      break;
    }

    case MenuUsuario::EV_CAL_INPUT_4_REQUEST: {
      inputRaw4mA = entrada.captureRaw(128);
      const bool ok = saveInputCalibration();
      char msg[44];
      snprintf(msg, sizeof(msg), ok ? "RAW 4mA=%u" : "CAL INVALIDA 4=%u", inputRaw4mA);
      menu.showMessage("CAL ENTRADA", msg, 1600);
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case MenuUsuario::EV_CAL_INPUT_20_REQUEST: {
      inputRaw20mA = entrada.captureRaw(128);
      const bool ok = saveInputCalibration();
      char msg[44];
      snprintf(msg, sizeof(msg), ok ? "RAW 20mA=%u" : "CAL INVALIDA 20=%u", inputRaw20mA);
      menu.showMessage("CAL ENTRADA", msg, 1600);
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case MenuUsuario::EV_CAL_OUTPUT_4_TRIMMED: {
      const uint16_t trialRaw4 = (uint16_t)lroundf(menu.editValue());
      holdOutputCalibrationPoint(0.0f, trialRaw4, menuCfg.outputRaw20mA);
      break;
    }

    case MenuUsuario::EV_CAL_OUTPUT_20_TRIMMED: {
      const uint16_t trialRaw20 = (uint16_t)lroundf(menu.editValue());
      holdOutputCalibrationPoint(100.0f, menuCfg.outputRaw4mA, trialRaw20);
      break;
    }

    case MenuUsuario::EV_CAL_OUTPUT_4_CHANGED: {
      const bool ok = saveOutputCalibration();
      holdOutputCalibrationPoint(0.0f);
      char msg[44];
      snprintf(msg, sizeof(msg), ok ? "RAW 4mA=%u" : "CAL INVALIDA", menuCfg.outputRaw4mA);
      menu.showMessage("CAL SALIDA", msg, 1600);
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case MenuUsuario::EV_CAL_OUTPUT_20_CHANGED: {
      const bool ok = saveOutputCalibration();
      holdOutputCalibrationPoint(100.0f);
      char msg[44];
      snprintf(msg, sizeof(msg), ok ? "RAW 20mA=%u" : "CAL INVALIDA", menuCfg.outputRaw20mA);
      menu.showMessage("CAL SALIDA", msg, 1600);
      if (menuCfg.audioEnabled) ok ? buzzer.ok() : buzzer.warning();
      break;
    }

    case MenuUsuario::EV_CAL_OUTPUT_CANCELLED:
      cancelOutputCalibrationHold();
      break;

    case MenuUsuario::EV_EXIT_MENU:
    default:
      break;
  }
}

void syncDisplay() {
  if (!peripheralsOn || !displayReady) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastDisplaySyncMs) < 100) return;
  lastDisplaySyncMs = now;

  display.setWiFiStatus(currentDisplayWiFiStatus());
  display.setBuzzerEnabled(menuCfg.audioEnabled);
  display.setCaptureActive(menuCfg.loggingEnabled);
  char displayFaultTags[8];
  buildDisplayFaultTags(displayFaultTags, sizeof(displayFaultTags));
  display.setFaultStatus(displayFaultTags[0] ? displayFaultTags : "OK",
                         displayFaultTags[0] != '\0');

  // Las tendencias se actualizan también fuera de la pantalla principal.
  display.setReadingPercent(inputPct);
  display.setOutputPercent(outputReady ? salida.getLastOutputPct() : 0.0f);

  // Diagnóstico RAW en vivo para la sección de calibración del menú.
  // Se usa el RAW ADC filtrado porque es el valor que alimenta la conversión a mA/%.
  menuCfg.inputRawCurrent = entrada.filteredRaw();
  menuCfg.outputRawCurrent = outputReady ? salida.getLastOutputRaw() : 0;

  if (display.isSpecialScreenActive()) {
    return;
  }

  if (menu.isMain()) {
    display.setScreenMode(Display::SCREEN_MAIN);
    display.setChannelEnabled(menuCfg.outputEnabled);
    display.setChannelMode(menuCfg.manualOutputMode ? "MAN" : "AUTO");
    display.setManualModeIndicator(menuCfg.manualOutputMode);
    display.setAlarms(entrada.overrange(), entrada.underrange());

  } else if (menu.isMenu()) {
    char l0[22], l1[22], l2[22], l3[22];
    uint8_t cursor = 0;
    menu.getVisibleMenuLines(menuCfg, l0, l1, l2, l3, cursor);
    display.setMenuScreen(menu.menuTitle(), l0, l1, l2, l3, cursor);

  } else if (menu.isEdit()) {
    display.setEditScreen(menu.editTitle(), menu.editValue(), menu.editUnit());

  } else if (menu.isMessage()) {
    display.setMessageScreen(menu.messageTitle(), menu.messageText());
  }
}

void updateLogging() {
  if (!peripheralsOn || !sdReady || !menuCfg.loggingEnabled) return;

  const uint32_t now = millis();
  if ((uint32_t)(now - lastLogMs) < CAPTURE_PERIOD_MS) return;
  lastLogMs = now;

  if (!captureOneSample()) {
    menuCfg.loggingEnabled = false;
    markSdUnavailable();
    if (displayReady) display.setCaptureActive(false);
    menu.showMessage("CAPTURA", "ERROR SD", 1400);
    if (menuCfg.audioEnabled) buzzer.warning();
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200); // Puerto de diagnóstico; la operación normal usa OLED y botones.
#ifdef ESP32
  randomSeed(esp_random());
#else
  randomSeed(analogRead(0));
#endif

  pinMode(PIN_PWR_BTN, INPUT);

  // Al despertar por GPIO15 el botón todavía suele estar presionado.
  // Se toma ese nivel como estado inicial y se ignora su primera liberación,
  // evitando un apagado instantáneo después del wake-up.
  const bool pwrAtBoot = (digitalRead(PIN_PWR_BTN) == HIGH);
  pwrRaw = pwrAtBoot;
  pwrStable = pwrAtBoot;
  pwrLastChangeMs = millis();
  ignoreNextPwrRelease = pwrAtBoot;

  loadSettings();

  buzzer.begin();
  buzzer.setEnabled(menuCfg.audioEnabled);

  teclado.begin();
  menu.begin();

  initPeripherals();
}

void loop() {
  if (powerButtonClicked()) {
    togglePeripheralsPower();
  }

  if (!peripheralsOn) {
    delay(5);
    return;
  }

  teclado.update();
  buzzer.update();

  updateInput();
  updateControlOutput();

  MenuUsuario::Event ev = MenuUsuario::EV_NONE;
  if (!displayReady || !display.isSpecialScreenActive()) {
    ev = menu.update(teclado, menuCfg);
  }
  handleMenuEvent(ev);
  commitPendingManualOutputSave();

  updateSdStatusPeriodic();
  updateWiFiInterface();

  syncDisplay();
  display.update();

  updateLogging();
}
