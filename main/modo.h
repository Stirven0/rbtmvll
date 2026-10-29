#ifndef MODO_H
#define MODO_H

#include "config.h"

void modo_init(void);
bool modo_cambiar(modo_operacion_t nuevo);
modo_operacion_t modo_actual(void);
const char *modo_nombre(void);
bool modo_es_manual(void);
bool modo_es_automatico(void);

#endif /* MODO_H */
