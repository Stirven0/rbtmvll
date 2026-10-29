#ifndef ALERTAS_H
#define ALERTAS_H

#include "config.h"

void alertas_init(void);
void alertas_tick(const lecturas_ambientales_t lecturas);

#endif /* ALERTAS_H */
