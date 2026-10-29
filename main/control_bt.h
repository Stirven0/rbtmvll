#ifndef CONTROL_BT_H
#define CONTROL_BT_H

#include "config.h"

void control_bt_init(void);
void control_bt_tick(void);
void control_bt_activar(void);
void control_bt_desactivar(void);
bool control_bt_habilitado(void);
void control_bt_imprimir_estado(void);
void control_bt_olvidar_claves(void);
void control_bt_reiniciar_busqueda(void);
void control_bt_alternar_debug(void);
bool control_bt_conectado(void);
const char *control_bt_nombre(void);
bool control_bt_consumir_solicitud_datos(void);
bool control_bt_consumir_solicitud_auto(void);

#endif /* CONTROL_BT_H */
