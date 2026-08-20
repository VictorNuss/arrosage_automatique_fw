#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file command.h
 * @ingroup command
 * @brief Types et parseur du contrat MQTT/HTTP commande.
 */

/**
 * @ingroup command
 * @brief Type d'une commande recue sur le topic commande.
 */
enum class CommandType {
    Invalid,    /**< Payload invalide, aucune action a effectuer */
    OpenValve,  /**< Ouvrir une vanne pour Command::duration_s secondes */
    CloseValve, /**< Fermer une vanne */
    StopAll,    /**< Arret d'urgence : fermer toutes les vannes */
    GetStatus,  /**< Republier l'etat courant de toutes les vannes et la derniere valeur connue de chaque capteur */
};

/**
 * @ingroup command
 * @brief Commande validee, prete a etre appliquee au gestionnaire de vannes.
 */
struct Command {
    CommandType type = CommandType::Invalid;
    char valve_key[16] = {0};  /**< Cle du contrat MQTT (ex "vanne_1") ; vide pour StopAll */
    uint32_t duration_s = 0;   /**< Duree en secondes ; pertinent seulement pour OpenValve */
};

/**
 * @ingroup command
 * @brief Parse le payload JSON recu sur le topic `arrosage/<device_id>/commande`.
 *
 * Retourne false (et out->type = CommandType::Invalid) pour tout payload
 * malforme, incomplet ou avec des champs manquants/de type incorrect - un
 * payload reseau ne doit jamais faire planter le parseur. Aucune duree
 * implicite n'est appliquee pour une ouverture : un duration_s absent,
 * nul ou hors bornes est rejete plutot que devine.
 *
 * @param json Payload JSON brut (pas necessairement termine par '\\0')
 * @param len Longueur du payload en octets
 * @param[out] out Commande resultante, ecrite dans tous les cas (voir CommandType::Invalid)
 * @return true si le payload est un JSON valide respectant le format attendu
 */
bool command_parse(const char* json, size_t len, Command* out);
