#include <cstring>

#include "unity.h"

#include "command.h"

TEST_CASE("open valide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":600}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::OpenValve);
    TEST_ASSERT_EQUAL_STRING("vanne_1", cmd.valve_key);
    TEST_ASSERT_EQUAL_UINT32(600, cmd.duration_s);
}

TEST_CASE("close valide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_2\",\"action\":\"close\"}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::CloseValve);
    TEST_ASSERT_EQUAL_STRING("vanne_2", cmd.valve_key);
}

TEST_CASE("stop_all valide, ignore vanne/duration_s superflus", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"stop_all\",\"vanne\":\"vanne_1\",\"duration_s\":10}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::StopAll);
}

TEST_CASE("open sans duration_s est invalide (pas de duree implicite)", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::Invalid);
}

TEST_CASE("open sans vanne est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"open\",\"duration_s\":60}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("open avec duration_s negatif ou nul est invalide", "[command_parser]")
{
    Command cmd;
    const char* json_neg = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":-5}";
    TEST_ASSERT_FALSE(command_parse(json_neg, strlen(json_neg), &cmd));

    const char* json_zero = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":0}";
    TEST_ASSERT_FALSE(command_parse(json_zero, strlen(json_zero), &cmd));
}

TEST_CASE("close sans vanne est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"close\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("vanne avec valeur non-string est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":42,\"action\":\"close\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("action inconnue est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"do_something\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("action absente est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("JSON malforme est invalide, ne crashe pas", "[command_parser]")
{
    Command cmd;
    const char* json = "{ceci n'est pas du json";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("payload vide est invalide", "[command_parser]")
{
    Command cmd;
    TEST_ASSERT_FALSE(command_parse("", 0, &cmd));
}

TEST_CASE("duration_s sous forme de chaine est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":\"600\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("duration_s flottant est tronque a l'entier", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":12.9}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_EQUAL_UINT32(12, cmd.duration_s);
}

// Regression : duration_s entre 0 et 1 (exclus) validait auparavant ">0" sur
// le double avant troncature, produisant duration_s=0 - une valeur ensuite
// traitee par valve_logic_clamp_duration comme "non specifiee" et ouvrant la
// vanne pour sa duree MAX configuree au lieu d'etre rejetee.
TEST_CASE("duration_s fractionnaire entre 0 et 1 est invalide (pas de troncature vers 0)", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":0.5}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::Invalid);
}

TEST_CASE("duration_s demesurement grand est invalide (pas de cast hors plage uint32_t)", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"vanne_1\",\"action\":\"open\",\"duration_s\":1e20}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("cle de vanne trop longue est tronquee sans deborder", "[command_parser]")
{
    Command cmd;
    const char* json =
        "{\"vanne\":\"vanne_avec_un_nom_beaucoup_trop_long_pour_le_buffer\",\"action\":\"close\"}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    // sizeof(Command::valve_key) == 16 : 15 caracteres utiles + '\0'
    TEST_ASSERT_EQUAL_INT(15, strlen(cmd.valve_key));
}

TEST_CASE("action non-string est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":42}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("objet JSON vide est invalide", "[command_parser]")
{
    Command cmd;
    TEST_ASSERT_FALSE(command_parse("{}", 2, &cmd));
}

TEST_CASE("tableau JSON au lieu d'un objet est invalide, ne crashe pas", "[command_parser]")
{
    Command cmd;
    const char* json = "[1,2,3]";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("champs superflus inconnus sont ignores sans faire echouer le parsing", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"stop_all\",\"debug\":true,\"extra\":{\"n\":1}}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::StopAll);
}

TEST_CASE("vanne vide (chaine vide) est invalide", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"vanne\":\"\",\"action\":\"close\"}";
    TEST_ASSERT_FALSE(command_parse(json, strlen(json), &cmd));
}

TEST_CASE("get_status valide, ignore vanne/duration_s superflus", "[command_parser]")
{
    Command cmd;
    const char* json = "{\"action\":\"get_status\",\"vanne\":\"vanne_1\",\"duration_s\":10}";
    TEST_ASSERT_TRUE(command_parse(json, strlen(json), &cmd));
    TEST_ASSERT_TRUE(cmd.type == CommandType::GetStatus);
}
