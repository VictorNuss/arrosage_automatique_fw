#include "unity.h"

#include "valve_logic.h"

TEST_CASE("clamp: duree dans les bornes est inchangee", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(300, valve_logic_clamp_duration(300, 1800));
}

TEST_CASE("clamp: duree superieure au max est ramenee au max", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_clamp_duration(9999, 1800));
}

TEST_CASE("clamp: duree egale au max est inchangee", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_clamp_duration(1800, 1800));
}

TEST_CASE("clamp: duree nulle est ramenee au max (pas d'ouverture indefinie)", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_clamp_duration(0, 1800));
}

TEST_CASE("find_by_key: trouve l'index correct", "[valve_logic]")
{
    const char* keys[] = {"vanne_1", "vanne_2", "vanne_3"};
    TEST_ASSERT_EQUAL_INT(1, valve_logic_find_by_key(keys, 3, "vanne_2"));
}

TEST_CASE("find_by_key: premiere et derniere entree", "[valve_logic]")
{
    const char* keys[] = {"vanne_1", "vanne_2", "vanne_3"};
    TEST_ASSERT_EQUAL_INT(0, valve_logic_find_by_key(keys, 3, "vanne_1"));
    TEST_ASSERT_EQUAL_INT(2, valve_logic_find_by_key(keys, 3, "vanne_3"));
}

TEST_CASE("find_by_key: cle absente retourne -1", "[valve_logic]")
{
    const char* keys[] = {"vanne_1", "vanne_2", "vanne_3"};
    TEST_ASSERT_EQUAL_INT(-1, valve_logic_find_by_key(keys, 3, "vanne_9"));
}

TEST_CASE("find_by_key: cle nulle retourne -1 sans crasher", "[valve_logic]")
{
    const char* keys[] = {"vanne_1"};
    TEST_ASSERT_EQUAL_INT(-1, valve_logic_find_by_key(keys, 1, nullptr));
}

TEST_CASE("find_by_key: tableau vide retourne -1", "[valve_logic]")
{
    const char* keys[] = {"vanne_1"};
    TEST_ASSERT_EQUAL_INT(-1, valve_logic_find_by_key(keys, 0, "vanne_1"));
}

TEST_CASE("clamp: duree superieure de un seul cran au max est ramenee au max", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(600, valve_logic_clamp_duration(601, 600));
}

TEST_CASE("clamp: duree minimale (1s) est inchangee", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1, valve_logic_clamp_duration(1, 1800));
}

TEST_CASE("find_by_key: cle correspondant a un prefixe d'une autre cle n'est pas confondue", "[valve_logic]")
{
    const char* keys[] = {"vanne_1", "vanne_10"};
    TEST_ASSERT_EQUAL_INT(0, valve_logic_find_by_key(keys, 2, "vanne_1"));
    TEST_ASSERT_EQUAL_INT(1, valve_logic_find_by_key(keys, 2, "vanne_10"));
}

TEST_CASE("find_by_key: recherche insensible a rien - la casse compte", "[valve_logic]")
{
    const char* keys[] = {"vanne_1"};
    TEST_ASSERT_EQUAL_INT(-1, valve_logic_find_by_key(keys, 1, "Vanne_1"));
}
