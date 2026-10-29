#ifndef COMANDOS_H
#define COMANDOS_H

#include "config.h"

/* Procesa comandos de movimiento desde USB (UART0). */
void comandos_procesar_serial(const char *origen);

#endif /* COMANDOS_H */
