#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include "config.h"

void telegram_init(void);
void telegram_tick(const lecturas_ambientales_t lecturas);
bool telegram_enviar_mensaje(const char *texto);

#endif /* TELEGRAM_BOT_H */
