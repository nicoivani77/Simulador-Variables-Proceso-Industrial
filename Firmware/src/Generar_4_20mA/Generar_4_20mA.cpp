/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 *
 * Módulo: Generación de señal 4-20 mA y simulación del proceso.
 *
 * Este archivo implementa tres etapas principales:
 *   1) Modelo dinámico del proceso: G(s) = K / (tau*s + 1), discretizado en el tiempo.
 *   2) Tiempo muerto: retardo de la entrada mediante un buffer circular de muestras.
 *   3) Modelo de sensor y salida: ruido/offset/fallas, conversión % -> mA -> RAW y escritura al DAC.
 *
 * Flujo en modo automático:
 *   Entrada 4-20 mA -> % -> tiempo muerto -> proceso de 1er orden
 *   -> modelo de sensor -> corriente de salida -> RAW DAC -> MCP4725.
 *
 * Importante:
 *   - 0 % de escala representa 4 mA, no una salida apagada.
 *   - setOutputOff() fuerza RAW = 0 para representar el estado físico de salida desactivada.
 */

#include "Generar_4_20mA.h"
#include <math.h>

// Constructor por defecto.
// Carga Config con sus valores iniciales y deja preparados los coeficientes y el buffer de retardo.
// No inicializa todavía el hardware I2C; eso se realiza en begin().
Generar_4_20mALib::Generar_4_20mALib() : _cfg() {
  recomputeDiscreteTF();
  recomputeDelaySamples();
  resetDelayBuffer(0.0f);
}

// Constructor con configuración externa.
// Copia cfg y calcula desde el inicio la discretización y la cantidad de muestras de retardo.
Generar_4_20mALib::Generar_4_20mALib(const Config& cfg) : _cfg(cfg) {
  recomputeDiscreteTF();
  recomputeDelaySamples();
  resetDelayBuffer(0.0f);
}

// Inicialización del módulo.
// Configura I2C, comprueba que el DAC responda en su dirección y reinicia el modelo.
// Si el bus no fue configurado o el DAC no responde, devuelve false.
bool Generar_4_20mALib::begin() {
  if (_cfg.wire == nullptr) return false;

  _cfg.wire->begin(_cfg.sdaPin, _cfg.sclPin);
  _cfg.wire->setClock(_cfg.i2cClock);

  _ready = probeDAC();
  if (!_ready) return false;

  reset(_cfg.y0Pct);
  return true;
}

// Reinicia los estados dinámicos sin cambiar la configuración.
// La entrada vuelve al mínimo del rango, el proceso parte de y0Pct y el buffer de retardo
// se llena con la entrada inicial. También se libera cualquier congelamiento previo del sensor.
void Generar_4_20mALib::reset(float y0Pct) {
  _lastInputmA = _cfg.inputMinmA;
  _lastInputPct = 0.0f;
  _lastDelayedInputPct = 0.0f;
  _lastProcessPct = clampf(y0Pct, _cfg.processMinPct, _cfg.processMaxPct);

  _freezeLatched = false;
  _frozenSensorPct = _lastProcessPct;

  resetDelayBuffer(_lastInputPct);

  const float sensorPct = applySensorModel(_lastProcessPct);
  writeSensorOutput(sensorPct);

  _lastUpdateMs = millis();
}

// Paso principal del modo automático.
//
// El modelo trabaja con un período discreto fijo sampleTimeMs. El reloj se mantiene
// acumulativo: si el loop() llega tarde, no se descarta el tiempo sobrante.
// En ese caso se ejecutan todos los pasos discretos pendientes antes de actualizar
// físicamente el DAC.
//
// Esto evita que una carga variable del loop (display, WiFi, SD, etc.) haga que la
// constante de tiempo y el retardo resulten mayores que los valores configurados.
//
// Flujo de cada paso discreto:
//   mA de entrada -> % -> retardo u[k-N] -> proceso de primer orden.
//
// El modelo de sensor y la escritura al DAC se aplican una sola vez, después de
// completar los pasos pendientes, para evitar escrituras I2C innecesarias.
bool Generar_4_20mALib::updateFromInputmA(float inputmA) {
  if (!_ready) return false;

  const uint32_t now = millis();
  const uint32_t elapsedMs = (uint32_t)(now - _lastUpdateMs);

  if (elapsedMs < _cfg.sampleTimeMs) {
    return false;
  }

  // Cantidad de pasos discretos de Ts que deben ejecutarse para mantener
  // sincronizado el tiempo simulado con el tiempo real transcurrido.
  const uint32_t pendingSteps = elapsedMs / _cfg.sampleTimeMs;

  // Avanza el reloj por múltiplos exactos de Ts. No se asigna "now" para
  // conservar el resto temporal y evitar deriva acumulativa.
  _lastUpdateMs += pendingSteps * _cfg.sampleTimeMs;

  // Limita la entrada al rango configurado antes de normalizarla.
  // Si hubo más de un paso pendiente, se utiliza la última entrada disponible
  // para completar esos pasos; el firmware no puede reconstruir muestras que
  // no fueron adquiridas durante un bloqueo del loop.
  _lastInputmA = clampf(inputmA, _cfg.inputMinmA, _cfg.inputMaxmA);
  _lastInputPct = mAToPercent(_lastInputmA, _cfg.inputMinmA, _cfg.inputMaxmA);

  for (uint32_t i = 0; i < pendingSteps; ++i) {
    // Recupera la muestra de entrada correspondiente al tiempo muerto configurado.
    _lastDelayedInputPct = pushAndGetDelayedInput(_lastInputPct);

    // Ecuación de estado del proceso discreto.
    // _lastDelayedInputPct ya contiene u[k-N].
    _lastProcessPct = (_a * _lastProcessPct) + (_b * _lastDelayedInputPct);

    if (_cfg.clampProcessOutput) {
      _lastProcessPct = clampf(_lastProcessPct, _cfg.processMinPct, _cfg.processMaxPct);
    }
  }

  // El modelo de sensor es posterior al proceso: representa lo que finalmente
  // mediría/transmitiría el instrumento. Se evalúa una sola vez por llamada.
  const float sensorPct = applySensorModel(_lastProcessPct);
  return writeSensorOutput(sensorPct);
}

// Fija manualmente la salida del proceso en porcentaje.
// Omite la dinámica de primer orden y el tiempo muerto, pero conserva el modelo de sensor.
// Por eso ruido, offset y fallas también pueden observarse durante el modo manual.
bool Generar_4_20mALib::setOutputPercent(float percent) {
  if (!_ready) return false;

  // La variable de proceso se fuerza al valor manual y luego se simula la medición del sensor.
  percent = clampf(percent, 0.0f, 100.0f);
  _lastProcessPct = percent;

  const float sensorPct = applySensorModel(_lastProcessPct);
  return writeSensorOutput(sensorPct);
}

// Variante manual expresada en mA.
// Convierte primero la corriente al porcentaje equivalente y reutiliza setOutputPercent().
bool Generar_4_20mALib::setOutputmA(float mA) {
  mA = clampf(mA, _cfg.outputMinmA, _cfg.outputMaxmA);
  const float percent = mAToPercent(mA, _cfg.outputMinmA, _cfg.outputMaxmA);
  return setOutputPercent(percent);
}

// Escritura directa en porcentaje.
// Convierte % -> mA y deriva a setOutputmADirect(), sin pasar por dinámica ni modelo de sensor.
bool Generar_4_20mALib::setOutputPercentDirect(float percent) {
  if (!_ready) return false;

  percent = clampf(percent, 0.0f, 100.0f);
  const float mA = percentToMA(percent, _cfg.outputMinmA, _cfg.outputMaxmA);
  return setOutputmADirect(mA);
}

// Escritura directa en mA.
// Aplica únicamente límites y calibración mA -> RAW, escribe el DAC y actualiza variables de estado.
// Es un bypass de la simulación del proceso y del sensor.
bool Generar_4_20mALib::setOutputmADirect(float mA) {
  if (!_ready) return false;

  mA = clampf(mA, _cfg.outputMinmA, _cfg.outputMaxmA);
  // Interpolación lineal entre las dos cuentas RAW de calibración.
  const uint16_t raw = currentToRaw(mA);

  if (!writeDACFast(raw)) return false;

  _lastOutputRaw = raw;
  _lastOutputmA = mA;
  _lastOutputPct = mAToPercent(mA, _cfg.outputMinmA, _cfg.outputMaxmA);
  _lastProcessPct = _lastOutputPct;
  return true;
}

// Escritura directa de una cuenta DAC.
// Se usa principalmente para calibración/diagnóstico. El DAC es de 12 bits, por lo que RAW se
// limita a 0...4095. Los valores de mA y % calculados después son solo una estimación informativa.
bool Generar_4_20mALib::setOutputRawDirect(uint16_t raw) {
  if (!_ready) return false;
  if (raw > 4095) raw = 4095;

  if (!writeDACFast(raw)) return false;

  _lastOutputRaw = raw;

  // Reconstrucción informativa de mA y % a partir del RAW escrito y de la calibración vigente.
  if (raw == 0) {
    _lastOutputmA = 0.0f;
    _lastOutputPct = 0.0f;
    _lastProcessPct = 0.0f;
  } else if (_cfg.rawAtOutputMax > _cfg.rawAtOutputMin) {
    const float frac = ((float)raw - (float)_cfg.rawAtOutputMin) /
                       ((float)_cfg.rawAtOutputMax - (float)_cfg.rawAtOutputMin);
    _lastOutputmA = _cfg.outputMinmA + frac * (_cfg.outputMaxmA - _cfg.outputMinmA);
    _lastOutputmA = clampf(_lastOutputmA, _cfg.outputMinmA, _cfg.outputMaxmA);
    _lastOutputPct = mAToPercent(_lastOutputmA, _cfg.outputMinmA, _cfg.outputMaxmA);
    _lastProcessPct = _lastOutputPct;
  }

  return true;
}

// Estado físico de salida desactivada.
// Fuerza RAW=0 directamente al DAC. Esto es distinto de 0 % de escala, que corresponde a 4 mA.
// No modifica la configuración del modelo y libera el latch de una falla FROZEN.
bool Generar_4_20mALib::setOutputOff() {
  if (!_ready) return false;

  // 0 % de una escala 4-20 mA equivale a 4 mA; RAW=0 se usa específicamente para OFF.
  if (!writeDACFast(0)) return false;

  _lastOutputRaw = 0;
  _lastOutputmA = 0.0f;
  _lastOutputPct = 0.0f;

  // Obliga a que una futura falla FROZEN capture un nuevo valor al reactivar la salida.
  _freezeLatched = false;
  return true;
}

// Actualiza los dos puntos de calibración del DAC.
// rawMin representa el RAW asociado a outputMinmA y rawMax el asociado a outputMaxmA.
// Se rechaza una calibración invertida o con ambos puntos iguales.
void Generar_4_20mALib::setCalibrationRaw(uint16_t rawMin, uint16_t rawMax) {
  if (rawMin > 4095) rawMin = 4095;
  if (rawMax > 4095) rawMax = 4095;
  if (rawMax <= rawMin) return;

  _cfg.rawAtOutputMin = rawMin;
  _cfg.rawAtOutputMax = rawMax;

  // Cambiar la calibración no escribe el DAC; únicamente modifica las conversiones posteriores.
}

// Configura el rango eléctrico esperado en la entrada del modelo.
void Generar_4_20mALib::setInputRange(float minmA, float maxmA) {
  if (maxmA <= minmA) return;
  _cfg.inputMinmA = minmA;
  _cfg.inputMaxmA = maxmA;
}

// Configura el rango eléctrico utilizado para representar la salida.
void Generar_4_20mALib::setOutputRange(float minmA, float maxmA) {
  if (maxmA <= minmA) return;
  _cfg.outputMinmA = minmA;
  _cfg.outputMaxmA = maxmA;
}

// Configura el proceso continuo G(s)=K/(tau*s+1) y su período de muestreo.
// Como estos parámetros alteran el modelo discreto y la equivalencia temporal del retardo,
// se recalculan a/b, N y se reinicia el buffer con la entrada actual.
void Generar_4_20mALib::setTransferFunction(float K, float tau_s, uint32_t sampleTimeMs) {
  _cfg.K = K;
  _cfg.tau_s = tau_s;
  _cfg.sampleTimeMs = sampleTimeMs;
  recomputeDiscreteTF();
  recomputeDelaySamples();
  resetDelayBuffer(_lastInputPct);

  // Reinicia la referencia temporal al cambiar la discretización para evitar
  // que queden pasos pendientes calculados con el período anterior.
  _lastUpdateMs = millis();
}

// Configura el tiempo muerto del proceso.
// Se limita al rango real utilizado por el proyecto: 0...30 s.
void Generar_4_20mALib::setDeadTime(float deadTime_s) {
  if (deadTime_s < 0.0f) deadTime_s = 0.0f;
  if (deadTime_s > 30.0f) deadTime_s = 30.0f;

  _cfg.deadTime_s = deadTime_s;
  recomputeDelaySamples();
  resetDelayBuffer(_lastInputPct);
}

// Configura la amplitud del ruido uniforme del sensor en % de escala.
// El rango aceptado por esta función es 0...20 %.
void Generar_4_20mALib::setOutputNoise(float noisePct) {
  if (noisePct < 0.0f) noisePct = 0.0f;
  if (noisePct > 20.0f) noisePct = 20.0f;
  _cfg.outputNoisePct = noisePct;

  // Este setter no modifica la salida física; el efecto aparece en la próxima actualización.
}

// Configura un offset aditivo de medición en % de escala.
// La biblioteca admite internamente valores entre -100 % y +100 %.
void Generar_4_20mALib::setSensorOffset(float offsetPct) {
  if (offsetPct < -100.0f) offsetPct = -100.0f;
  if (offsetPct > 100.0f) offsetPct = 100.0f;
  _cfg.sensorOffsetPct = offsetPct;

  // Este setter solo cambia la configuración; no fuerza una escritura inmediata al DAC.
}

// Selecciona la falla simulada del sensor.
// Al cambiar de modo se libera el latch de congelamiento para que una futura falla FROZEN
// capture una muestra nueva.
void Generar_4_20mALib::setSensorFaultMode(SensorFaultMode mode) {
  if (_cfg.sensorFaultMode != mode) {
    _cfg.sensorFaultMode = mode;
    _freezeLatched = false;
    _frozenSensorPct = _lastOutputPct;
  }

  // Este setter solo cambia la configuración; no fuerza una escritura inmediata al DAC.
}

// Define la corriente que representará una desconexión del sensor.
// Se admite desde 0 mA hasta el máximo configurado de salida.
void Generar_4_20mALib::setSensorDisconnectmA(float mA) {
  if (mA < 0.0f) mA = 0.0f;
  if (mA > _cfg.outputMaxmA) mA = _cfg.outputMaxmA;
  _cfg.sensorDisconnectmA = mA;
}

// Define los porcentajes impuestos por las fallas STUCK_MIN y STUCK_MAX.
void Generar_4_20mALib::setSensorStuckPercent(float minPct, float maxPct) {
  _cfg.sensorStuckMinPct = clampf(minPct, 0.0f, 100.0f);
  _cfg.sensorStuckMaxPct = clampf(maxPct, 0.0f, 100.0f);
}

// Getters de monitoreo.
// Exponen el último estado interno calculado sin modificar ninguna variable del modelo.
float Generar_4_20mALib::getLastInputmA() const { return _lastInputmA; }
float Generar_4_20mALib::getLastInputPct() const { return _lastInputPct; }
float Generar_4_20mALib::getLastProcessPct() const { return _lastProcessPct; }
float Generar_4_20mALib::getLastOutputPct() const { return _lastOutputPct; }
float Generar_4_20mALib::getLastOutputmA() const { return _lastOutputmA; }
uint16_t Generar_4_20mALib::getLastOutputRaw() const { return _lastOutputRaw; }
uint16_t Generar_4_20mALib::getRawAtOutputMin() const { return _cfg.rawAtOutputMin; }
uint16_t Generar_4_20mALib::getRawAtOutputMax() const { return _cfg.rawAtOutputMax; }
float Generar_4_20mALib::getDeadTime() const { return _cfg.deadTime_s; }
uint16_t Generar_4_20mALib::getDelaySamples() const { return _delaySamples; }
float Generar_4_20mALib::getOutputNoise() const { return _cfg.outputNoisePct; }
float Generar_4_20mALib::getSensorOffset() const { return _cfg.sensorOffsetPct; }
Generar_4_20mALib::SensorFaultMode Generar_4_20mALib::getSensorFaultMode() const { return _cfg.sensorFaultMode; }
// Indica si existe alguna alteración intencional de la medición.
// Se considera condición de falla/alteración tanto un modo de falla explícito como ruido u offset.
bool Generar_4_20mALib::hasSensorFault() const {
  return _cfg.sensorFaultMode != SENSOR_FAULT_NONE ||
         _cfg.outputNoisePct > 0.0f ||
         fabsf(_cfg.sensorOffsetPct) > 0.001f;
}
bool Generar_4_20mALib::isReady() const { return _ready; }

// Devuelve un texto corto para mostrar el estado del modelo de sensor en las interfaces.
// Si no existe una falla discreta, informa primero offset, luego ruido y finalmente OK.
const char* Generar_4_20mALib::getSensorFaultText() const {
  switch (_cfg.sensorFaultMode) {
    case SENSOR_FAULT_DISCONNECTED: return "Desconectado";
    case SENSOR_FAULT_FROZEN:       return "Congelado";
    case SENSOR_FAULT_STUCK_MIN:    return "Min";
    case SENSOR_FAULT_STUCK_MAX:    return "Max";
    case SENSOR_FAULT_OFFSET:
    case SENSOR_FAULT_NONE:
    default:
      if (fabsf(_cfg.sensorOffsetPct) > 0.001f) return "Offset";
      if (_cfg.outputNoisePct > 0.0f) return "Ruido";
      return "OK";
  }
}

// Sondea la dirección I2C del DAC.
// endTransmission()==0 indica que un dispositivo respondió correctamente.
bool Generar_4_20mALib::probeDAC() {
  _cfg.wire->beginTransmission(_cfg.i2cAddr);
  return (_cfg.wire->endTransmission() == 0);
}

// Escribe directamente los 12 bits del DAC mediante I2C.
// El valor se separa en los 4 bits superiores y los 8 bits inferiores requeridos por la trama.
bool Generar_4_20mALib::writeDACFast(uint16_t raw) {
  if (raw > 4095) raw = 4095;

  _cfg.wire->beginTransmission(_cfg.i2cAddr);
  _cfg.wire->write((raw >> 8) & 0x0F);
  _cfg.wire->write(raw & 0xFF);

  return (_cfg.wire->endTransmission() == 0);
}

// Convierte el resultado final del modelo de sensor en la señal física.
// Normalmente hace % -> mA; en DISCONNECTED usa directamente sensorDisconnectmA.
// Luego convierte mA -> RAW, escribe el DAC y actualiza los valores de monitoreo.
bool Generar_4_20mALib::writeSensorOutput(float sensorPct) {
  float mA;

  if (_cfg.sensorFaultMode == SENSOR_FAULT_DISCONNECTED) {
    mA = _cfg.sensorDisconnectmA;
  } else {
    sensorPct = clampf(sensorPct, 0.0f, 100.0f);
    mA = percentToMA(sensorPct, _cfg.outputMinmA, _cfg.outputMaxmA);
  }

  const uint16_t raw = currentToRawExtended(mA);
  if (!writeDACFast(raw)) return false;

  _lastOutputRaw = raw;
  _lastOutputmA = mA;
  _lastOutputPct = mAToPercent(mA, _cfg.outputMinmA, _cfg.outputMaxmA);

  if (_cfg.sensorFaultMode != SENSOR_FAULT_DISCONNECTED) {
    _lastOutputPct = clampf(sensorPct, 0.0f, 100.0f);
  }

  return true;
}

// Saturación genérica de un valor flotante dentro del intervalo [lo, hi].
float Generar_4_20mALib::clampf(float x, float lo, float hi) const {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

// Conversión lineal de corriente a porcentaje de escala.
// minmA -> 0 % y maxmA -> 100 %.
float Generar_4_20mALib::mAToPercent(float mA, float minmA, float maxmA) const {
  mA = clampf(mA, minmA, maxmA);
  return ((mA - minmA) * 100.0f) / (maxmA - minmA);
}

// Conversión lineal inversa: porcentaje de escala -> corriente.
float Generar_4_20mALib::percentToMA(float pct, float minmA, float maxmA) const {
  pct = clampf(pct, 0.0f, 100.0f);
  return minmA + ((maxmA - minmA) * pct / 100.0f);
}

// Conversión calibrada de corriente a cuenta DAC dentro del rango normal de salida.
// Interpola linealmente entre rawAtOutputMin y rawAtOutputMax.
uint16_t Generar_4_20mALib::currentToRaw(float mA) const {
  mA = clampf(mA, _cfg.outputMinmA, _cfg.outputMaxmA);

  const float frac = (mA - _cfg.outputMinmA) / (_cfg.outputMaxmA - _cfg.outputMinmA);
  float raw = (float)_cfg.rawAtOutputMin + frac * ((float)_cfg.rawAtOutputMax - (float)_cfg.rawAtOutputMin);

  raw = clampf(raw, 0.0f, 4095.0f);
  return (uint16_t)lroundf(raw);
}

// Conversión extendida para representar corrientes inferiores al mínimo normal.
// Permite, por ejemplo, aproximar 0...4 mA durante una falla de desconexión.
// Por encima de outputMinmA reutiliza la calibración normal de currentToRaw().
uint16_t Generar_4_20mALib::currentToRawExtended(float mA) const {
  if (mA <= 0.0f) return 0;

  if (mA < _cfg.outputMinmA) {
    const float fracLow = mA / _cfg.outputMinmA;
    const float rawLow = fracLow * (float)_cfg.rawAtOutputMin;
    return (uint16_t)lroundf(clampf(rawLow, 0.0f, 4095.0f));
  }

  return currentToRaw(mA);
}

// Discretiza exactamente el modelo de primer orden para un retenedor de orden cero.
// Modelo continuo: G(s)=K/(tau*s+1)
// Ecuación implementada: y[k] = a*y[k-1] + b*u[k-N]
//   a = exp(-Ts/tau)
//   b = K*(1-a)
void Generar_4_20mALib::recomputeDiscreteTF() {
  if (_cfg.sampleTimeMs == 0) _cfg.sampleTimeMs = 1;

  const float Ts = _cfg.sampleTimeMs / 1000.0f;

  if (_cfg.tau_s <= 0.000001f) {
    _a = 0.0f;
    _b = _cfg.K;
    return;
  }

  // Discretización exacta del polo de primer orden para el período Ts.
  _a = expf(-Ts / _cfg.tau_s);
  _b = _cfg.K * (1.0f - _a);
}

// Convierte el tiempo muerto expresado en segundos a un número entero de muestras.
// N = round(deadTime/Ts). El resultado se limita a la capacidad del buffer circular.
void Generar_4_20mALib::recomputeDelaySamples() {
  if (_cfg.sampleTimeMs == 0) _cfg.sampleTimeMs = 1;

  if (_cfg.deadTime_s < 0.0f) _cfg.deadTime_s = 0.0f;
  if (_cfg.deadTime_s > 30.0f) _cfg.deadTime_s = 30.0f;

  const float Ts = _cfg.sampleTimeMs / 1000.0f;
  // Cantidad de posiciones que debe retroceder la lectura dentro del buffer.
  uint32_t samples = (uint32_t)lroundf(_cfg.deadTime_s / Ts);

  if (samples >= MAX_DELAY_SAMPLES) {
    samples = MAX_DELAY_SAMPLES - 1;
  }

  _delaySamples = (uint16_t)samples;
  if (_delayIndex >= MAX_DELAY_SAMPLES) _delayIndex = 0;
}

// Inicializa todo el buffer de tiempo muerto con un mismo valor.
// Esto evita que después de cambiar parámetros aparezcan datos antiguos del retardo.
void Generar_4_20mALib::resetDelayBuffer(float valuePct) {
  valuePct = clampf(valuePct, 0.0f, 100.0f);
  for (uint16_t i = 0; i < MAX_DELAY_SAMPLES; i++) {
    _delayBuffer[i] = valuePct;
  }
  _delayIndex = 0;
}

// Implementa el tiempo muerto mediante un buffer circular.
// Escribe la entrada actual en _delayIndex y lee la muestra ubicada N posiciones atrás.
// Con N=0, devuelve la entrada actual sin retardo.
float Generar_4_20mALib::pushAndGetDelayedInput(float inputPct) {
  inputPct = clampf(inputPct, 0.0f, 100.0f);

  // Escribe primero la muestra actual en la posición de escritura.
  _delayBuffer[_delayIndex] = inputPct;

  uint16_t readIndex = _delayIndex;
  if (_delaySamples > 0) {
    readIndex = (_delayIndex + MAX_DELAY_SAMPLES - _delaySamples) % MAX_DELAY_SAMPLES;
  }

  // Valor que ve el proceso en este instante después de aplicar el tiempo muerto.
  const float delayed = _delayBuffer[readIndex];

  _delayIndex++;
  if (_delayIndex >= MAX_DELAY_SAMPLES) _delayIndex = 0;

  return delayed;
}

// Aplica las imperfecciones/fallas de medición sobre la salida ideal del proceso.
// Orden base: proceso + offset + ruido; luego el modo de falla puede reemplazar ese valor.
// La salida de esta función queda limitada finalmente a 0...100 %.
float Generar_4_20mALib::applySensorModel(float processPct) {
  // Primero se construye la medición nominal agregando offset y ruido al valor ideal del proceso.
  float sensorPct = processPct + _cfg.sensorOffsetPct + randomNoisePct();

  switch (_cfg.sensorFaultMode) {
    case SENSOR_FAULT_DISCONNECTED:
      _freezeLatched = false;
      sensorPct = 0.0f;
      break;

    case SENSOR_FAULT_FROZEN:
      if (!_freezeLatched) {
        _frozenSensorPct = sensorPct;
        _freezeLatched = true;
      }
      sensorPct = _frozenSensorPct;
      break;

    case SENSOR_FAULT_STUCK_MIN:
      _freezeLatched = false;
      sensorPct = _cfg.sensorStuckMinPct;
      break;

    case SENSOR_FAULT_STUCK_MAX:
      _freezeLatched = false;
      sensorPct = _cfg.sensorStuckMaxPct;
      break;

    case SENSOR_FAULT_OFFSET:
    case SENSOR_FAULT_NONE:
    default:
      _freezeLatched = false;
      break;
  }

  return clampf(sensorPct, 0.0f, 100.0f);
}

// Genera ruido uniforme centrado en cero.
// El resultado pertenece aproximadamente a [-outputNoisePct, +outputNoisePct].
float Generar_4_20mALib::randomNoisePct() const {
  if (_cfg.outputNoisePct <= 0.0f) return 0.0f;

  // random() genera un factor aproximadamente uniforme entre -1 y +1.
  const long r = random(-10000L, 10001L);
  return ((float)r / 10000.0f) * _cfg.outputNoisePct;
}
