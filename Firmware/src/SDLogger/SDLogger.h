/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 *
 * Descripcion:
 *   Interfaz del sistema de registro de datos en tarjeta microSD mediante SPI.
 *
 * La clase permite:
 *   - montar y verificar la SD;
 *   - registrar muestras en formato CSV;
 *   - seleccionar columnas mediante una mascara de bits;
 *   - guardar la calibracion RAW de entrada;
 *   - consultar el estado y el ultimo error.
 *
 * La seleccion de columnas se representa con FieldMask: cada variable ocupa
 * un bit independiente, por lo que varias columnas se habilitan combinando
 * sus constantes mediante OR binario.
 */

#ifndef SDLOGGER_H
#define SDLOGGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class SDLogger {
public:

  // Mascara de campos disponibles para el archivo CSV.
  // Cada constante ocupa un bit distinto para poder combinar selecciones.
  //
  // IMPORTANTE:
  // LOG_FIELD_INPUT_MA y LOG_FIELD_OUTPUT_MA estan definidos individualmente,
  // pero actualmente no forman parte de LOG_FIELD_ALL ni se escriben en las
  // funciones de encabezado/muestra. Se mantienen disponibles en la interfaz.
  enum FieldMask : uint16_t {
    LOG_FIELD_MS                 = 1 << 0,
    LOG_FIELD_INPUT_MA           = 1 << 1,
    LOG_FIELD_INPUT_PCT          = 1 << 2,
    LOG_FIELD_OUTPUT_MA          = 1 << 3,
    LOG_FIELD_OUTPUT_PCT         = 1 << 4,
    LOG_FIELD_OUTPUT_ENABLED     = 1 << 5,
    LOG_FIELD_MODE               = 1 << 6,
    LOG_FIELD_PRESET             = 1 << 7,
    LOG_FIELD_PROCESS_K          = 1 << 8,
    LOG_FIELD_PROCESS_TAU        = 1 << 9,
    LOG_FIELD_PROCESS_DEADTIME   = 1 << 10,
    LOG_FIELD_SENSOR_NOISE       = 1 << 11,
    LOG_FIELD_SENSOR_OFFSET      = 1 << 12,
    LOG_FIELD_ERRORS             = 1 << 13,
    LOG_FIELD_OUTPUT_CALIBRATION = 1 << 14,

    // Conjunto de columnas actualmente habilitables desde la seleccion general.
    LOG_FIELD_ALL =
      LOG_FIELD_MS |
      LOG_FIELD_INPUT_PCT |
      LOG_FIELD_OUTPUT_PCT |
      LOG_FIELD_OUTPUT_ENABLED |
      LOG_FIELD_MODE |
      LOG_FIELD_PRESET |
      LOG_FIELD_PROCESS_K |
      LOG_FIELD_PROCESS_TAU |
      LOG_FIELD_PROCESS_DEADTIME |
      LOG_FIELD_SENSOR_NOISE |
      LOG_FIELD_SENSOR_OFFSET |
      LOG_FIELD_ERRORS |
      LOG_FIELD_OUTPUT_CALIBRATION,

    // Configuracion inicial: utiliza todas las columnas incluidas en LOG_FIELD_ALL.
    LOG_FIELD_DEFAULT = LOG_FIELD_ALL
  };

  // Construye el logger con pinout SPI, ruta y frecuencia por defecto.
  SDLogger();

  // Inicializa SPI, monta la tarjeta y crea el encabezado si es necesario.
  // Pinout por defecto:
  //   SCK=12, MISO=13, MOSI=11, CS=10
  // Frecuencia SPI por defecto: 10 MHz.
  bool begin(uint8_t sckPin = 12,
             uint8_t misoPin = 13,
             uint8_t mosiPin = 11,
             uint8_t csPin = 10,
             const char* logPath = "/sim_log.csv",
             uint32_t spiFrequency = 10000000UL);

  // Estado y diagnostico.
  bool isReady() const;
  const char* lastError() const;
  const char* logPath() const;

  // Seleccion dinamica de columnas CSV.
  // recreateFile=true elimina el CSV actual y genera un nuevo encabezado.
  bool setFieldMask(uint16_t mask, bool recreateFile = false);
  uint16_t fieldMask() const;

  // Prueba fisica de escritura sobre la SD.
  bool testWrite();

  // Agrega una muestra al CSV. Los campos realmente escritos dependen de fieldMask().
  bool logSample(uint32_t ms,
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
                 bool outputFromCalibration);

  // Guarda los dos puntos RAW de calibracion en /cal_input.csv.
  bool writeCalibration(uint16_t raw4mA, uint16_t raw20mA);

  // Finaliza la tarjeta SD y deshabilita el logger.
  void end();

private:
  // Configuracion del bus SPI.
  uint8_t _sckPin;
  uint8_t _misoPin;
  uint8_t _mosiPin;
  uint8_t _csPin;
  uint32_t _spiFrequency;

  // Estado interno del logger.
  char _logPath[32];
  char _lastError[48];
  bool _ready;
  uint16_t _fieldMask;

  // Utilidades internas para diagnostico, creacion del archivo y seleccion de campos.
  void setError(const char* error);
  bool ensureHeader();
  uint16_t sanitizeFieldMask(uint16_t mask) const;
  // Escritura del encabezado y de cada fila con el mismo orden de columnas.
  void printSelectedHeader(File& f);
  void printSelectedSample(File& f,
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
                           bool outputFromCalibration);
};

#endif
