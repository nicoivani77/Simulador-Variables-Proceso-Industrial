/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5 - diagnostico de entrada
 * Autor: Nicolas Emiliano Ivani
 *
 * Descripcion:
 *   Interfaz de la clase encargada de adquirir una entrada 4-20 mA con el
 *   ADC del ESP32-S3, filtrarla, calibrarla y convertirla a mA y porcentaje.
 *
 * Cadena de procesamiento:
 *   ADC -> promedio -> filtro IIR -> calibracion 4/20 mA -> mA -> porcentaje.
 *
 * La clase tambien permite capturar valores RAW para calibracion y realizar
 * diagnosticos de underrange, overrange y validez de los puntos calibrados.
 */

#ifndef LEER_4_20mA_H
#define LEER_4_20mA_H

#include <Arduino.h>

class Leer_4_20mA {
public:
  // Configuracion inicial del canal.
  // samplePeriodUs: periodo minimo entre actualizaciones.
  // filterShift: intensidad del filtro IIR; 0 lo desactiva.
  // samplesPerUpdate: cantidad de lecturas ADC promediadas por actualizacion.
  Leer_4_20mA(uint8_t pin,
              uint16_t rawAt4mA = 0,
              uint16_t rawAt20mA = 4095,
              uint32_t samplePeriodUs = 2000,
              uint8_t filterShift = 3,
              uint8_t samplesPerUpdate = 4);

  // Inicializacion y adquisicion periodica no bloqueante.
  void begin();
  bool update();

  // Parametros configurables en tiempo de ejecucion.
  void setCalibration(uint16_t rawAt4mA, uint16_t rawAt20mA);
  void setSamplePeriodUs(uint32_t samplePeriodUs);
  void setFilterShift(uint8_t filterShift);
  void setSamplesPerUpdate(uint8_t samplesPerUpdate);

  // Captura puntual promediada, utilizada principalmente durante calibracion.
  uint16_t captureRaw(uint16_t samples = 64);

  // Acceso a datos RAW y referencias de calibracion.
  uint8_t pin() const;
  uint16_t raw() const;
  uint16_t filteredRaw() const;
  uint16_t rawAt4mA() const;
  uint16_t rawAt20mA() const;

  // Conversiones de la senal calibrada.
  // currentmA(): corriente obtenida desde el RAW filtrado.
  // currentmAFromRaw(): convierte una cuenta ADC especifica a mA.
  // percent(): porcentaje limitado a 0...100 %.
  // percentUnclamped(): porcentaje sin saturar, util para diagnostico.
  float currentmA() const;
  float currentmAFromRaw(uint16_t raw) const;
  float percent() const;
  float percentUnclamped() const;

  // Diagnosticos:
  // underrange  -> corriente < 3.8 mA.
  // overrange   -> corriente > 20.2 mA.
  // calibracion valida -> referencias dentro de 12 bits, orden correcto
  //                       y separacion minima de 300 cuentas.
  bool underrange() const;
  bool overrange() const;
  bool hasValidCalibration() const;

  // Imprime RAW, filtrado, calibracion, mA, porcentajes y estados UR/OR
  // sobre cualquier salida compatible con Print, por ejemplo Serial.
  void printDebug(Print& out, const char* label = "AI") const;

private:
  // Configuracion del canal y calibracion.
  uint8_t _pin;
  uint16_t _rawAt4mA;
  uint16_t _rawAt20mA;
  uint32_t _samplePeriodUs;
  uint8_t _filterShift;
  uint8_t _samplesPerUpdate;

  // Estado interno de adquisicion y filtro.
  // _filteredQ8 usa punto fijo Q8: el entero real se almacena multiplicado por 256.
  uint16_t _raw;
  int32_t _filteredQ8;
  uint32_t _lastSampleUs;
  bool _started;
  bool _hasSample;

  // Utilidades internas: promedio de ADC y saturacion numerica.
  uint16_t sampleAveraged_();
  static float clampf_(float x, float minValue, float maxValue);
};

#endif
