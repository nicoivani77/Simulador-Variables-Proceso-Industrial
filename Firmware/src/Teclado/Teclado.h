/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Interfaz de lectura de botones con antirrebote y auto-repetición.
 */

#ifndef TECLADO_H
#define TECLADO_H

#include <Arduino.h>

class Teclado {
public:
  enum Boton : uint8_t {
    BTN_MENU = 0,
    BTN_SUBIR,
    BTN_BAJAR,
    BTN_ENTER,
    BTN_CANT
  };

  explicit Teclado(uint32_t debounceMs = 25);

  void begin();
  void update();

  bool fuePresionado(Boton boton);
  bool fueLiberado(Boton boton);
  bool fueClick(Boton boton);

  // Evento para SUBIR/BAJAR con repetición automática y aceleración progresiva.
  bool fueAutoRepeat(Boton boton);

  bool estaPresionado(Boton boton) const;

  const char* nombre(Boton boton) const;
  uint8_t pin(Boton boton) const;

  void setDebounce(uint32_t debounceMs);
  uint32_t getDebounce() const;

  void setAutoRepeat(uint32_t delayInicialMs,
                     uint32_t intervaloLentoMs,
                     uint32_t intervaloRapidoMs,
                     uint8_t pasosAceleracion = 12);

private:
  struct ConfigBoton {
    uint8_t pin;
    bool activoAlto;
  };

  struct EstadoBoton {
    ConfigBoton cfg;
    bool lecturaCruda;
    bool estadoEstable;
    uint32_t ultimoCambioMs;
    uint32_t presionadoDesdeMs;
    uint32_t ultimoRepeatMs;
    uint16_t repeatCount;

    bool eventoPresionado;
    bool eventoLiberado;
    bool eventoClick;
    bool eventoAutoRepeat;
  };

  EstadoBoton _botones[BTN_CANT];
  uint32_t _debounceMs;

  uint32_t _repeatDelayMs;
  uint32_t _repeatSlowIntervalMs;
  uint32_t _repeatFastIntervalMs;
  uint8_t _repeatAccelerationSteps;

  bool leerBotonCrudo(Boton boton) const;
  bool consumirEvento(bool &evento);
  uint32_t intervaloAutoRepeat(const EstadoBoton& boton) const;
};

#endif
