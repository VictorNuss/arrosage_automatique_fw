#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file valve_manager.h
 * @ingroup valve
 * @brief API publique du gestionnaire de vannes.
 *
 * Thread-safe : l'etat runtime des vannes est protege par un mutex interne,
 * pris pour la duree minimale necessaire (jamais de blocage/MQTT dedans).
 */

/**
 * @ingroup valve
 * @brief Nombre de vannes configurees (voir valve_config.cpp).
 *
 * Ajouter une vanne = ajouter une ligne dans le tableau de config ; cette
 * constante suit automatiquement, aucun autre code n'a besoin d'etre modifie.
 */
extern const size_t VALVE_COUNT;

/**
 * @ingroup valve
 * @brief Initialise le GPIO et le timer d'auto-fermeture de chaque vanne.
 * @note A appeler une seule fois au demarrage, avant tout autre appel du module.
 */
void valve_manager_init(void);

/**
 * @ingroup valve
 * @brief Ouvre une vanne pour une duree donnee.
 *
 * La duree est clampee en interne a la duree max configuree pour cette
 * vanne (securite : impossible d'ouvrir une vanne indefiniment ou trop
 * longtemps). Si l'armement du timer d'auto-fermeture echoue, l'ouverture
 * est refusee plutot que de laisser la vanne ouverte sans filet.
 *
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @param duration_s Duree d'ouverture demandee, en secondes
 * @return false si idx est hors bornes ou si l'armement du timer a echoue
 */
bool valve_manager_open(int idx, uint32_t duration_s);

/**
 * @ingroup valve
 * @brief Ferme immediatement une vanne (annule le timer d'auto-fermeture actif).
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @return false si idx est hors bornes
 */
bool valve_manager_close(int idx);

/**
 * @ingroup valve
 * @brief Ferme toutes les vannes (arret d'urgence).
 */
void valve_manager_close_all(void);

/**
 * @ingroup valve
 * @brief Recherche l'index d'une vanne par sa cle du contrat MQTT (ex "vanne_1").
 * @param mqtt_key Cle du contrat MQTT
 * @return L'index correspondant, ou -1 si aucune vanne ne correspond
 */
int valve_manager_find_by_key(const char* mqtt_key);

/**
 * @ingroup valve
 * @brief Etat courant d'une vanne (protege par mutex interne).
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @return true si la vanne est ouverte, false si fermee ou si idx est hors bornes
 */
bool valve_manager_is_open(int idx);

/**
 * @ingroup valve
 * @brief Cle du contrat MQTT d'une vanne (donnee de config en lecture seule).
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @return La cle (ex "vanne_1"), ou nullptr si idx est hors bornes
 */
const char* valve_manager_mqtt_key(int idx);

/**
 * @ingroup valve
 * @brief Nom humain d'une vanne (donnee de config en lecture seule), pour les logs.
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @return Le nom (ex "Pelouse"), ou nullptr si idx est hors bornes
 */
const char* valve_manager_name(int idx);
