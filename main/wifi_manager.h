#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"

void wifi_init(void);
void wifi_tick(void);
void wifi_apagar(void);
void wifi_encender(void);
bool wifi_habilitado(void);
bool wifi_conectado(void);
const char *wifi_ip(void);
int wifi_rssi(void);

#endif /* WIFI_MANAGER_H */
