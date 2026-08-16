idf_component_register(
    SRCS "valve_manager.cpp" "valve_config.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_gpio esp_timer valve_logic mqtt_event
)
