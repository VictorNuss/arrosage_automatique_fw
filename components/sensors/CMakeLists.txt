idf_component_register(
    SRCS
        "sensor_manager.cpp"
        "water_level_sensor.cpp"
        "soil_humidity_sensor.cpp"
        "ds18b20_temperature_sensor.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_gpio esp_adc esp_timer cjson freertos sensor_logic
)
