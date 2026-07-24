#include <cstdlib>

#include "unity.h"

extern "C" void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    int failures = UNITY_END();

    // Sur la cible hote "linux", le simulateur FreeRTOS continue de tourner
    // indefiniment apres le retour de app_main (comme sur materiel reel) :
    // il faut donc terminer explicitement le processus, avec le nombre
    // d'echecs comme code de sortie (0 = tous les tests passent).
    exit(failures == 0 ? 0 : 1);
}
