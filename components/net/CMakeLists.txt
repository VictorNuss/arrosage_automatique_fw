idf_component_register(
    SRCS "net_events.cpp" "wifi.cpp" "time_sync.cpp" "mqtt.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif esp_event esp_app_format mqtt command
)
