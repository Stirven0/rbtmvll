#ifndef COMANDOS_H
#define COMANDOS_H

#include "config.h"

/* Procesa comandos de movimiento desde USB (UART0). */
void comandos_procesar_serial(const char *origen);

/* Procesa comandos desde Bluetooth serial (UART2). */
void comandos_procesar_uart2(const char *origen, bool eco);

/* Reseta el parser de UART2 (usado cuando el mando tiene prioridad). */
void comandos_reset_uart2(void);

#endif /* COMANDOS_H */
