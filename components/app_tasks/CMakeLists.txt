idf_component_register(
    SRCS "state_json.cpp" "command_task.cpp" "sensor_task.cpp" "diag_console.cpp"
    INCLUDE_DIRS "include"
    REQUIRES command valve sensors net console cjson
)
