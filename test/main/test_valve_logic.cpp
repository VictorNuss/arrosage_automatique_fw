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

TEST_CASE("extend: la duree demandee s'ajoute au temps restant", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(900, valve_logic_extend_duration(300, 600, 1800));
}

TEST_CASE("extend: le total est borne par la duree max", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_extend_duration(1500, 600, 1800));
}

TEST_CASE("extend: une demande au-dela du max ne depasse pas le max", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_extend_duration(0, 9999, 1800));
    TEST_ASSERT_EQUAL_UINT32(1800, valve_logic_extend_duration(UINT32_MAX, UINT32_MAX, 1800));
}

TEST_CASE("extend: sans temps restant, equivaut au clamp", "[valve_logic]")
{
    TEST_ASSERT_EQUAL_UINT32(600, valve_logic_extend_duration(0, 600, 1800));
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

TEST_CASE("queue: FIFO, la premiere vanne ajoutee sort en premier", "[valve_logic]")
{
    valve_queue_t q = {};
    TEST_ASSERT_TRUE(valve_queue_push(&q, 2));
    TEST_ASSERT_TRUE(valve_queue_push(&q, 0));
    int out = -1;
    TEST_ASSERT_TRUE(valve_queue_pop(&q, &out));
    TEST_ASSERT_EQUAL_INT(2, out);
    TEST_ASSERT_TRUE(valve_queue_pop(&q, &out));
    TEST_ASSERT_EQUAL_INT(0, out);
    TEST_ASSERT_FALSE(valve_queue_pop(&q, &out));
}

TEST_CASE("queue: une vanne deja en attente n'est pas dupliquee et garde sa place", "[valve_logic]")
{
    valve_queue_t q = {};
    valve_queue_push(&q, 1);
    valve_queue_push(&q, 2);
    TEST_ASSERT_TRUE(valve_queue_push(&q, 1));
    TEST_ASSERT_EQUAL_UINT32(2, q.count);
    int out = -1;
    valve_queue_pop(&q, &out);
    TEST_ASSERT_EQUAL_INT(1, out);
}

TEST_CASE("queue: remove retire une vanne au milieu sans changer l'ordre des autres", "[valve_logic]")
{
    valve_queue_t q = {};
    valve_queue_push(&q, 0);
    valve_queue_push(&q, 1);
    valve_queue_push(&q, 2);
    TEST_ASSERT_TRUE(valve_queue_remove(&q, 1));
    TEST_ASSERT_FALSE(valve_queue_remove(&q, 1));
    int out = -1;
    valve_queue_pop(&q, &out);
    TEST_ASSERT_EQUAL_INT(0, out);
    valve_queue_pop(&q, &out);
    TEST_ASSERT_EQUAL_INT(2, out);
}

TEST_CASE("queue: push refuse quand la file est pleine", "[valve_logic]")
{
    valve_queue_t q = {};
    for (int i = 0; i < VALVE_LOGIC_QUEUE_CAPACITY; i++) {
        TEST_ASSERT_TRUE(valve_queue_push(&q, i));
    }
    TEST_ASSERT_FALSE(valve_queue_push(&q, VALVE_LOGIC_QUEUE_CAPACITY));
    TEST_ASSERT_TRUE(valve_queue_push(&q, 0));  // deja presente : pas un ajout
}
