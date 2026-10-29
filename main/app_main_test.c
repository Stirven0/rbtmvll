/* ========================================================================
 *  IndoorWatch - Punto de entrada para pruebas por módulo
 *
 *  Use este archivo en lugar de app_main.c para ejecutar el menú de
 *  pruebas. Luego restaure app_main.c para el firmware normal.
 * ======================================================================== */

#include "test_menu.h"

void app_main(void)
{
    test_menu_run();
}
