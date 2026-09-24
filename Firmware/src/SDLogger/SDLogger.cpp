/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 *
 * Descripcion:
 *   Implementacion del registro de variables del simulador en archivos CSV
 *   almacenados sobre una tarjeta microSD conectada por SPI.
 *
 * Funciones principales:
 *   - Inicializar y verificar la tarjeta SD.
 *   - Crear el archivo CSV y su encabezado.
 *   - Seleccionar dinamicamente las columnas mediante una mascara de bits.
 *   - Agregar muestras sin reescribir el archivo completo.
 *   - Guardar los puntos RAW de calibracion de la entrada.
 *   - Mantener un texto de diagnostico con el ultimo estado/error.
 *
 * El archivo principal de registro es /sim_log.csv por defecto.
 * Cada llamada a logSample() abre el archivo en modo append, agrega una fila
 * y lo vuelve a cerrar para reducir el riesgo de perder datos ante un reinicio.
 */

#include "SDLogger.h"
#include <FS.h>
#include <stdio.h>
#include <string.h>


// Escribe texto de forma segura dentro de una celda CSV.
// Siempre encierra el contenido entre comillas dobles.
// Si encuentra una comilla interna, la duplica segun el formato CSV.
// Los saltos de linea se reemplazan por espacios para no generar filas falsas.
static void printCsvText(File& f, const char* value) {
  if (!value) value = "";

  f.print('"');
  for (const char* p = value; *p; ++p) {
    if (*p == '"') {
      f.print("\"\"");
    } else if (*p == '\n' || *p == '\r') {
      f.print(' ');
    } else {
      f.print(*p);
    }
  }
  f.print('"');
}

// Representa un booleano en el CSV como texto legible: "SI" o "NO".
static void printSiNo(File& f, bool value) {
  f.print(value ? "SI" : "NO");
}

// Constructor: establece los pines SPI, frecuencia, archivo y mascara por defecto.
// El logger comienza marcado como no disponible hasta que begin() monte la SD.
SDLogger::SDLogger()
: _sckPin(12),
  _misoPin(13),
  _mosiPin(11),
  _csPin(10),
  _spiFrequency(10000000UL),
  _ready(false),
  _fieldMask(LOG_FIELD_DEFAULT) {
  strncpy(_logPath, "/sim_log.csv", sizeof(_logPath));
  _logPath[sizeof(_logPath) - 1] = '\0';
  setError("Sin iniciar");
}

// Inicializa el bus SPI y monta la tarjeta SD.
//
// Secuencia:
//   1) Guarda pines, frecuencia y ruta del archivo.
//   2) Inicializa SPI con el pinout indicado.
//   3) Ejecuta SD.begin().
//   4) Verifica que realmente exista una tarjeta.
//   5) Marca el logger como listo.
//   6) Crea el encabezado CSV si el archivo todavia no existe.
//
// Retorna true solamente si todo el proceso finaliza correctamente.
bool SDLogger::begin(uint8_t sckPin,
                     uint8_t misoPin,
                     uint8_t mosiPin,
                     uint8_t csPin,
                     const char* logPath,
                     uint32_t spiFrequency) {
  _sckPin = sckPin;
  _misoPin = misoPin;
  _mosiPin = mosiPin;
  _csPin = csPin;
  _spiFrequency = spiFrequency;

  if (logPath && logPath[0]) {
    strncpy(_logPath, logPath, sizeof(_logPath));
    _logPath[sizeof(_logPath) - 1] = '\0';
  }

  SPI.begin(_sckPin, _misoPin, _mosiPin, _csPin);

  if (!SD.begin(_csPin, SPI, _spiFrequency)) {
    _ready = false;
    setError("No inicia SD");
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    _ready = false;
    setError("Sin tarjeta SD");
    return false;
  }

  _ready = true;
  setError("OK");
  return ensureHeader();
}

// Indica si la tarjeta fue inicializada correctamente y el logger esta operativo.
bool SDLogger::isReady() const {
  return _ready;
}

// Devuelve el ultimo mensaje interno de estado o error.
const char* SDLogger::lastError() const {
  return _lastError;
}

// Devuelve la ruta actual del archivo CSV principal.
const char* SDLogger::logPath() const {
  return _logPath;
}

// Cambia la mascara de columnas que se incluiran en el CSV.
// Cada bit de la mascara corresponde a un campo definido en FieldMask.
//
// Si recreateFile=true y la SD esta lista:
//   - elimina el archivo de registro actual;
//   - lo vuelve a crear con un encabezado compatible con la nueva seleccion.
//
// Esto evita que un archivo conserve un encabezado viejo mientras las nuevas
// filas contienen un conjunto de columnas diferente.
bool SDLogger::setFieldMask(uint16_t mask, bool recreateFile) {
  _fieldMask = sanitizeFieldMask(mask);

  if (_ready && recreateFile) {
    SD.remove(_logPath);
    return ensureHeader();
  }

  return true;
}

// Devuelve la mascara de campos actualmente activa.
uint16_t SDLogger::fieldMask() const {
  return _fieldMask;
}

// Prueba simple de escritura sobre la tarjeta.
// Agrega una linea a /sd_test.txt con el tiempo desde el arranque.
// Sirve para distinguir un problema de montaje de uno de escritura real.
bool SDLogger::testWrite() {
  if (!_ready) {
    setError("SD no lista");
    return false;
  }

  File f = SD.open("/sd_test.txt", FILE_APPEND);
  if (!f) {
    setError("No abre test");
    return false;
  }

  f.print("Test SD OK, ms=");
  f.println(millis());
  f.close();
  setError("Test OK");
  return true;
}

// Agrega una muestra al archivo CSV principal.
//
// Recibe el estado completo del simulador, pero printSelectedSample() escribe
// solamente los campos habilitados en _fieldMask.
//
// El archivo se abre con FILE_APPEND para preservar las muestras anteriores.
// Luego de escribir la fila se cierra inmediatamente.
bool SDLogger::logSample(uint32_t ms,
                         float inputmA,
                         float inputPct,
                         float outputmA,
                         float outputPct,
                         bool outputEnabled,
                         bool manualMode,
                         const char* presetName,
                         float processK,
                         float processTau,
                         float processDeadTime,
                         float sensorNoisePct,
                         float sensorOffsetPct,
                         const char* errorText,
                         bool outputFromCalibration) {
  if (!_ready) {
    setError("SD no lista");
    return false;
  }

  File f = SD.open(_logPath, FILE_APPEND);
  if (!f) {
    setError("No abre log");
    return false;
  }

  printSelectedSample(f,
                      ms,
                      inputmA,
                      inputPct,
                      outputmA,
                      outputPct,
                      outputEnabled,
                      manualMode,
                      presetName,
                      processK,
                      processTau,
                      processDeadTime,
                      sensorNoisePct,
                      sensorOffsetPct,
                      errorText,
                      outputFromCalibration);
  f.close();

  setError("Log OK");
  return true;
}

// Guarda los dos puntos RAW de calibracion de la entrada 4-20 mA.
// El archivo /cal_input.csv se escribe con un encabezado simple:
//   RAW_4_mA, RAW_20_mA
//
// Se usa FILE_WRITE porque este archivo representa la calibracion vigente,
// no un historial acumulativo de calibraciones.
bool SDLogger::writeCalibration(uint16_t raw4mA, uint16_t raw20mA) {
  if (!_ready) {
    setError("SD no lista");
    return false;
  }

  File f = SD.open("/cal_input.csv", FILE_WRITE);
  if (!f) {
    setError("No abre cal");
    return false;
  }

  f.println("RAW_4_mA,RAW_20_mA");
  f.print(raw4mA);
  f.print(',');
  f.println(raw20mA);
  f.close();

  setError("Cal OK");
  return true;
}

// Finaliza el acceso a la tarjeta SD y deja el logger marcado como no disponible.
void SDLogger::end() {
  if (_ready) {
    SD.end();
  }
  _ready = false;
  setError("SD detenida");
}

// Actualiza el texto de diagnostico interno.
// strncpy() y la terminacion manual aseguran que el buffer siempre quede
// terminado en '\0', incluso si el mensaje recibido supera su capacidad.
void SDLogger::setError(const char* error) {
  if (!error) error = "";
  strncpy(_lastError, error, sizeof(_lastError));
  _lastError[sizeof(_lastError) - 1] = '\0';
}

// Garantiza que exista el archivo de registro con su encabezado.
//
// Si el archivo ya existe, no lo modifica.
// Si no existe, lo crea y escribe el encabezado correspondiente a _fieldMask.
// Un fallo al crear el archivo deja _ready=false porque el logger no puede operar.
bool SDLogger::ensureHeader() {
  if (!_ready) return false;

  if (SD.exists(_logPath)) {
    setError("OK");
    return true;
  }

  File f = SD.open(_logPath, FILE_WRITE);
  if (!f) {
    setError("No crea log");
    _ready = false;
    return false;
  }

  printSelectedHeader(f);
  f.close();
  setError("OK");
  return true;
}


// Limpia una mascara recibida desde configuracion externa.
// Elimina bits que no pertenecen a LOG_FIELD_ALL.
// Si luego del filtrado no queda ningun campo seleccionado, recupera
// LOG_FIELD_DEFAULT para evitar generar un CSV completamente vacio.
uint16_t SDLogger::sanitizeFieldMask(uint16_t mask) const {
  mask &= LOG_FIELD_ALL;
  if (mask == 0) {
    return LOG_FIELD_DEFAULT;
  }
  return mask;
}

// Escribe la primera fila del CSV segun los bits activos en _fieldMask.
//
// La lambda printName() administra automaticamente las comas:
//   - el primer nombre se escribe sin separador previo;
//   - los siguientes se escriben precedidos por ','.
//
// Los nombres corresponden directamente a los parámetros utilizados por el modelo.
// K se registra como ganancia adimensional y tau como constante de tiempo en segundos.
void SDLogger::printSelectedHeader(File& f) {
  bool first = true;

  auto printName = [&](const char* name) {
    if (!first) f.print(',');
    f.print(name);
    first = false;
  };

  if (_fieldMask & LOG_FIELD_MS) printName("Tiempo_ms");
  if (_fieldMask & LOG_FIELD_INPUT_PCT) printName("Entrada_%");
  if (_fieldMask & LOG_FIELD_OUTPUT_PCT) printName("Salida_%");
  if (_fieldMask & LOG_FIELD_OUTPUT_ENABLED) printName("Salida_activa");
  if (_fieldMask & LOG_FIELD_MODE) printName("Modo");
  if (_fieldMask & LOG_FIELD_PRESET) printName("Preset");
  if (_fieldMask & LOG_FIELD_PROCESS_K) printName("Ganancia_K");
  if (_fieldMask & LOG_FIELD_PROCESS_TAU) printName("Tau_s");
  if (_fieldMask & LOG_FIELD_PROCESS_DEADTIME) printName("Retardo_s");
  if (_fieldMask & LOG_FIELD_SENSOR_NOISE) printName("Ruido_%");
  if (_fieldMask & LOG_FIELD_SENSOR_OFFSET) printName("Offset_%");
  if (_fieldMask & LOG_FIELD_ERRORS) printName("Errores");
  if (_fieldMask & LOG_FIELD_OUTPUT_CALIBRATION) printName("Salida_por_calibracion");

  f.println();
}

// Escribe una fila de datos respetando exactamente el mismo orden utilizado
// por printSelectedHeader().
//
// La lambda sep() administra las comas entre columnas.
// Los parametros numericos se formatean con una cantidad fija de decimales,
// los booleanos se convierten a SI/NO y los textos se escapan como CSV.
//
// Aunque logSample() recibe inputmA y outputmA, las mascaras activas actuales
// LOG_FIELD_ALL/DEFAULT no incluyen esos campos; se conservan en la firma
// para mantener disponible esa informacion dentro de la interfaz del logger.
void SDLogger::printSelectedSample(File& f,
                                   uint32_t ms,
                                   float inputmA,
                                   float inputPct,
                                   float outputmA,
                                   float outputPct,
                                   bool outputEnabled,
                                   bool manualMode,
                                   const char* presetName,
                                   float processK,
                                   float processTau,
                                   float processDeadTime,
                                   float sensorNoisePct,
                                   float sensorOffsetPct,
                                   const char* errorText,
                                   bool outputFromCalibration) {
  bool first = true;

  auto sep = [&]() {
    if (!first) f.print(',');
    first = false;
  };

  if (_fieldMask & LOG_FIELD_MS) {
    sep();
    f.print(ms);
  }


  if (_fieldMask & LOG_FIELD_INPUT_PCT) {
    sep();
    f.print(inputPct, 2);
  }


  if (_fieldMask & LOG_FIELD_OUTPUT_PCT) {
    sep();
    f.print(outputPct, 2);
  }

  if (_fieldMask & LOG_FIELD_OUTPUT_ENABLED) {
    sep();
    printSiNo(f, outputEnabled);
  }

  if (_fieldMask & LOG_FIELD_MODE) {
    sep();
    printCsvText(f, manualMode ? "Manual" : "Automatico");
  }

  if (_fieldMask & LOG_FIELD_PRESET) {
    sep();
    printCsvText(f, presetName);
  }

  if (_fieldMask & LOG_FIELD_PROCESS_K) {
    sep();
    f.print(processK, 3);
  }

  if (_fieldMask & LOG_FIELD_PROCESS_TAU) {
    sep();
    f.print(processTau, 3);
  }

  if (_fieldMask & LOG_FIELD_PROCESS_DEADTIME) {
    sep();
    f.print(processDeadTime, 3);
  }

  if (_fieldMask & LOG_FIELD_SENSOR_NOISE) {
    sep();
    f.print(sensorNoisePct, 2);
  }

  if (_fieldMask & LOG_FIELD_SENSOR_OFFSET) {
    sep();
    f.print(sensorOffsetPct, 2);
  }

  if (_fieldMask & LOG_FIELD_ERRORS) {
    sep();
    printCsvText(f, errorText && errorText[0] ? errorText : "Sin errores");
  }

  if (_fieldMask & LOG_FIELD_OUTPUT_CALIBRATION) {
    sep();
    printSiNo(f, outputFromCalibration);
  }

  f.println();
}
