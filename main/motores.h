#ifndef MOTORES_H
#define MOTORES_H

#include "config.h"

void motores_init(void);
void motores_tick(void);
void motores_mover(direccion_movimiento_t direccion, uint32_t duracion_ms);
void motores_control_continuo(direccion_movimiento_t direccion);
void motores_detener(void);
void motores_demo(void);
bool motores_demo_activa(void);
const char *motores_estado_texto(void);

#endif /* MOTORES_H */
