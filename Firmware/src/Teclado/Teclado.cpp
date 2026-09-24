/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de lectura de botones con antirrebote y auto-repetición.
 */

#include "Teclado.h"

Teclado::Teclado(uint32_t debounceMs)
: _debounceMs(debounceMs),
  _repeatDelayMs(450),
  _repeatSlowIntervalMs(160),
  _repeatFastIntervalMs(45),
  _repeatAccelerationSteps(12) {
  // Configuración fija según pinout actual de la placa.
  // Entradas activas en alto con pull-down externo.
  _botones[BTN_MENU].cfg  = {5,  true};
  _botones[BTN_SUBIR].cfg = {7,  true};
  _botones[BTN_BAJAR].cfg = {16, true};
  _botones[BTN_ENTER].cfg = {17, true};

  for (uint8_t i = 0; i < BTN_CANT; i++) {
    _botones[i].lecturaCruda = false;
    _botones[i].estadoEstable = false;
    _botones[i].ultimoCambioMs = 0;
    _botones[i].presionadoDesdeMs = 0;
    _botones[i].ultimoRepeatMs = 0;
    _botones[i].repeatCount = 0;
    _botones[i].eventoPresionado = false;
    _botones[i].eventoLiberado = false;
    _botones[i].eventoClick = false;
    _botones[i].eventoAutoRepeat = false;
  }
}

void Teclado::begin() {
  for (uint8_t i = 0; i < BTN_CANT; i++) {
    // La placa usa pull-down externo; se configura como INPUT.
    pinMode(_botones[i].cfg.pin, INPUT);

    bool estadoInicial = leerBotonCrudo(static_cast<Boton>(i));
    _botones[i].lecturaCruda = estadoInicial;
    _botones[i].estadoEstable = estadoInicial;
    _botones[i].ultimoCambioMs = millis();
    _botones[i].presionadoDesdeMs = estadoInicial ? millis() : 0;
    _botones[i].ultimoRepeatMs = millis();
    _botones[i].repeatCount = 0;

    _botones[i].eventoPresionado = false;
    _botones[i].eventoLiberado = false;
    _botones[i].eventoClick = false;
    _botones[i].eventoAutoRepeat = false;
  }
}

void Teclado::update() {
  const uint32_t ahora = millis();

  for (uint8_t i = 0; i < BTN_CANT; i++) {
    Boton boton = static_cast<Boton>(i);
    EstadoBoton &b = _botones[i];

    bool lecturaActual = leerBotonCrudo(boton);

    // Detecta cambio crudo y reinicia temporizador de debounce
    if (lecturaActual != b.lecturaCruda) {
      b.lecturaCruda = lecturaActual;
      b.ultimoCambioMs = ahora;
    }

    // Si la lectura se mantuvo estable el tiempo suficiente, confirma cambio
    if ((ahora - b.ultimoCambioMs) >= _debounceMs) {
      if (b.estadoEstable != b.lecturaCruda) {
        bool estadoAnterior = b.estadoEstable;
        b.estadoEstable = b.lecturaCruda;

        if (!estadoAnterior && b.estadoEstable) {
          b.eventoPresionado = true;

          // Primer evento inmediato: un toque corto también cambia el valor
          // sin tener que esperar a soltar el botón.
          b.eventoAutoRepeat = true;
          b.presionadoDesdeMs = ahora;
          b.ultimoRepeatMs = ahora;
          b.repeatCount = 0;
        }

        if (estadoAnterior && !b.estadoEstable) {
          b.eventoLiberado = true;
          b.eventoClick = true;
          b.presionadoDesdeMs = 0;
          b.ultimoRepeatMs = 0;
          b.repeatCount = 0;
          b.eventoAutoRepeat = false;
        }
      }
    }

    // Auto-repeat para botones mantenidos.
    // No bloquea: solo genera un evento cuando corresponde según millis().
    if (b.estadoEstable && b.presionadoDesdeMs != 0) {
      const uint32_t tiempoPresionado = ahora - b.presionadoDesdeMs;
      const uint32_t intervalo = intervaloAutoRepeat(b);

      if (tiempoPresionado >= _repeatDelayMs &&
          (uint32_t)(ahora - b.ultimoRepeatMs) >= intervalo) {
        b.eventoAutoRepeat = true;
        b.ultimoRepeatMs = ahora;
        if (b.repeatCount < 65535) b.repeatCount++;
      }
    }
  }
}

bool Teclado::fuePresionado(Boton boton) {
  return consumirEvento(_botones[boton].eventoPresionado);
}

bool Teclado::fueLiberado(Boton boton) {
  return consumirEvento(_botones[boton].eventoLiberado);
}

bool Teclado::fueClick(Boton boton) {
  return consumirEvento(_botones[boton].eventoClick);
}

bool Teclado::fueAutoRepeat(Boton boton) {
  return consumirEvento(_botones[boton].eventoAutoRepeat);
}

bool Teclado::estaPresionado(Boton boton) const {
  return _botones[boton].estadoEstable;
}

const char* Teclado::nombre(Boton boton) const {
  switch (boton) {
    case BTN_MENU:  return "MENU";
    case BTN_SUBIR: return "SUBIR";
    case BTN_BAJAR: return "BAJAR";
    case BTN_ENTER: return "ENTER";
    default:         return "DESCONOCIDO";
  }
}

uint8_t Teclado::pin(Boton boton) const {
  return _botones[boton].cfg.pin;
}

void Teclado::setDebounce(uint32_t debounceMs) {
  _debounceMs = debounceMs;
}

uint32_t Teclado::getDebounce() const {
  return _debounceMs;
}

void Teclado::setAutoRepeat(uint32_t delayInicialMs,
                            uint32_t intervaloLentoMs,
                            uint32_t intervaloRapidoMs,
                            uint8_t pasosAceleracion) {
  _repeatDelayMs = delayInicialMs;
  _repeatSlowIntervalMs = intervaloLentoMs;
  _repeatFastIntervalMs = intervaloRapidoMs;
  _repeatAccelerationSteps = pasosAceleracion;

  // Defensa mínima para evitar configuraciones absurdas.
  if (_repeatFastIntervalMs < 20) _repeatFastIntervalMs = 20;
  if (_repeatSlowIntervalMs < _repeatFastIntervalMs) {
    _repeatSlowIntervalMs = _repeatFastIntervalMs;
  }
}

bool Teclado::leerBotonCrudo(Boton boton) const {
  const EstadoBoton &b = _botones[boton];
  int nivel = digitalRead(b.cfg.pin);

  if (b.cfg.activoAlto) {
    return (nivel == HIGH);
  } else {
    return (nivel == LOW);
  }
}

bool Teclado::consumirEvento(bool &evento) {
  if (evento) {
    evento = false;
    return true;
  }
  return false;
}

uint32_t Teclado::intervaloAutoRepeat(const EstadoBoton& boton) const {
  if (_repeatAccelerationSteps == 0 || _repeatSlowIntervalMs <= _repeatFastIntervalMs) {
    return _repeatFastIntervalMs;
  }

  const uint16_t pasos = boton.repeatCount > _repeatAccelerationSteps
                         ? _repeatAccelerationSteps
                         : boton.repeatCount;

  const uint32_t rango = _repeatSlowIntervalMs - _repeatFastIntervalMs;
  return _repeatSlowIntervalMs - ((rango * pasos) / _repeatAccelerationSteps);
}
