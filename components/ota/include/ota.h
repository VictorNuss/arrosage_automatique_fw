#pragma once

#include <cstddef>

#include "esp_err.h"

/**
 * @file ota.h
 * @ingroup ota
 * @brief Mise a jour du firmware par ecriture directe dans la partition OTA
 * inactive (esp_ota_ops), pilotee par un flux de donnees externe (ex: corps
 * d'une requete HTTP - voir components/web/web_server.cpp).
 *
 * Necessite une table de partitions avec au moins deux slots app (voir
 * sdkconfig.defaults, CONFIG_PARTITION_TABLE_TWO_OTA) - incompatible avec un
 * device deja flashe avec l'ancienne table "single app" sans reflash complet.
 */

/**
 * @ingroup ota
 * @brief Demarre une mise a jour OTA vers la partition inactive.
 * @param image_size Taille attendue de l'image, si connue (ex Content-Length
 * HTTP) ; 0 si inconnue.
 * @return ESP_OK si demarree avec succes ; ESP_ERR_INVALID_STATE si une mise
 * a jour est deja en cours ; ESP_FAIL si aucune partition OTA n'est disponible.
 */
esp_err_t ota_begin(size_t image_size);

/**
 * @ingroup ota
 * @brief Ecrit un fragment de l'image recue. Peut etre appelee plusieurs
 * fois avec des fragments de taille arbitraire (aucun alignement requis).
 * @return ESP_ERR_INVALID_STATE si ota_begin() n'a pas ete appelee au prealable.
 */
esp_err_t ota_write(const void* data, size_t len);

/**
 * @ingroup ota
 * @brief Termine et valide la mise a jour, puis bascule le prochain
 * demarrage sur la nouvelle image.
 *
 * L'appelant doit redemarrer explicitement (esp_restart()) pour appliquer
 * la mise a jour - cette fonction ne redemarre pas elle-meme, afin de
 * laisser l'appelant terminer proprement ce qu'il a en cours (ex: envoyer
 * la reponse HTTP avant de couper la connexion).
 *
 * @return ESP_OK si l'image est valide et le prochain boot bascule dessus ;
 * une erreur sinon (image corrompue/incomplete - l'ancienne image reste active).
 */
esp_err_t ota_finish(void);

/**
 * @ingroup ota
 * @brief Annule une mise a jour en cours (image partielle/invalide) sans
 * modifier la partition de boot active.
 */
void ota_abort(void);

/**
 * @ingroup ota
 * @brief Confirme que l'image actuellement demarree fonctionne correctement,
 * annulant tout rollback automatique en attente.
 *
 * @note A appeler une fois au demarrage, apres l'initialisation complete de
 * l'application (voir main/app_main.cpp) - sans cet appel, un redemarrage
 * inattendu de l'image fraichement flashee declenche un retour automatique
 * sur la precedente (voir CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE).
 */
void ota_confirm_boot_ok(void);
