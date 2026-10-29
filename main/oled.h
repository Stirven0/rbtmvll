#ifndef OLED_H
#define OLED_H

#include "config.h"

void oled_init(void);
void oled_tick(const lecturas_ambientales_t lecturas);
void oled_mostrar_datos(void);
void oled_mostrar_modo(modo_operacion_t modo);
bool oled_disponible(void);

#endif /* OLED_H */
