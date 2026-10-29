#ifndef SENSORES_H
#define SENSORES_H

#include "config.h"

void sensores_init(void);
bool sensores_tick(void);                 /* true si hubo nueva lectura */
const lecturas_ambientales_t *sensores_obtener_puntero(void);

#endif /* SENSORES_H */
