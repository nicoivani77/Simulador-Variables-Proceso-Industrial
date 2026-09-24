/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de avisos sonoros no bloqueantes mediante PWM.
 */

#include "BuzzerLib.h"

// Escala usada por los avisos sonoros habituales.
static constexpr uint16_t SPEED_NUM = 5;
static constexpr uint16_t SPEED_DEN = 6;

BuzzerLib::BuzzerLib(uint8_t pin, uint8_t resolution, uint32_t baseFreq)
  : _pin(pin),
    _resolution(resolution),
    _baseFreq(baseFreq),
    _enabled(true),
    _sequence(nullptr),
    _length(0),
    _index(0),
    _playing(false),
    _stepStartMs(0),
    _tempoNum(SPEED_NUM),
    _tempoDen(SPEED_DEN) {}

void BuzzerLib::begin() {
  ledcAttach(_pin, _baseFreq, _resolution);
  outputOff();
}

void BuzzerLib::update() {
  if (!_playing || !_sequence || _index >= _length) return;

  if (millis() - _stepStartMs >= scaleDuration(_sequence[_index].duration)) {
    nextStep();
  }
}

void BuzzerLib::setEnabled(bool enabled) {
  _enabled = enabled;

  // La salida se apaga al cambiar el estado para evitar tonos sostenidos inesperados.
  outputOff();
}

bool BuzzerLib::isEnabled() const {
  return _enabled;
}

void BuzzerLib::stop() {
  _playing = false;
  _sequence = nullptr;
  _length = 0;
  _index = 0;
  outputOff();
}

bool BuzzerLib::isPlaying() const {
  return _playing;
}

bool BuzzerLib::isIdle() const {
  return !_playing;
}

void BuzzerLib::play(const Step* sequence, uint8_t length) {
  playScaled(sequence, length, SPEED_NUM, SPEED_DEN);
}

void BuzzerLib::playScaled(const Step* sequence,
                           uint8_t length,
                           uint16_t tempoNum,
                           uint16_t tempoDen) {
  if (!sequence || !length || tempoNum == 0 || tempoDen == 0) {
    stop();
    return;
  }

  _sequence = sequence;
  _length = length;
  _index = 0;
  _tempoNum = tempoNum;
  _tempoDen = tempoDen;
  _playing = true;
  startStep();
}

void BuzzerLib::startStep() {
  if (!_playing || !_sequence || _index >= _length) {
    stop();
    return;
  }

  applyStep(_sequence[_index]);
  _stepStartMs = millis();
}

void BuzzerLib::nextStep() {
  if (++_index >= _length) {
    stop();
  } else {
    startStep();
  }
}

void BuzzerLib::applyStep(const Step& step) {
  if (!_enabled || step.freq == 0) {
    outputOff();
    return;
  }

  ledcWriteTone(_pin, step.freq);
}

void BuzzerLib::outputOff() {
  ledcWriteTone(_pin, 0);
}

uint16_t BuzzerLib::scaleDuration(uint16_t duration) const {
  uint16_t d = (uint32_t)duration * _tempoNum / _tempoDen;
  return d ? d : 1;
}

// Secuencias sonoras predefinidas.

static const BuzzerLib::Step SEQ_STARTUP[] = {
  {1175,  60},
  {0,     20},
  {1319,  80},
  {1568, 120}
};

static const BuzzerLib::Step SEQ_SHUTDOWN[] = {
  {1568,  70},
  {1319,  70},
  {1175, 120}
};

static const BuzzerLib::Step SEQ_CLICK[] = {
  {2200, 35}
};

static const BuzzerLib::Step SEQ_OK[] = {
  {1500, 60},
  {1900, 90}
};

static const BuzzerLib::Step SEQ_WARNING[] = {
  {1800, 100},
  {0,     70},
  {1800, 100},
  {0,     70},
  {1800, 140}
};

static const BuzzerLib::Step SEQ_ALARM_HIGH[] = {
  {2100, 120},
  {0,     60},
  {2400, 180},
  {0,    100},
  {2400, 180}
};

static const BuzzerLib::Step SEQ_ALARM_LOW[] = {
  {1200, 120},
  {0,     60},
  {900,  180},
  {0,    100},
  {900,  180}
};

static const BuzzerLib::Step SEQ_TRIP[] = {
  {2500, 100},
  {0,     40},
  {2500, 100},
  {0,     40},
  {800,  500}
};

static const BuzzerLib::Step SEQ_CRITICAL_ERROR[] = {
  {900,  220},
  {600,  220},
  {900,  220},
  {600,  220},
  {500,  350}
};

static const BuzzerLib::Step SEQ_HEARTBEAT[] = {
  {2000, 35},
  {0,    90},
  {2000, 35}
};

// Secuencia adicional. Las duraciones se almacenan en su valor base y se
// reproducen con un factor 0.6. Cada nota incluye una pausa del 10 %.
static constexpr uint16_t NOTE_F4  = 349;
static constexpr uint16_t NOTE_GS4 = 415;
static constexpr uint16_t NOTE_A4  = 440;
static constexpr uint16_t NOTE_C5  = 523;
static constexpr uint16_t NOTE_E5  = 659;
static constexpr uint16_t NOTE_F5  = 698;
static constexpr uint16_t REST     = 0;

static const BuzzerLib::Step SEQ_SPECIAL[] = {
  {NOTE_A4,  500}, {REST,  50},
  {NOTE_A4,  500}, {REST,  50},
  {NOTE_A4,  500}, {REST,  50},
  {NOTE_F4,  350}, {REST,  35},
  {NOTE_C5,  150}, {REST,  15},

  {NOTE_A4,  500}, {REST,  50},
  {NOTE_F4,  350}, {REST,  35},
  {NOTE_C5,  150}, {REST,  15},
  {NOTE_A4, 1000}, {REST, 100},

  {NOTE_E5,  500}, {REST,  50},
  {NOTE_E5,  500}, {REST,  50},
  {NOTE_E5,  500}, {REST,  50},
  {NOTE_F5,  350}, {REST,  35},
  {NOTE_C5,  150}, {REST,  15},

  {NOTE_GS4, 500}, {REST,  50},
  {NOTE_F4,  350}, {REST,  35},
  {NOTE_C5,  150}, {REST,  15},
  {NOTE_A4, 1000}, {REST, 100}
};

static constexpr uint16_t SPECIAL_TEMPO_NUM = 6;
static constexpr uint16_t SPECIAL_TEMPO_DEN = 10;

// Presets públicos de aviso sonoro.

void BuzzerLib::startup() {
  play(SEQ_STARTUP, sizeof(SEQ_STARTUP) / sizeof(SEQ_STARTUP[0]));
}

void BuzzerLib::shutdown() {
  play(SEQ_SHUTDOWN, sizeof(SEQ_SHUTDOWN) / sizeof(SEQ_SHUTDOWN[0]));
}

void BuzzerLib::click() {
  play(SEQ_CLICK, sizeof(SEQ_CLICK) / sizeof(SEQ_CLICK[0]));
}

void BuzzerLib::ok() {
  play(SEQ_OK, sizeof(SEQ_OK) / sizeof(SEQ_OK[0]));
}

void BuzzerLib::warning() {
  play(SEQ_WARNING, sizeof(SEQ_WARNING) / sizeof(SEQ_WARNING[0]));
}

void BuzzerLib::alarmHigh() {
  play(SEQ_ALARM_HIGH, sizeof(SEQ_ALARM_HIGH) / sizeof(SEQ_ALARM_HIGH[0]));
}

void BuzzerLib::alarmLow() {
  play(SEQ_ALARM_LOW, sizeof(SEQ_ALARM_LOW) / sizeof(SEQ_ALARM_LOW[0]));
}

void BuzzerLib::trip() {
  play(SEQ_TRIP, sizeof(SEQ_TRIP) / sizeof(SEQ_TRIP[0]));
}

void BuzzerLib::criticalError() {
  play(SEQ_CRITICAL_ERROR, sizeof(SEQ_CRITICAL_ERROR) / sizeof(SEQ_CRITICAL_ERROR[0]));
}

void BuzzerLib::heartbeat() {
  play(SEQ_HEARTBEAT, sizeof(SEQ_HEARTBEAT) / sizeof(SEQ_HEARTBEAT[0]));
}

void BuzzerLib::special() {
  playScaled(SEQ_SPECIAL,
             sizeof(SEQ_SPECIAL) / sizeof(SEQ_SPECIAL[0]),
             SPECIAL_TEMPO_NUM,
             SPECIAL_TEMPO_DEN);
}
