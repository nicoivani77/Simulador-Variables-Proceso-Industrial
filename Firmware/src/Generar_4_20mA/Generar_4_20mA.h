/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 *
 * Módulo: Interfaz de configuración y control de la salida 4-20 mA.
 *
 * La clase Generar_4_20mALib concentra:
 *   - La comunicación I2C con el DAC.
 *   - La calibración de la salida 4-20 mA.
 *   - El modelo de proceso de primer orden G(s) = K / (tau*s + 1).
 *   - El tiempo muerto implementado como retardo discreto.
 *   - El modelo de sensor: ruido, offset y fallas simuladas.
 *
 * La cabecera declara la configuración, API pública y estado interno.
 * La implementación matemática y las escrituras al hardware están en Generar_4_20mA.cpp.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

class Generar_4_20mALib {
public:
  // Modos de falla que puede imponer el modelo de sensor sobre la señal calculada.
  // NONE deja pasar la medición; los demás modos alteran la salida para ensayar diagnósticos.
  enum SensorFaultMode : uint8_t {
    SENSOR_FAULT_NONE = 0,        // Sin falla forzada.
    SENSOR_FAULT_OFFSET,          // Estado lógico asociado a un desplazamiento de la medición.
    SENSOR_FAULT_DISCONNECTED,    // Fuerza la corriente configurada como desconexión, normalmente 0 mA.
    SENSOR_FAULT_FROZEN,          // Mantiene fija la primera medición tomada al activar la falla.
    SENSOR_FAULT_STUCK_MIN,       // Fuerza la medición al porcentaje mínimo configurado.
    SENSOR_FAULT_STUCK_MAX        // Fuerza la medición al porcentaje máximo configurado.
  };

  struct Config {
    // ===== I2C / DAC =====
    // Bus, dirección y velocidad utilizados para comunicarse con el DAC externo.
    TwoWire* wire = &Wire;
    uint8_t i2cAddr = 0x60;
    int sdaPin = 8;
    int sclPin = 9;
    uint32_t i2cClock = 400000;

    // ===== Rangos eléctricos =====
    // Rangos usados para convertir entre corriente y porcentaje de escala.
    float inputMinmA  = 4.0f;
    float inputMaxmA  = 20.0f;
    float outputMinmA = 4.0f;
    float outputMaxmA = 20.0f;

    // ===== Calibración DAC =====
    // Calibración lineal de dos puntos: RAW medido para el mínimo y máximo de corriente.
    uint16_t rawAtOutputMin = 440;    // Cuenta DAC asociada a 4 mA.
    uint16_t rawAtOutputMax = 4000;   // Cuenta DAC asociada a 20 mA.

    // ===== Función de transferencia =====
    // Modelo continuo utilizado: G(s) = K / (tau*s + 1).
    // K es la ganancia estática, tau_s la constante de tiempo y sampleTimeMs el período de cálculo.
    float K = 1.0f;
    float tau_s = 2.0f;
    uint32_t sampleTimeMs = 50;

    // Tiempo muerto del proceso.
    // Se convierte a N muestras según sampleTimeMs y se aplica como u[k-N] mediante un buffer circular.
    float deadTime_s = 0.0f;

    // ===== Modelo de sensor / medición =====
    // Amplitud máxima del ruido uniforme sumado a la medición, expresada en % de escala.
    // Ejemplo: 0.5 genera un término aleatorio entre -0.5 % y +0.5 %.
    float outputNoisePct = 0.0f;

    // Offset aditivo del sensor en % de escala. Puede ser positivo o negativo.
    float sensorOffsetPct = 0.0f;

    // Falla forzada sobre la medición simulada. SENSOR_FAULT_NONE deja operar normalmente.
    SensorFaultMode sensorFaultMode = SENSOR_FAULT_NONE;

    // Corriente enviada cuando se simula SENSOR_FAULT_DISCONNECTED.
    // El valor 0 mA representa idealmente un lazo abierto.
    float sensorDisconnectmA = 0.0f;

    // Porcentajes utilizados por las fallas de sensor trabado en mínimo o máximo.
    float sensorStuckMinPct = 0.0f;
    float sensorStuckMaxPct = 100.0f;

    // ===== Estado inicial / límites del modelo =====
    // y0Pct fija la condición inicial. Los límites pueden saturar la salida interna del proceso.
    float y0Pct = 0.0f;
    float processMinPct = 0.0f;
    float processMaxPct = 100.0f;
    bool clampProcessOutput = true;
  };

  // Constructor con configuración por defecto.
  Generar_4_20mALib();
  // Constructor con una configuración suministrada por el programa principal.
  explicit Generar_4_20mALib(const Config& cfg);

  // Inicializa el bus I2C, verifica que el DAC responda y reinicia el modelo.
  // Devuelve true únicamente si el DAC queda disponible.
  bool begin();
  // Reinicia estados dinámicos, buffer de retardo y falla de congelamiento.
  // y0Pct define la condición inicial de la salida del proceso.
  void reset(float y0Pct = 0.0f);

  // Actualización del modo automático.
  // Mantiene un paso discreto fijo sampleTimeMs mediante temporización acumulativa:
  // si el loop llega tarde, ejecuta los pasos pendientes sin perder tiempo simulado.
  // Convierte la entrada en mA a %, aplica tiempo muerto y dinámica de primer orden,
  // pasa el resultado por el modelo de sensor y finalmente actualiza el DAC.
  // Devuelve false si todavía no transcurrió sampleTimeMs o si falla la escritura.
  bool updateFromInputmA(float inputmA);

  // Salida manual.
  // No aplica dinámica de proceso ni tiempo muerto, pero sí aplica ruido, offset y fallas
  // del modelo de sensor antes de escribir la salida física.
  bool setOutputmA(float mA);
  bool setOutputPercent(float percent);

  // Escritura directa en unidades físicas.
  // Omite dinámica, tiempo muerto y modelo de sensor; conserva únicamente la conversión
  // calibrada %/mA -> RAW. Se usa cuando se necesita imponer un valor sin simulación.
  bool setOutputmADirect(float mA);
  bool setOutputPercentDirect(float percent);

  // Escritura RAW directa para diagnóstico y calibración del DAC.
  // El valor se limita al rango de 12 bits: 0...4095.
  bool setOutputRawDirect(uint16_t raw);

  // Desactiva físicamente la salida escribiendo RAW=0.
  // No debe reemplazarse por setOutputPercent(0), porque 0 % equivale a 4 mA.
  // También libera el valor memorizado por una eventual falla de sensor congelado.
  bool setOutputOff();

  // ===== Configuración en tiempo de ejecución =====
  // Estos setters modifican calibración, rangos y parámetros del modelo.
  // Cambiar K/tau/tiempo de muestreo o tiempo muerto obliga a recalcular estados asociados.
  void setCalibrationRaw(uint16_t rawMin, uint16_t rawMax);
  void setInputRange(float minmA, float maxmA);
  void setOutputRange(float minmA, float maxmA);
  void setTransferFunction(float K, float tau_s, uint32_t sampleTimeMs);
  void setDeadTime(float deadTime_s);
  void setOutputNoise(float noisePct);
  void setSensorOffset(float offsetPct);
  void setSensorFaultMode(SensorFaultMode mode);
  void setSensorDisconnectmA(float mA);
  void setSensorStuckPercent(float minPct, float maxPct);

  // ===== Variables para monitoreo y diagnóstico =====
  // Permiten consultar el último estado calculado sin modificar el modelo.
  float getLastInputmA() const;
  float getLastInputPct() const;

  // Salida interna del proceso antes de introducir ruido, offset o fallas del sensor.
  float getLastProcessPct() const;

  // Valores finales de la salida después del modelo de sensor y de la conversión al DAC.
  float getLastOutputPct() const;
  float getLastOutputmA() const;
  uint16_t getLastOutputRaw() const;
  uint16_t getRawAtOutputMin() const;
  uint16_t getRawAtOutputMax() const;

  float getDeadTime() const;
  uint16_t getDelaySamples() const;
  float getOutputNoise() const;
  float getSensorOffset() const;
  SensorFaultMode getSensorFaultMode() const;
  const char* getSensorFaultText() const;
  bool hasSensorFault() const;
  bool isReady() const;

private:
  // Capacidad fija del buffer circular usado para el tiempo muerto.
  // Se necesitan 601 posiciones para representar N = 600, porque la muestra actual
  // se escribe antes de leer la posición retardada. Con Ts = 50 ms, N = 600 equivale a 30.0 s.
  static const uint16_t MAX_DELAY_SAMPLES = 601;

  // ===== Acceso al DAC =====
  // probeDAC comprueba presencia I2C; writeDACFast escribe 12 bits;
  // writeSensorOutput convierte la salida del sensor a corriente/RAW y la envía.
  bool probeDAC();
  bool writeDACFast(uint16_t raw);
  bool writeSensorOutput(float sensorPct);

  // ===== Conversión y utilidades =====
  // Funciones auxiliares de saturación y conversión entre %, mA y cuentas DAC.
  float clampf(float x, float lo, float hi) const;
  float mAToPercent(float mA, float minmA, float maxmA) const;
  float percentToMA(float pct, float minmA, float maxmA) const;
  uint16_t currentToRaw(float mA) const;
  uint16_t currentToRawExtended(float mA) const;

  // ===== Modelo dinámico y sensor =====
  // Recalcula la discretización, administra el retardo y aplica el modelo de medición.
  void recomputeDiscreteTF();
  void recomputeDelaySamples();
  void resetDelayBuffer(float valuePct);
  float pushAndGetDelayedInput(float inputPct);
  float applySensorModel(float processPct);
  float randomNoisePct() const;

private:
  // Copia local de todos los parámetros de funcionamiento de la biblioteca.
  Config _cfg;

  // Coeficientes de la ecuación discreta:
  // y[k] = a*y[k-1] + b*u[k-N].
  float _a = 0.0f;
  float _b = 1.0f;

  // Últimos valores calculados. Se conservan para monitoreo y para el siguiente paso del modelo.
  float _lastInputmA = 4.0f;
  float _lastInputPct = 0.0f;
  float _lastDelayedInputPct = 0.0f;
  float _lastProcessPct = 0.0f;
  float _lastOutputPct = 0.0f;
  float _lastOutputmA = 4.0f;
  uint16_t _lastOutputRaw = 0;

  // Buffer circular de entrada para implementar u[k-N].
  // _delaySamples es el retardo expresado en muestras y _delayIndex es la posición de escritura.
  float _delayBuffer[MAX_DELAY_SAMPLES];
  uint16_t _delaySamples = 0;
  uint16_t _delayIndex = 0;

  // Estado de la falla FROZEN. Al activarse, captura una muestra una sola vez y la mantiene.
  bool _freezeLatched = false;
  float _frozenSensorPct = 0.0f;

  // Referencia temporal acumulativa del modelo discreto y estado del DAC.
  // _lastUpdateMs avanza únicamente en múltiplos exactos de sampleTimeMs para
  // conservar el tiempo sobrante entre llamadas y evitar deriva temporal.
  uint32_t _lastUpdateMs = 0;
  bool _ready = false;
};
