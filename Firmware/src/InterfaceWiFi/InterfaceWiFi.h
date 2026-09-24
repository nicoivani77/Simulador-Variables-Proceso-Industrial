/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Interfaz de la comunicación WiFi y sincronización de datos con la web embebida.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "../ProcessPresets/ProcessPresets.h"

static const uint16_t IFW_CSV_MS                 = 1 << 0;
static const uint16_t IFW_CSV_INPUT_MA           = 1 << 1;
static const uint16_t IFW_CSV_INPUT_PCT          = 1 << 2;
static const uint16_t IFW_CSV_OUTPUT_MA          = 1 << 3;
static const uint16_t IFW_CSV_OUTPUT_PCT         = 1 << 4;
static const uint16_t IFW_CSV_OUTPUT_ENABLED     = 1 << 5;
static const uint16_t IFW_CSV_MODE               = 1 << 6;
static const uint16_t IFW_CSV_PRESET             = 1 << 7;
static const uint16_t IFW_CSV_PROCESS_K          = 1 << 8;
static const uint16_t IFW_CSV_PROCESS_TAU        = 1 << 9;
static const uint16_t IFW_CSV_PROCESS_DEADTIME   = 1 << 10;
static const uint16_t IFW_CSV_SENSOR_NOISE       = 1 << 11;
static const uint16_t IFW_CSV_SENSOR_OFFSET      = 1 << 12;
static const uint16_t IFW_CSV_ERRORS             = 1 << 13;
static const uint16_t IFW_CSV_OUTPUT_CALIBRATION = 1 << 14;

static const uint16_t IFW_CSV_ALL_MASK =
  IFW_CSV_MS |
  IFW_CSV_INPUT_PCT |
  IFW_CSV_OUTPUT_PCT |
  IFW_CSV_OUTPUT_ENABLED |
  IFW_CSV_MODE |
  IFW_CSV_PRESET |
  IFW_CSV_PROCESS_K |
  IFW_CSV_PROCESS_TAU |
  IFW_CSV_PROCESS_DEADTIME |
  IFW_CSV_SENSOR_NOISE |
  IFW_CSV_SENSOR_OFFSET |
  IFW_CSV_ERRORS |
  IFW_CSV_OUTPUT_CALIBRATION;

static const uint16_t IFW_CSV_DEFAULT_MASK = IFW_CSV_ALL_MASK;

struct InterfaceWiFiLogConfig
{
  uint16_t csvMask = IFW_CSV_DEFAULT_MASK;
};

struct InterfaceWiFiChannel
{
  bool enabled = true;
  String mode = "PASIVO";
  float outputPct = 0.0f;
  float inputPct = 0.0f;
  String note = "OK";

  ProcessPreset processPreset = PROCESS_PRESET_CUSTOM;
  float processK = 1.0f;
  float processTau = 2.0f;
  float processDeadTime = 0.0f;
  float sensorNoisePct = 0.0f;
  float sensorOffsetPct = 0.0f;
  String sensorFault = "OK";
  bool faultActive = false;
};

struct InterfaceWiFiProcessConfig
{
  ProcessPreset preset = PROCESS_PRESET_CUSTOM;
  float K = 1.0f;
  float tau = 2.0f;
  float deadTime = 0.0f;
};

enum InterfaceWiFiUserAction : uint8_t
{
  IFW_ACTION_NONE = 0,
  IFW_ACTION_WIFI_RESET,
  IFW_ACTION_SD_DETECT,
  IFW_ACTION_SAVE_DATA,
  IFW_ACTION_CAL_INPUT_4,
  IFW_ACTION_CAL_INPUT_20,
  IFW_ACTION_TEST_OUTPUT_4,
  IFW_ACTION_TEST_OUTPUT_20,
  IFW_ACTION_SPECIAL
};

struct InterfaceWiFiUserConfig
{
  bool outputEnabled = false;
  bool manualOutputMode = false;
  float manualOutputPct = 0.0f;

  ProcessPreset preset = PROCESS_PRESET_CUSTOM;
  float K = 1.0f;
  float tau = 2.0f;
  float deadTime = 0.0f;

  float sensorNoisePct = 0.0f;
  uint8_t sensorFaultMode = 0;
  float sensorOffsetPct = 0.0f;

  bool wifiEnabled = false;
  bool audioEnabled = true;
  bool loggingEnabled = false;

  uint16_t outputRaw4mA = 440;
  uint16_t outputRaw20mA = 4000;
  bool outputCalibrationUpdate = false;
};

struct InterfaceWiFiUserActionRequest
{
  InterfaceWiFiUserAction action = IFW_ACTION_NONE;

  // Valores RAW de salida usados únicamente para pruebas temporales desde la web.
  uint16_t outputRaw4mA = 440;
  uint16_t outputRaw20mA = 4000;
};

class InterfaceWiFiClass
{
public:
  InterfaceWiFiClass();

  bool begin(const char* hostname,
             const char* apSsid,
             const char* apPass,
             uint32_t stationTimeoutMs = 15000);

  void handle();
  void end();
  void clearCredentials();

  bool isActive() const;
  bool isConnecting() const;
  bool isStationConnected() const;
  bool isAccessPointMode() const;

  void setDeviceName(const String& name);
  void setProjectState(const String& state);

  // Actualización directa de porcentajes usados por la web.
  void setProcessPercent(bool enabled,
                         const String& mode,
                         float outputPct,
                         float inputPct,
                         const String& note);

  void setProcessDiagnostics(ProcessPreset preset,
                             float K,
                             float tau,
                             float deadTime,
                             float noisePct,
                             const String& sensorFault,
                             float sensorOffsetPct,
                             bool faultActive);

  void setUserConfigSnapshot(bool outputEnabled,
                             bool manualOutputMode,
                             float manualOutputPct,
                             bool wifiEnabled,
                             bool audioEnabled,
                             bool sdReady,
                             bool loggingEnabled,
                             uint16_t inputRaw4mA,
                             uint16_t inputRaw20mA,
                             uint16_t inputRawCurrent,
                             uint16_t outputRawCurrent,
                             uint16_t outputRaw4mA,
                             uint16_t outputRaw20mA,
                             uint8_t sensorFaultMode);

  bool consumeProcessConfigRequest(InterfaceWiFiProcessConfig& out);
  bool consumeLogConfigRequest(InterfaceWiFiLogConfig& out);
  bool consumeUserConfigRequest(InterfaceWiFiUserConfig& out);
  bool consumeUserActionRequest(InterfaceWiFiUserActionRequest& out);

  uint16_t getCsvLogMask() const;
  void setCsvLogMask(uint16_t mask);
  void setLastCsvPath(const char* path);

  String getModeString() const;
  String getIpString() const;
  String getSSIDString() const;
  int32_t getRSSI() const;

private:
  enum ConnectionState : uint8_t {
    CONN_IDLE = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_AP
  };

  WebServer _server;
  Preferences _prefs;

  String _hostname;
  String _apSsid;
  String _apPass;

  String _savedSsid;
  String _savedPass;

  String _deviceName = "Simulador de Variables de Proceso para Lazos de Control Industrial";
  String _projectState = "Inicializando";

  InterfaceWiFiChannel _channel;
  InterfaceWiFiProcessConfig _pendingProcessConfig;
  bool _processConfigPending = false;

  uint16_t _csvLogMask = IFW_CSV_DEFAULT_MASK;
  InterfaceWiFiLogConfig _pendingLogConfig;
  bool _logConfigPending = false;

  InterfaceWiFiUserConfig _controlSnapshot;
  bool _sdReady = false;
  uint16_t _inputRaw4mA = 0;
  uint16_t _inputRaw20mA = 0;
  uint16_t _inputRawCurrent = 0;
  uint16_t _outputRawCurrent = 0;
  InterfaceWiFiUserConfig _pendingUserConfig;
  bool _userConfigPending = false;
  InterfaceWiFiUserActionRequest _pendingUserAction;
  bool _userActionPending = false;
  String _lastCsvPath = "";

  ConnectionState _connectionState = CONN_IDLE;
  uint32_t _stationTimeoutMs = 15000;
  uint32_t _connectStartMs = 0;

  bool _restartPending = false;
  uint32_t _restartAtMs = 0;
  bool _active = false;
  bool _routesConfigured = false;
  bool _serverStarted = false;

  void startStationAttempt();
  void startAccessPoint();
  void saveCredentials(const String& ssid, const String& pass);
  void updateConnectionState();
  void updatePendingRestart();

  void setupRoutes();
  void handleRoot();
  void handleStatus();
  void handleSaveWiFi();
  void handleSaveProcessConfig();
  void handleSaveLogConfig();
  void handleDownloadLastCsv();
  void handleSaveUserConfig();
  void handleUserAction();
  void handleRestart();
  void handleNotFound();

  String buildStatusJson() const;
  String jsonEscape(const String& input) const;
  uint16_t sanitizeCsvLogMask(uint16_t mask) const;
  bool argBool(const char* name, bool fallback);
  float argFloat(const char* name, float fallback, float lo, float hi);
  uint16_t argU16(const char* name, uint16_t fallback, uint16_t lo, uint16_t hi);
};
