idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash valve sensors net app_tasks web ota
)
