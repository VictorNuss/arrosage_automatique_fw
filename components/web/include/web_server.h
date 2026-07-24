#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @file web_server.h
 * @ingroup web
 * @brief Serveur HTTP local de test.
 */

/**
 * @ingroup web
 * @brief Demarre un serveur HTTP local (port 80) permettant de tester le firmware
 * sans dependre du backend/broker MQTT.
 *
 * Endpoints exposes :
 * | Route | Description |
 * |---|---|
 * | `GET /` | Page HTML de test (etat courant + controle des vannes) |
 * | `GET /api/state` | JSON d'etat - exactement le meme format que le contrat MQTT `arrosage/<device_id>/etat` (voir state_json_build()) |
 * | `POST /api/command` | JSON de commande - exactement le meme format que le contrat MQTT `arrosage/<device_id>/commande` (voir command_parse()) |
 *
 * La commande recue est deposee dans la meme command_queue que celle
 * utilisee par net_mqtt_init() : tester ici revient a tester exactement ce
 * que le vrai backend enverra plus tard.
 *
 * @warning HTTP simple, sans authentification - reseau local de confiance
 * uniquement (meme modele que le broker Mosquitto anonyme).
 *
 * @param command_queue Queue FreeRTOS partagee avec net_mqtt_init(), ou deposer les commandes recues
 */
void web_server_start(QueueHandle_t command_queue);
