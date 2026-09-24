/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Interfaz de avisos sonoros no bloqueantes mediante PWM.
 */

#ifndef BUZZERLIB_H
#define BUZZERLIB_H

#include <Arduino.h>

class BuzzerLib {
public:
  struct Step {
    uint16_t freq;
    uint16_t duration;
  };

  BuzzerLib(uint8_t pin, uint8_t resolution = 8, uint32_t baseFreq = 2000);

  void begin();
  void update();

  void setEnabled(bool enabled);
  bool isEnabled() const;

  void stop();
  bool isPlaying() const;
  bool isIdle() const;

  void play(const Step* sequence, uint8_t length);

  void startup();
  void shutdown();
  void click();
  void ok();
  void warning();
  void alarmHigh();
  void alarmLow();
  void trip();
  void criticalError();
  void heartbeat();
  void special();

private:
  uint8_t _pin;
  uint8_t _resolution;
  uint32_t _baseFreq;

  bool _enabled;
  const Step* _sequence;
  uint8_t _length;
  uint8_t _index;
  bool _playing;
  unsigned long _stepStartMs;
  uint16_t _tempoNum;
  uint16_t _tempoDen;

  void playScaled(const Step* sequence, uint8_t length, uint16_t tempoNum, uint16_t tempoDen);
  void startStep();
  void nextStep();
  void applyStep(const Step& step);
  void outputOff();
  uint16_t scaleDuration(uint16_t duration) const;
};

#endif