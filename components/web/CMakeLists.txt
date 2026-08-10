idf_component_register(
    SRCS "web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server command app_tasks cjson ota
)
