/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Definición de presets de proceso para el modelo de primer orden con tiempo muerto opcional.
 */

#pragma once

#include <Arduino.h>


// Rangos directos de los parámetros del modelo de primer orden.
// La interfaz y el registro CSV utilizan estos mismos valores sin conversiones didácticas.
static constexpr float PROCESS_K_MIN = 0.0f;
static constexpr float PROCESS_K_MAX = 5.0f;
static constexpr float PROCESS_TAU_MIN_S = 0.1f;
static constexpr float PROCESS_TAU_MAX_S = 60.0f;

static inline float clampProcessParam(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

// Presets representativos para la función de transferencia de primer orden:
// G(s) = K / (tau*s + 1), con tiempo muerto opcional.
//
// Los presets no representan modelos identificados de plantas específicas. Se definieron
// como configuraciones didácticas sobre un mismo modelo matemático, tomando como referencia
// el comportamiento relativo observado en distintos tipos de procesos industriales.
//
// Se adopta K = 1.0 en los cuatro presets para aprovechar el rango completo normalizado:
// en ausencia de offset, ruido y saturación, una entrada entre 0 % y 100 % puede producir
// una salida estacionaria equivalente entre 0 % y 100 %.
//
// La diferenciación entre presets se realiza principalmente mediante tau y tiempo muerto:
// Caudal es el proceso más rápido, seguido por Presión y Nivel, mientras que Temperatura
// presenta la dinámica más lenta y el mayor retardo.
//
// Estos valores son puntos de partida configurables y pueden modificarse desde las
// interfaces del equipo.

enum ProcessPreset : uint8_t {
  PROCESS_PRESET_CUSTOM = 0,
  PROCESS_PRESET_LEVEL,
  PROCESS_PRESET_FLOW,
  PROCESS_PRESET_PRESSURE,
  PROCESS_PRESET_TEMPERATURE,
  PROCESS_PRESET_COUNT
};

struct ProcessPresetDefinition {
  ProcessPreset preset;
  const char* name;
  const char* shortName;
  float K;
  float tau_s;
  float deadTime_s;
};

static const ProcessPresetDefinition PROCESS_PRESET_DEFINITIONS[] = {
  {PROCESS_PRESET_CUSTOM,      "Personalizado", "Personal", 1.00f,  2.0f, 0.0f},
  {PROCESS_PRESET_LEVEL,       "Nivel",         "Nivel",    1.00f, 10.0f, 0.8f},
  {PROCESS_PRESET_FLOW,        "Caudal",        "Caudal",   1.00f,  2.0f, 0.2f},
  {PROCESS_PRESET_PRESSURE,    "Presion",       "Presion",  1.00f,  5.0f, 0.5f},
  {PROCESS_PRESET_TEMPERATURE, "Temperatura",   "Temp",     1.00f, 20.0f, 2.0f}
};

static inline ProcessPreset sanitizeProcessPreset(uint8_t raw) {
  if (raw >= PROCESS_PRESET_COUNT) return PROCESS_PRESET_CUSTOM;
  return static_cast<ProcessPreset>(raw);
}

static inline const ProcessPresetDefinition& processPresetDefinition(ProcessPreset preset) {
  const ProcessPreset safePreset = sanitizeProcessPreset(static_cast<uint8_t>(preset));
  return PROCESS_PRESET_DEFINITIONS[static_cast<uint8_t>(safePreset)];
}

static inline const char* processPresetName(ProcessPreset preset) {
  return processPresetDefinition(preset).name;
}

static inline const char* processPresetShortName(ProcessPreset preset) {
  return processPresetDefinition(preset).shortName;
}

static inline bool processPresetIsAutomatic(ProcessPreset preset) {
  return sanitizeProcessPreset(static_cast<uint8_t>(preset)) != PROCESS_PRESET_CUSTOM;
}

static inline ProcessPreset nextAutomaticProcessPreset(ProcessPreset current) {
  switch (sanitizeProcessPreset(static_cast<uint8_t>(current))) {
    case PROCESS_PRESET_LEVEL:       return PROCESS_PRESET_FLOW;
    case PROCESS_PRESET_FLOW:        return PROCESS_PRESET_PRESSURE;
    case PROCESS_PRESET_PRESSURE:    return PROCESS_PRESET_TEMPERATURE;
    case PROCESS_PRESET_TEMPERATURE: return PROCESS_PRESET_LEVEL;
    case PROCESS_PRESET_CUSTOM:
    default:                         return PROCESS_PRESET_LEVEL;
  }
}
