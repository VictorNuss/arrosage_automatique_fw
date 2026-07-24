#pragma once

#include <cstddef>

/**
 * @file time_sync.h
 * @ingroup net
 * @brief Synchronisation de l'horloge via NTP.
 */

/**
 * @ingroup net
 * @brief Enregistre le service SNTP (demarre automatiquement des que le WiFi obtient une IP).
 *
 * N'attend pas la synchronisation : voir ::NET_TIME_SYNCED_BIT
 * (net_events.h) pour une attente bornee cote appelant si necessaire.
 *
 * @note A appeler une seule fois, apres net_events_init().
 */
void net_time_sync_init(void);

/**
 * @ingroup net
 * @brief Formate l'heure courante en ISO8601 UTC ("2026-07-16T10:00:00Z").
 *
 * Si l'heure n'a pas encore ete synchronisee via NTP, reflete l'epoque par
 * defaut du systeme (1970-01-01) - l'appelant doit avoir verifie
 * ::NET_TIME_SYNCED_BIT au prealable si un horodatage fiable est requis.
 *
 * @param[out] buf Buffer de destination
 * @param len Taille du buffer
 */
void net_time_sync_now_iso8601(char* buf, size_t len);
