#pragma once

/**
 * @file diag_console.h
 * @ingroup app_tasks
 * @brief Console de diagnostic materiel (REPL UART).
 */

/**
 * @ingroup app_tasks
 * @brief Demarre une console interactive (REPL sur UART) de diagnostic materiel.
 *
 * Expose les commandes `valve open/close <index> [duree_s]` et `sensor`.
 * Reutilise les memes composants (valve_manager, sensor_manager) que le
 * mode normal - aucun code duplique.
 *
 * @note Usage : bring-up progressif sur le materiel reel (voir
 * docs/bring_up_checklist.md), pas destine a un usage en production continue.
 */
void diag_console_start(void);
