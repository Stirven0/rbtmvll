#ifndef SERVO_ULTRASONIDO_H
#define SERVO_ULTRASONIDO_H

#include "config.h"

void servo_ultrasonido_init(void);
void servo_ultrasonido_tick(bool escanear);
void servo_ultrasonido_centrar(void);

float servo_ultrasonido_distancia_cm(uint8_t indice);
float servo_ultrasonido_distancia_frente_cm(void);
float servo_ultrasonido_distancia_izquierda_cm(void);
float servo_ultrasonido_distancia_derecha_cm(void);
bool servo_ultrasonido_obstaculo_frente(void);
uint8_t servo_ultrasonido_angulo_actual(void);
uint8_t servo_ultrasonido_indice_actual(void);
bool servo_ultrasonido_escaner_activo(void);
const char *servo_ultrasonido_direccion_mas_libre(void);

#endif /* SERVO_ULTRASONIDO_H */
