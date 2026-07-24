#pragma once

/**
 * @file wifi.h
 * @ingroup net
 * @brief Connexion WiFi station.
 */

/**
 * @ingroup net
 * @brief Initialise esp_netif/l'event loop par defaut et demarre la connexion WiFi (STA).
 *
 * Utilise le SSID/mot de passe configures via `idf.py menuconfig`
 * (Kconfig.projbuild). La reconnexion sur perte de signal est geree
 * automatiquement (WIFI_EVENT_STA_DISCONNECTED).
 *
 * @note A appeler une seule fois, apres net_events_init().
 */
void net_wifi_init(void);
