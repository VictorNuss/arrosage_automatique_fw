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
 * @brief Initialise le GPIO et les timers (auto-fermeture, transition) de chaque vanne.
 * @note A appeler une seule fois au demarrage, avant tout autre appel du module.
 */
void valve_manager_init(void);

/**
 * @ingroup valve
 * @brief Lance l'ouverture d'une vanne pour une duree donnee.
 *
 * La duree est clampee en interne a la duree max configuree pour cette
 * vanne (securite : impossible d'ouvrir une vanne indefiniment ou trop
 * longtemps). Si l'armement du timer d'auto-fermeture echoue, l'ouverture
 * est refusee plutot que de laisser la vanne ouverte sans filet.
 *
 * Le GPIO bascule immediatement, mais l'etat passe d'abord par
 * "transitioning" (evenement MQTT publie tout de suite) avant de devenir
 * "open" `CONFIG_ARROSAGE_VALVE_TRANSITION_DELAY_S` secondes plus tard (voir
 * valve_manager_state_string()) : le temps que le condensateur de demarrage
 * de la vanne motorisee permette au moteur d'ouvrir reellement le passage
 * d'eau - annoncer "open" plus tot serait faux.
 *
 * @param idx Index de la vanne (0..VALVE_COUNT-1)
 * @param duration_s Duree d'ouverture demandee, en secondes
 * @return false si idx est hors bornes ou si l'armement du timer a echoue
 */
bool valve_manager_open(int idx, uint32_t duration_s);

/**
 * @ingroup valve
 * @brief Lance la fermeture d'une vanne (annule le timer d'auto-fermeture actif).
 *
 * Meme delai de transition qu'a l'ouverture (voir valve_manager_open()) avant
 * que l'etat ne devienne "closed".
 *
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
 * @brief Republie l'etat courant de toutes les vannes sur la queue MQTT evenementielle.
 *
 * A appeler en reponse a une commande `get_status` (voir CommandType::GetStatus) :
 * l'etat d'une vanne est toujours connu (pas d'ambiguite "jamais lue" comme
 * pour un capteur), donc toutes les vannes sont republiees sans exception.
 */
void valve_manager_publish_all(void);

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
 * @return "open", "closed", "transitioning" (ouverture/fermeture en cours), ou nullptr si idx est hors bornes
 */
const char* valve_manager_state_string(int idx);

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
