/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 *
 * Descripcion:
 *   Implementacion de la adquisicion de una entrada industrial de 4-20 mA
 *   mediante el ADC del ESP32-S3.
 *
 * Flujo general:
 *   Senal analogica -> ADC -> promedio de muestras -> filtro IIR ->
 *   calibracion de dos puntos -> corriente [mA] -> porcentaje [%].
 *
 * La calibracion se realiza con dos referencias reales:
 *   rawAt4mA  <->  4 mA  <->   0 %
 *   rawAt20mA <-> 20 mA  <-> 100 %
 *
 * Ademas se incluyen funciones de diagnostico para detectar valores fuera
 * del rango nominal, validar la calibracion y observar variables internas.
 */

#include "Leer_4_20mA.h"

// Constructor de la entrada analogica.
// Recibe el GPIO del ADC, los dos puntos de calibracion, el periodo de muestreo,
// la intensidad del filtro digital y la cantidad de conversiones a promediar.
Leer_4_20mA::Leer_4_20mA(uint8_t pin,
                         uint16_t rawAt4mA,
                         uint16_t rawAt20mA,
                         uint32_t samplePeriodUs,
                         uint8_t filterShift,
                         uint8_t samplesPerUpdate)
  : _pin(pin),
    _rawAt4mA(rawAt4mA),
    _rawAt20mA(rawAt20mA),
    _samplePeriodUs(samplePeriodUs),
    _filterShift(filterShift),
    _samplesPerUpdate(samplesPerUpdate ? samplesPerUpdate : 1),
    _raw(0),
    _filteredQ8(0),
    _lastSampleUs(0),
    _started(false),
    _hasSample(false) {}

// Inicializa el ADC y el estado interno del filtro.
// El ESP32-S3 trabaja aqui con resolucion de 12 bits: 0...4095 cuentas.
// En ESP32 se selecciona ADC_11db para ampliar el rango de tension medible.
// La primera lectura se utiliza para arrancar el filtro desde un valor real
// y evitar un transitorio artificial desde cero.
void Leer_4_20mA::begin() {
  analogReadResolution(12);
#if defined(ARDUINO_ARCH_ESP32)
  analogSetPinAttenuation(_pin, ADC_11db);
#endif
  pinMode(_pin, INPUT);

  _raw = analogRead(_pin);
  _filteredQ8 = static_cast<int32_t>(_raw) << 8;
  _lastSampleUs = micros();
  _started = true;
  _hasSample = true;
}

// Actualiza la medicion de forma no bloqueante.
// Solo toma una nueva muestra cuando transcurrio _samplePeriodUs.
// Retorna true cuando hubo una actualizacion y false cuando todavia no corresponde.
//
// Procesamiento:
//   1) Promedia _samplesPerUpdate conversiones ADC.
//   2) Convierte el RAW a formato fijo Q8 para conservar precision fraccional.
//   3) Aplica un filtro IIR de primer orden.
//      y[k] = y[k-1] + (x[k] - y[k-1]) / 2^filterShift
//
// Con filterShift = 3:
//      y[k] = y[k-1] + (x[k] - y[k-1]) / 8
//
// Un filterShift mayor produce mas suavizado pero una respuesta mas lenta.
// Con filterShift = 0 se desactiva el filtrado y se usa directamente el promedio.
bool Leer_4_20mA::update() {
  if (!_started) {
    begin();
  }

  const uint32_t now = micros();
  if (static_cast<uint32_t>(now - _lastSampleUs) < _samplePeriodUs) {
    return false;
  }
  _lastSampleUs = now;

  _raw = sampleAveraged_();

  const int32_t rawQ8 = static_cast<int32_t>(_raw) << 8;

  if (!_hasSample) {
    _filteredQ8 = rawQ8;
    _hasSample = true;
  } else if (_filterShift == 0) {
    _filteredQ8 = rawQ8;
  } else {
    _filteredQ8 += (rawQ8 - _filteredQ8) >> _filterShift;
  }

  return true;
}

// Actualiza los dos puntos de calibracion ADC.
// No realiza una nueva medicion: solamente reemplaza las referencias utilizadas
// posteriormente para convertir cuentas ADC a mA y porcentaje.
void Leer_4_20mA::setCalibration(uint16_t rawAt4mA, uint16_t rawAt20mA) {
  _rawAt4mA = rawAt4mA;
  _rawAt20mA = rawAt20mA;
}

// Modifica el periodo minimo entre actualizaciones, expresado en microsegundos.
void Leer_4_20mA::setSamplePeriodUs(uint32_t samplePeriodUs) {
  _samplePeriodUs = samplePeriodUs;
}

// Ajusta la constante del filtro IIR mediante una potencia de dos.
// filterShift = 0 -> sin filtro.
// filterShift = 1 -> corrige 1/2 del error por muestra.
// filterShift = 3 -> corrige 1/8 del error por muestra.
void Leer_4_20mA::setFilterShift(uint8_t filterShift) {
  _filterShift = filterShift;
}

// Define cuantas conversiones ADC se promedian en cada actualizacion.
// Si se solicita cero, se fuerza una muestra como minimo.
void Leer_4_20mA::setSamplesPerUpdate(uint8_t samplesPerUpdate) {
  _samplesPerUpdate = samplesPerUpdate ? samplesPerUpdate : 1;
}

// Realiza una captura puntual promediando varias conversiones ADC.
// Se usa principalmente para obtener un valor RAW estable durante la
// calibracion fisica de los puntos de 4 mA y 20 mA.
// Esta funcion no utiliza el valor filtrado habitual de update().
uint16_t Leer_4_20mA::captureRaw(uint16_t samples) {
  if (!_started) {
    begin();
  }

  if (samples == 0) {
    samples = 1;
  }

  uint32_t acc = 0;
  for (uint16_t i = 0; i < samples; ++i) {
    acc += analogRead(_pin);
  }
  return static_cast<uint16_t>((acc + (samples / 2U)) / samples);
}

// Devuelve el GPIO utilizado como entrada ADC.
uint8_t Leer_4_20mA::pin() const {
  return _pin;
}

// Devuelve la ultima lectura promediada antes de aplicar el filtro IIR.
uint16_t Leer_4_20mA::raw() const {
  return _raw;
}

// Devuelve el RAW filtrado.
// Internamente _filteredQ8 posee 8 bits fraccionales; se suma 128 antes
// del desplazamiento para redondear al entero mas cercano.
uint16_t Leer_4_20mA::filteredRaw() const {
  return static_cast<uint16_t>((_filteredQ8 + 128) >> 8);
}

// Devuelve la cuenta ADC almacenada como referencia de 4 mA.
uint16_t Leer_4_20mA::rawAt4mA() const {
  return _rawAt4mA;
}

// Devuelve la cuenta ADC almacenada como referencia de 20 mA.
uint16_t Leer_4_20mA::rawAt20mA() const {
  return _rawAt20mA;
}

// Convierte la ultima lectura filtrada a corriente utilizando la calibracion.
float Leer_4_20mA::currentmA() const {
  return currentmAFromRaw(filteredRaw());
}

// Convierte una cuenta ADC cualquiera a mA mediante interpolacion lineal.
//
// Formula:
//   I[mA] = 4 + 16 * (RAW - RAW_4mA) / (RAW_20mA - RAW_4mA)
//
// La conversion NO limita primero el RAW al rango calibrado, por lo que puede
// devolver valores menores de 4 mA o mayores de 20 mA. Esto es intencional:
// permite detectar underrange y overrange.
// Si la calibracion no es valida se devuelve NAN.
float Leer_4_20mA::currentmAFromRaw(uint16_t rawValue) const {
  if (!hasValidCalibration()) {
    return NAN;
  }

  const int32_t spanCounts =
      static_cast<int32_t>(_rawAt20mA) - static_cast<int32_t>(_rawAt4mA);
  const int32_t deltaCounts =
      static_cast<int32_t>(rawValue) - static_cast<int32_t>(_rawAt4mA);

  return 4.0f +
         (16.0f * static_cast<float>(deltaCounts) /
          static_cast<float>(spanCounts));
}

// Devuelve la entrada escalada al rango util 0...100 %.
// A diferencia de percentUnclamped(), limita valores fuera del rango.
float Leer_4_20mA::percent() const {
  return clampf_(percentUnclamped(), 0.0f, 100.0f);
}

// Calcula el porcentaje directamente desde las cuentas ADC, sin limitarlo.
//
// Formula:
//   % = 100 * (RAW - RAW_4mA) / (RAW_20mA - RAW_4mA)
//
// Es matematicamente equivalente a convertir primero a mA y luego a porcentaje,
// pero conservar el calculo desde RAW facilita el diagnostico del canal.
// Puede devolver valores negativos o mayores de 100 % si la entrada sale
// del rango nominal. Si la calibracion no es valida devuelve NAN.
float Leer_4_20mA::percentUnclamped() const {
  if (!hasValidCalibration()) {
    return NAN;
  }
  const int32_t spanCounts =
      static_cast<int32_t>(_rawAt20mA) - static_cast<int32_t>(_rawAt4mA);
  const int32_t deltaCounts =
      static_cast<int32_t>(filteredRaw()) - static_cast<int32_t>(_rawAt4mA);

  return 100.0f * static_cast<float>(deltaCounts) /
         static_cast<float>(spanCounts);
}

// Diagnostico de corriente baja.
// Se considera underrange cuando la corriente calculada es menor a 3.8 mA.
// El margen respecto de 4 mA evita declarar falla por pequenas variaciones normales.
bool Leer_4_20mA::underrange() const {
  const float mA = currentmA();
  return !isnan(mA) && (mA < 3.8f);
}

// Diagnostico de corriente alta.
// Se considera overrange cuando la corriente calculada supera 20.2 mA.
bool Leer_4_20mA::overrange() const {
  const float mA = currentmA();
  return !isnan(mA) && (mA > 20.2f);
}

// Verifica que los dos puntos de calibracion sean coherentes.
// Condiciones:
//   - ambos deben caber dentro del ADC de 12 bits (0...4095);
//   - RAW_20mA debe ser mayor que RAW_4mA;
//   - el span debe ser de al menos 300 cuentas para evitar una calibracion
//     demasiado pequena o numericamente sensible.
bool Leer_4_20mA::hasValidCalibration() const {
  if (_rawAt4mA > 4095 || _rawAt20mA > 4095) return false;
  if (_rawAt20mA <= _rawAt4mA) return false;

  return ((_rawAt20mA - _rawAt4mA) >= 300);
}

// Imprime una linea completa de diagnostico sobre cualquier objeto Print
// (por ejemplo Serial). Incluye RAW instantaneo, RAW filtrado, calibracion,
// corriente, porcentaje libre, porcentaje limitado y banderas UR/OR.
void Leer_4_20mA::printDebug(Print& out, const char* label) const {
  const uint16_t rawInstant = raw();
  const uint16_t rawFiltered = filteredRaw();
  const float mA = currentmAFromRaw(rawFiltered);
  const float pctUnclamped = percentUnclamped();
  const float pctClamped = percent();

  out.print('[');
  out.print(label ? label : "AI");
  out.print(F("] RAW="));
  out.print(rawInstant);
  out.print(F(" | FILT="));
  out.print(rawFiltered);
  out.print(F(" | CAL4="));
  out.print(_rawAt4mA);
  out.print(F(" | CAL20="));
  out.print(_rawAt20mA);
  out.print(F(" | mA="));

  if (isnan(mA)) out.print(F("NAN"));
  else out.print(mA, 3);

  out.print(F(" | PCT_LIBRE="));
  if (isnan(pctUnclamped)) out.print(F("NAN"));
  else out.print(pctUnclamped, 2);

  out.print(F(" | PCT="));
  if (isnan(pctClamped)) out.print(F("NAN"));
  else out.print(pctClamped, 2);

  out.print(F(" | UR="));
  out.print(underrange() ? 1 : 0);
  out.print(F(" | OR="));
  out.println(overrange() ? 1 : 0);
}

// Realiza el sobremuestreo basico de cada actualizacion.
// Suma _samplesPerUpdate conversiones consecutivas y devuelve su promedio.
// El termino samples/2 implementa redondeo entero al valor mas cercano.
uint16_t Leer_4_20mA::sampleAveraged_() {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < _samplesPerUpdate; ++i) {
    acc += analogRead(_pin);
  }
  return static_cast<uint16_t>((acc + (_samplesPerUpdate / 2U)) /
                               _samplesPerUpdate);
}

// Funcion auxiliar de saturacion: limita x al intervalo [minValue, maxValue].
float Leer_4_20mA::clampf_(float x, float minValue, float maxValue) {
  if (x < minValue) return minValue;
  if (x > maxValue) return maxValue;
  return x;
}
