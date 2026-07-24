# WHOLE_ARCHIVE : TEST_CASE() s'enregistre via un constructeur C++ statique,
# jamais reference ailleurs - sans ce flag l'editeur de liens elimine ces
# fichiers objet de l'archive et aucun test n'est detecte a l'execution.
idf_component_register(
    SRCS "test_main.cpp" "test_command_parser.cpp" "test_valve_logic.cpp" "test_sensor_logic.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity command valve_logic sensor_logic
    WHOLE_ARCHIVE
)
