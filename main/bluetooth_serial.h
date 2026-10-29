#ifndef BLUETOOTH_SERIAL_H
#define BLUETOOTH_SERIAL_H

#include "config.h"

void bluetooth_serial_init(void);
void bluetooth_serial_tick(void);
void bluetooth_serial_descartar(void);
const char *bluetooth_serial_estado(void);

#endif /* BLUETOOTH_SERIAL_H */
