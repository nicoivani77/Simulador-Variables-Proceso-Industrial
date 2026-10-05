/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de la interfaz web embebida y los servicios HTTP de configuración y monitoreo.
 */

#include "InterfaceWiFi.h"
#include "InterfaceWiFiPage.h"
#include <esp_system.h>
#include <SD.h>
#include <math.h>

InterfaceWiFiClass::InterfaceWiFiClass()
  : _server(80) {}

bool InterfaceWiFiClass::begin(const char* hostname,
                               const char* apSsid,
                               const char* apPass,
                               uint32_t stationTimeoutMs)
{
  if (_active) {
    return isStationConnected();
  }

  _hostname = hostname ? hostname : "simulador-industrial";
  _apSsid   = apSsid   ? apSsid   : "Simulador420_Wifi";
  _apPass   = apPass   ? apPass   : "";
  _stationTimeoutMs = stationTimeoutMs;

  _prefs.begin("ifwifi", false);
  _savedSsid = _prefs.getString("ssid", "");
  _savedPass = _prefs.getString("pass", "");
  _csvLogMask = sanitizeCsvLogMask((uint16_t)_prefs.getUInt("csvMask", IFW_CSV_DEFAULT_MASK));

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  if (_savedSsid.length() > 0) {
    startStationAttempt();
  } else {
    startAccessPoint();
  }

  if (!_routesConfigured) {
    setupRoutes();
    _routesConfigured = true;
  }

  if (!_serverStarted) {
    _server.begin();
    _serverStarted = true;
  }

  _active = true;

  Serial.println();
  Serial.println("InterfaceWiFi lista.");
  Serial.print("Modo WiFi: ");
  Serial.println(getModeString());
  Serial.print("SSID: ");
  Serial.println(getSSIDString());
  Serial.print("IP: ");
  Serial.println(getIpString());

  return isStationConnected();
}

void InterfaceWiFiClass::handle()
{
  if (!_active) return;

  if (_serverStarted) {
    _server.handleClient();
  }

  updateConnectionState();
  updatePendingRestart();
}

void InterfaceWiFiClass::end()
{
  if (_serverStarted) {
    _server.stop();
    _serverStarted = false;
  }

  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  _connectionState = CONN_IDLE;
  _active = false;
  _restartPending = false;
}

void InterfaceWiFiClass::clearCredentials()
{
  Preferences p;
  p.begin("ifwifi", false);
  p.remove("ssid");
  p.remove("pass");
  p.end();

  _savedSsid = "";
  _savedPass = "";

  Serial.println("Credenciales WiFi borradas.");
}

bool InterfaceWiFiClass::isActive() const
{
  return _active;
}

bool InterfaceWiFiClass::isConnecting() const
{
  return _active && _connectionState == CONN_CONNECTING;
}

bool InterfaceWiFiClass::isStationConnected() const
{
  return _active && WiFi.status() == WL_CONNECTED;
}

bool InterfaceWiFiClass::isAccessPointMode() const
{
  if (!_active) return false;
  const wifi_mode_t mode = WiFi.getMode();
  return mode == WIFI_AP || mode == WIFI_AP_STA || _connectionState == CONN_AP;
}

void InterfaceWiFiClass::setDeviceName(const String& name)
{
  _deviceName = name;
}

void InterfaceWiFiClass::setProjectState(const String& state)
{
  _projectState = state;
}


void InterfaceWiFiClass::setProcessPercent(bool enabled,
                                           const String& mode,
                                           float outputPct,
                                           float inputPct,
                                           const String& note)
{
  _channel.enabled = enabled;
  _channel.mode = mode;
  _channel.outputPct = constrain(outputPct, 0.0f, 100.0f);
  _channel.inputPct = constrain(inputPct, 0.0f, 100.0f);
  _channel.note = note;
}

void InterfaceWiFiClass::setProcessDiagnostics(ProcessPreset preset,
                                               float K,
                                               float tau,
                                               float deadTime,
                                               float noisePct,
                                               const String& sensorFault,
                                               float sensorOffsetPct,
                                               bool faultActive)
{
  _channel.processPreset = sanitizeProcessPreset(static_cast<uint8_t>(preset));
  _channel.processK = K;
  _channel.processTau = tau;
  _channel.processDeadTime = deadTime;
  _channel.sensorNoisePct = constrain(noisePct, 0.0f, 100.0f);
  _channel.sensorFault = sensorFault;
  _channel.sensorOffsetPct = sensorOffsetPct;
  _channel.faultActive = faultActive;
}

void InterfaceWiFiClass::setUserConfigSnapshot(bool outputEnabled,
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
                                                uint8_t sensorFaultMode)
{
  _controlSnapshot.outputEnabled = outputEnabled;
  _controlSnapshot.manualOutputMode = manualOutputMode;
  _controlSnapshot.manualOutputPct = constrain(manualOutputPct, 0.0f, 100.0f);
  _controlSnapshot.preset = _channel.processPreset;
  _controlSnapshot.K = _channel.processK;
  _controlSnapshot.tau = _channel.processTau;
  _controlSnapshot.deadTime = _channel.processDeadTime;
  _controlSnapshot.sensorNoisePct = _channel.sensorNoisePct;
  _controlSnapshot.sensorFaultMode = sensorFaultMode;
  _controlSnapshot.sensorOffsetPct = _channel.sensorOffsetPct;
  _controlSnapshot.wifiEnabled = wifiEnabled;
  _controlSnapshot.audioEnabled = audioEnabled;
  _controlSnapshot.loggingEnabled = loggingEnabled;
  _controlSnapshot.inputRaw4mA = inputRaw4mA;
  _controlSnapshot.inputRaw20mA = inputRaw20mA;
  _controlSnapshot.outputRaw4mA = outputRaw4mA;
  _controlSnapshot.outputRaw20mA = outputRaw20mA;
  _sdReady = sdReady;
  _inputRaw4mA = inputRaw4mA;
  _inputRaw20mA = inputRaw20mA;
  _inputRawCurrent = inputRawCurrent;
  _outputRawCurrent = outputRawCurrent;
}

bool InterfaceWiFiClass::consumeProcessConfigRequest(InterfaceWiFiProcessConfig& out)
{
  if (!_processConfigPending) return false;

  out = _pendingProcessConfig;
  _processConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeLogConfigRequest(InterfaceWiFiLogConfig& out)
{
  if (!_logConfigPending) return false;

  out = _pendingLogConfig;
  _logConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeUserConfigRequest(InterfaceWiFiUserConfig& out)
{
  if (!_userConfigPending) return false;

  out = _pendingUserConfig;
  _userConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeUserActionRequest(InterfaceWiFiUserActionRequest& out)
{
  if (!_userActionPending) return false;

  out = _pendingUserAction;
  _pendingUserAction.action = IFW_ACTION_NONE;
  _userActionPending = false;
  return true;
}

uint16_t InterfaceWiFiClass::getCsvLogMask() const
{
  return _csvLogMask;
}

void InterfaceWiFiClass::setCsvLogMask(uint16_t mask)
{
  _csvLogMask = sanitizeCsvLogMask(mask);
}

void InterfaceWiFiClass::setLastCsvPath(const char* path)
{
  if (!path || !path[0]) {
    _lastCsvPath = "";
    return;
  }

  _lastCsvPath = path;
  if (!_lastCsvPath.startsWith("/")) {
    _lastCsvPath = "/" + _lastCsvPath;
  }
}

void InterfaceWiFiClass::startStationAttempt()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(_hostname.c_str());
  WiFi.begin(_savedSsid.c_str(), _savedPass.c_str());

  _connectionState = CONN_CONNECTING;
  _connectStartMs = millis();

  Serial.println("Intentando conectar a WiFi guardado...");
  Serial.print("SSID: ");
  Serial.println(_savedSsid);
}

void InterfaceWiFiClass::startAccessPoint()
{
  WiFi.mode(WIFI_AP);

  bool apStarted = false;
  if (_apPass.length() >= 8) {
    apStarted = WiFi.softAP(_apSsid.c_str(), _apPass.c_str());
  } else {
    // WPA/WPA2 no permite claves menores a 8 caracteres. Si se configura "1234",
    // el AP se levanta abierto para no fallar silenciosamente.
    apStarted = WiFi.softAP(_apSsid.c_str());
  }

  _connectionState = CONN_AP;

  Serial.println("Levantando Access Point...");
  Serial.print("AP SSID: ");
  Serial.println(_apSsid);
  Serial.print("AP protegido: ");
  Serial.println((_apPass.length() >= 8 && apStarted) ? "SI" : "NO");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void InterfaceWiFiClass::saveCredentials(const String& ssid, const String& pass)
{
  _prefs.putString("ssid", ssid);
  _prefs.putString("pass", pass);
  _savedSsid = ssid;
  _savedPass = pass;

  Serial.println("Credenciales WiFi guardadas.");
}

void InterfaceWiFiClass::updateConnectionState()
{
  if (_connectionState == CONN_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      _connectionState = CONN_CONNECTED;
      Serial.println("WiFi conectado.");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;
    }

    const uint32_t now = millis();
    if ((uint32_t)(now - _connectStartMs) >= _stationTimeoutMs) {
      Serial.println("No se pudo conectar al WiFi guardado.");
      startAccessPoint();
    }
  } else if (_connectionState == CONN_CONNECTED) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Se perdio la conexion WiFi. Volviendo a AP.");
      startAccessPoint();
    }
  }
}

void InterfaceWiFiClass::updatePendingRestart()
{
  if (_restartPending && (int32_t)(millis() - _restartAtMs) >= 0) {
    ESP.restart();
  }
}

void InterfaceWiFiClass::setupRoutes()
{
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/live", HTTP_GET, [this]() { handleLive(); });
  _server.on("/api/wifi/save", HTTP_POST, [this]() { handleSaveWiFi(); });
  _server.on("/api/process/config", HTTP_POST, [this]() { handleSaveProcessConfig(); });
  _server.on("/api/log/config", HTTP_POST, [this]() { handleSaveLogConfig(); });
  _server.on("/api/log/download", HTTP_GET, [this]() { handleDownloadLastCsv(); });
  _server.on("/api/user/config", HTTP_POST, [this]() { handleSaveUserConfig(); });
  _server.on("/api/user/action", HTTP_POST, [this]() { handleUserAction(); });
  _server.on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
  _server.onNotFound([this]() { handleNotFound(); });
}

void InterfaceWiFiClass::handleRoot()
{
  _server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void InterfaceWiFiClass::handleStatus()
{
  _server.send(200, "application/json; charset=utf-8", buildStatusJson());
}

void InterfaceWiFiClass::handleLive()
{
  // Endpoint liviano para la tendencia. Evita reconstruir el JSON completo
  // de estado varias veces por segundo solo para actualizar dos variables.
  String s;
  s.reserve(96);
  s += "{\"inputPct\":";
  s += String(_channel.inputPct, 2);
  s += ",\"outputPct\":";
  s += String(_channel.outputPct, 2);
  s += ",\"uptimeMs\":";
  s += String(millis());
  s += "}";
  _server.send(200, "application/json; charset=utf-8", s);
}

void InterfaceWiFiClass::handleSaveWiFi()
{
  if (!_server.hasArg("ssid")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro ssid.");
    return;
  }

  const String ssid = _server.arg("ssid");
  const String pass = _server.arg("pass");

  if (ssid.length() == 0) {
    _server.send(400, "text/plain; charset=utf-8", "El SSID no puede estar vacío.");
    return;
  }

  saveCredentials(ssid, pass);
  _server.send(200, "text/plain; charset=utf-8", "Credenciales WiFi guardadas. Reiniciá el equipo o apagá y encendé WiFi desde el menú.");
}

void InterfaceWiFiClass::handleSaveProcessConfig()
{
  InterfaceWiFiProcessConfig cfg;
  cfg.preset = sanitizeProcessPreset((uint8_t)_server.arg("preset").toInt());

  if (processPresetIsAutomatic(cfg.preset)) {
    const ProcessPresetDefinition& def = processPresetDefinition(cfg.preset);
    cfg.K = def.K;
    cfg.tau = def.tau_s;
    cfg.deadTime = def.deadTime_s;
  } else {
    cfg.K = _server.hasArg("K") ? _server.arg("K").toFloat() : _channel.processK;
    cfg.tau = _server.hasArg("tau") ? _server.arg("tau").toFloat() : _channel.processTau;
    cfg.deadTime = _server.hasArg("deadTime") ? _server.arg("deadTime").toFloat() : _channel.processDeadTime;

    cfg.K = constrain(cfg.K, PROCESS_K_MIN, PROCESS_K_MAX);
    cfg.tau = constrain(cfg.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    cfg.deadTime = constrain(cfg.deadTime, 0.0f, 30.0f);
  }

  _channel.processPreset = cfg.preset;
  _channel.processK = cfg.K;
  _channel.processTau = cfg.tau;
  _channel.processDeadTime = cfg.deadTime;

  _pendingProcessConfig = cfg;
  _processConfigPending = true;

  String msg = "Modelo aplicado: ";
  msg += processPresetName(cfg.preset);
  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleSaveLogConfig()
{
  if (!_server.hasArg("mask")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro mask.");
    return;
  }

  const uint16_t mask = sanitizeCsvLogMask((uint16_t)_server.arg("mask").toInt());

  _csvLogMask = mask;
  _prefs.putUInt("csvMask", _csvLogMask);

  _pendingLogConfig.csvMask = _csvLogMask;
  _logConfigPending = true;

  _server.send(200, "text/plain; charset=utf-8", "Configuración de columnas CSV guardada.");
}

void InterfaceWiFiClass::handleDownloadLastCsv()
{
  if (_lastCsvPath.length() == 0) {
    _server.send(404, "text/plain; charset=utf-8", "No hay CSV generado todavía.");
    return;
  }

  String path = _lastCsvPath;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (!SD.exists(path)) {
    _server.send(404, "text/plain; charset=utf-8", "No se encontró el último CSV en la SD.");
    return;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    _server.send(500, "text/plain; charset=utf-8", "No se pudo abrir el CSV.");
    return;
  }

  String fileName = path;
  const int slash = fileName.lastIndexOf('/');
  if (slash >= 0) {
    fileName = fileName.substring(slash + 1);
  }

  _server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
  _server.streamFile(f, "text/csv");
  f.close();
}

void InterfaceWiFiClass::handleSaveUserConfig()
{
  InterfaceWiFiUserConfig cfg;

  cfg.outputEnabled = argBool("outputEnabled", argBool("simulation", _controlSnapshot.outputEnabled));
  cfg.manualOutputMode = argBool("manualMode", _controlSnapshot.manualOutputMode);
  cfg.manualOutputPct = argFloat("manualPct", _controlSnapshot.manualOutputPct, 0.0f, 100.0f);

  cfg.preset = sanitizeProcessPreset((uint8_t)_server.arg("preset").toInt());
  if (processPresetIsAutomatic(cfg.preset)) {
    const ProcessPresetDefinition& def = processPresetDefinition(cfg.preset);
    cfg.K = def.K;
    cfg.tau = def.tau_s;
    cfg.deadTime = def.deadTime_s;
  } else {
    cfg.K = argFloat("K", _controlSnapshot.K, PROCESS_K_MIN, PROCESS_K_MAX);
    cfg.tau = argFloat("tau", _controlSnapshot.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    cfg.deadTime = argFloat("deadTime", _controlSnapshot.deadTime, 0.0f, 30.0f);
  }

  cfg.sensorNoisePct = argFloat("noise", _controlSnapshot.sensorNoisePct, 0.0f, 10.0f);
  cfg.sensorFaultMode = (uint8_t)constrain(_server.arg("fault").toInt(), 0, 1);
  cfg.sensorOffsetPct = argFloat("offset", _controlSnapshot.sensorOffsetPct, -25.0f, 25.0f);

  cfg.wifiEnabled = argBool("wifi", _controlSnapshot.wifiEnabled);
  cfg.audioEnabled = argBool("audio", _controlSnapshot.audioEnabled);
  cfg.loggingEnabled = argBool("logging", _controlSnapshot.loggingEnabled);

  const String source = _server.hasArg("source") ? _server.arg("source") : "general";
  cfg.inputRaw4mA = _inputRaw4mA;
  cfg.inputRaw20mA = _inputRaw20mA;
  cfg.inputCalibrationUpdate = false;
  cfg.outputRaw4mA = _controlSnapshot.outputRaw4mA;
  cfg.outputRaw20mA = _controlSnapshot.outputRaw20mA;
  cfg.outputCalibrationUpdate = false;

  if (source == "calibracionEntrada") {
    if (!_server.hasArg("in4") || !_server.hasArg("in20")) {
      _server.send(400, "text/plain; charset=utf-8", "Faltan los valores RAW de calibración de entrada.");
      return;
    }

    const uint16_t raw4 = argU16("in4", _inputRaw4mA, 0, 4095);
    const uint16_t raw20 = argU16("in20", _inputRaw20mA, 0, 4095);

    if (raw20 <= raw4 || (raw20 - raw4) < 300U) {
      _server.send(400, "text/plain; charset=utf-8", "Calibración de entrada inválida. RAW 20 mA debe ser mayor que RAW 4 mA y la separación mínima es de 300 cuentas.");
      return;
    }

    cfg.inputRaw4mA = raw4;
    cfg.inputRaw20mA = raw20;
    cfg.inputCalibrationUpdate = true;
  }

  if (source == "calibracionSalida") {
    if (!_server.hasArg("out4") || !_server.hasArg("out20")) {
      _server.send(400, "text/plain; charset=utf-8", "Faltan los valores RAW de calibración.");
      return;
    }

    const uint16_t raw4 = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    const uint16_t raw20 = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);

    if (raw20 <= raw4 || (raw20 - raw4) < 100U) {
      _server.send(400, "text/plain; charset=utf-8", "Calibración de salida inválida. Revisá RAW 4 mA y RAW 20 mA.");
      return;
    }

    cfg.outputRaw4mA = raw4;
    cfg.outputRaw20mA = raw20;
    cfg.outputCalibrationUpdate = true;
  }

  if (_server.hasArg("csvMask")) {
    _csvLogMask = sanitizeCsvLogMask((uint16_t)_server.arg("csvMask").toInt());
    _prefs.putUInt("csvMask", _csvLogMask);
  }

  _pendingUserConfig = cfg;
  _userConfigPending = true;

  _controlSnapshot = cfg;
  if (cfg.inputCalibrationUpdate) {
    _inputRaw4mA = cfg.inputRaw4mA;
    _inputRaw20mA = cfg.inputRaw20mA;
  }
  _channel.enabled = cfg.outputEnabled;
  _channel.mode = cfg.manualOutputMode ? "MANUAL" : "AUTO";
  _channel.processPreset = cfg.preset;
  _channel.processK = cfg.K;
  _channel.processTau = cfg.tau;
  _channel.processDeadTime = cfg.deadTime;
  _channel.sensorNoisePct = cfg.sensorNoisePct;
  _channel.sensorOffsetPct = cfg.sensorOffsetPct;
  _channel.sensorFault = cfg.sensorFaultMode ? "Congelado" : "OK";
  _channel.faultActive = cfg.sensorFaultMode != 0 || cfg.sensorNoisePct > 0.0f || fabsf(cfg.sensorOffsetPct) > 0.001f;

  String msg;
  if (source == "calibracionEntrada") {
    msg = "Calibración de entrada enviada al equipo.";
  } else if (source == "calibracionSalida") {
    msg = "Calibración de salida enviada al equipo.";
  } else if (source == "salida") {
    msg = cfg.outputEnabled ? "Salida activada." : "Salida desactivada.";
  } else if (source == "modo") {
    msg = cfg.manualOutputMode ? "Modo manual activado." : "Modo automático activado.";
  } else if (source == "salidaManual") {
    msg = "Salida manual actualizada.";
  } else if (source == "capturaInicio") {
    msg = "Captura CSV iniciada.";
  } else if (source == "capturaDetencion") {
    msg = "Captura CSV detenida.";
  } else if (source == "audio") {
    msg = cfg.audioEnabled ? "Audio activado." : "Audio desactivado.";
  } else {
    msg = "Configuración guardada.";
  }

  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleUserAction()
{
  if (!_server.hasArg("action")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro action.");
    return;
  }

  const String action = _server.arg("action");
  InterfaceWiFiUserActionRequest req;

  if (action == "wifiReset") {
    req.action = IFW_ACTION_WIFI_RESET;
  } else if (action == "sdDetect") {
    req.action = IFW_ACTION_SD_DETECT;
  } else if (action == "saveData") {
    req.action = IFW_ACTION_SAVE_DATA;
  } else if (action == "calInput4") {
    req.action = IFW_ACTION_CAL_INPUT_4;
  } else if (action == "calInput20") {
    req.action = IFW_ACTION_CAL_INPUT_20;
  } else if (action == "testOut4") {
    req.action = IFW_ACTION_TEST_OUTPUT_4;
    req.outputRaw4mA = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    req.outputRaw20mA = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);
  } else if (action == "testOut20") {
    req.action = IFW_ACTION_TEST_OUTPUT_20;
    req.outputRaw4mA = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    req.outputRaw20mA = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);
  } else if (action == "special") {
    req.action = IFW_ACTION_SPECIAL;
  } else {
    _server.send(400, "text/plain; charset=utf-8", "Acción desconocida.");
    return;
  }

  _pendingUserAction = req;
  _userActionPending = true;

  String msg;
  if (action == "wifiReset") {
    msg = "Borrado de credenciales WiFi solicitado.";
  } else if (action == "sdDetect") {
    msg = "Detección de tarjeta SD solicitada.";
  } else if (action == "saveData") {
    msg = "Guardado manual de datos solicitado.";
  } else if (action == "calInput4") {
    msg = "Calibración de entrada 4 mA solicitada.";
  } else if (action == "calInput20") {
    msg = "Calibración de entrada 20 mA solicitada.";
  } else if (action == "testOut4") {
    msg = "Prueba de salida 4 mA solicitada.";
  } else if (action == "testOut20") {
    msg = "Prueba de salida 20 mA solicitada.";
  } else if (action == "special") {
    msg = "OK";
  } else {
    msg = "Acción recibida.";
  }

  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleRestart()
{
  _server.send(200, "text/plain; charset=utf-8", "Equipo reiniciando...");
  _restartPending = true;
  _restartAtMs = millis() + 300;
}

void InterfaceWiFiClass::handleNotFound()
{
  _server.send(404, "text/plain; charset=utf-8", "Recurso no encontrado.");
}

String InterfaceWiFiClass::jsonEscape(const String& input) const
{
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    const char c = input[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }

  return out;
}

String InterfaceWiFiClass::buildStatusJson() const
{
  String s;
  s.reserve(1600);

  s += "{";
  s += "\"deviceName\":\"" + jsonEscape(_deviceName) + "\",";
  s += "\"projectState\":\"" + jsonEscape(_projectState) + "\",";

  s += "\"wifi\":{";
  s += "\"mode\":\"" + jsonEscape(getModeString()) + "\",";
  s += "\"ssid\":\"" + jsonEscape(getSSIDString()) + "\",";
  s += "\"ip\":\"" + jsonEscape(getIpString()) + "\",";
  s += "\"rssi\":" + String(getRSSI());
  s += "},";

  s += "\"uptimeMs\":" + String(millis()) + ",";

  s += "\"logging\":{";
  s += "\"csvMask\":" + String(_csvLogMask) + ",";
  s += "\"lastCsv\":\"" + jsonEscape(_lastCsvPath) + "\"";
  s += "},";

  s += "\"control\":{";
  s += "\"manualOutputPct\":" + String(_controlSnapshot.manualOutputPct, 1) + ",";
  s += "\"wifiEnabled\":" + String(_controlSnapshot.wifiEnabled ? "true" : "false") + ",";
  s += "\"audioEnabled\":" + String(_controlSnapshot.audioEnabled ? "true" : "false") + ",";
  s += "\"sdReady\":" + String(_sdReady ? "true" : "false") + ",";
  s += "\"loggingEnabled\":" + String(_controlSnapshot.loggingEnabled ? "true" : "false") + ",";
  s += "\"sensorFaultMode\":" + String(_controlSnapshot.sensorFaultMode) + ",";
  s += "\"inputRaw4mA\":" + String(_inputRaw4mA) + ",";
  s += "\"inputRaw20mA\":" + String(_inputRaw20mA) + ",";
  s += "\"inputRawCurrent\":" + String(_inputRawCurrent) + ",";
  s += "\"outputRawCurrent\":" + String(_outputRawCurrent) + ",";
  s += "\"outputRaw4mA\":" + String(_controlSnapshot.outputRaw4mA) + ",";
  s += "\"outputRaw20mA\":" + String(_controlSnapshot.outputRaw20mA);
  s += "},";

  s += "\"process\":{";
  s += "\"enabled\":" + String(_channel.enabled ? "true" : "false") + ",";
  s += "\"mode\":\"" + jsonEscape(_channel.mode) + "\",";
  s += "\"outputPct\":" + String(_channel.outputPct, 1) + ",";
  s += "\"inputPct\":" + String(_channel.inputPct, 1) + ",";
  s += "\"preset\":{";
  s += "\"id\":" + String((uint8_t)_channel.processPreset) + ",";
  s += "\"name\":\"" + jsonEscape(processPresetName(_channel.processPreset)) + "\"";
  s += "},";
  s += "\"model\":{";
  s += "\"K\":" + String(_channel.processK, 3) + ",";
  s += "\"tau\":" + String(_channel.processTau, 3) + ",";
  s += "\"deadTime\":" + String(_channel.processDeadTime, 3);
  s += "},";
  s += "\"sensor\":{";
  s += "\"noisePct\":" + String(_channel.sensorNoisePct, 2) + ",";
  s += "\"offsetPct\":" + String(_channel.sensorOffsetPct, 2) + ",";
  s += "\"fault\":\"" + jsonEscape(_channel.sensorFault) + "\",";
  s += "\"active\":" + String(_channel.faultActive ? "true" : "false");
  s += "},";
  s += "\"note\":\"" + jsonEscape(_channel.note) + "\"";
  s += "}";

  s += "}";
  return s;
}

uint16_t InterfaceWiFiClass::sanitizeCsvLogMask(uint16_t mask) const
{
  mask &= IFW_CSV_ALL_MASK;
  if (mask == 0) {
    return IFW_CSV_DEFAULT_MASK;
  }
  return mask;
}

bool InterfaceWiFiClass::argBool(const char* name, bool fallback)
{
  if (!_server.hasArg(name)) return fallback;
  const String v = _server.arg(name);
  return v == "1" || v == "true" || v == "on" || v == "ON";
}

float InterfaceWiFiClass::argFloat(const char* name, float fallback, float lo, float hi)
{
  if (!_server.hasArg(name)) return fallback;
  return constrain(_server.arg(name).toFloat(), lo, hi);
}

uint16_t InterfaceWiFiClass::argU16(const char* name, uint16_t fallback, uint16_t lo, uint16_t hi)
{
  if (!_server.hasArg(name)) return fallback;
  int32_t value = _server.arg(name).toInt();
  if (value < (int32_t)lo) value = lo;
  if (value > (int32_t)hi) value = hi;
  return (uint16_t)value;
}

String InterfaceWiFiClass::getModeString() const
{
  if (!_active) return "OFF";
  if (WiFi.status() == WL_CONNECTED) return "STA";
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return "AP";
  if (_connectionState == CONN_CONNECTING) return "CONNECTING";
  return "OFF";
}

String InterfaceWiFiClass::getIpString() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

String InterfaceWiFiClass::getSSIDString() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return WiFi.softAPSSID();
  if (_connectionState == CONN_CONNECTING) return _savedSsid;
  return "--";
}

int32_t InterfaceWiFiClass::getRSSI() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.RSSI();
  return 0;
}
