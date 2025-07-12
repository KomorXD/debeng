#include <gtest/gtest.h>

#include "eng/containers/registry.hpp"

using namespace eng::ecs;

TEST(Registry, OneEntity) {
    Registry reg = Registry::create();
    EntityID ent = reg.create_entity();

    ASSERT_FALSE(reg.has_component<int>(ent))
        << "Empty entity should not have an INT component";

    int &val = reg.add_component<int>(ent);
    ASSERT_EQ(reg.get_component<int>(ent), 0)
        << "INT's default value should be zero";

    ASSERT_TRUE(reg.has_component<int>(ent))
        << "Entity should have an INT component";

    val = 12;
    ASSERT_EQ(reg.get_component<int>(ent), 12)
        << "INT's changed value should be 12";

    reg.get_component<int>(ent) = 24;
    ASSERT_EQ(reg.get_component<int>(ent), 24)
        << "INT's changed value should be 24";

    reg.remove_component<int>(ent);
    ASSERT_FALSE(reg.has_component<int>(ent))
        << "Empty entity should not have an INT component after deleting it";

    ASSERT_EQ(reg.add_component<int>(ent), 0)
        << "INT's default value should be zero, even if entity had INT "
           "component before";

    reg.destroy();
}

TEST(Registry, ManyEntities) {
    Registry reg = Registry::create();
    EntityID e1 = reg.create_entity();
    EntityID e2 = reg.create_entity();
    EntityID e3 = reg.create_entity();

    reg.add_component<int>(e1) = 1;
    reg.add_component<float>(e1) = 1.0f;

    reg.add_component<int>(e2) = 2;
    reg.add_component<char>(e2) = '2';

    reg.add_component<int>(e3) = 3;
    reg.add_component<float>(e3) = 3.0f;
    reg.add_component<char>(e3) = '3';

    ASSERT_TRUE(reg.has_component<int>(e1))
        << "e1 should have an INT component";
    ASSERT_TRUE(reg.has_component<float>(e1))
        << "e1 should have a FLOAT component";
    ASSERT_FALSE(reg.has_component<char>(e1))
        << "e1 should NOT have a CHAR component";

    ASSERT_TRUE(reg.has_component<int>(e2))
        << "e2 should have an INT component";
    ASSERT_TRUE(reg.has_component<char>(e2))
        << "e2 should have a CHAR component";
    ASSERT_FALSE(reg.has_component<float>(e2))
        << "e2 should NOT have a FLOAT component";

    ASSERT_TRUE(reg.has_component<int>(e3))
        << "e3 should have an INT component";
    ASSERT_TRUE(reg.has_component<float>(e3))
        << "e3 should have a FLOAT component";
    ASSERT_TRUE(reg.has_component<char>(e3))
        << "e3 should have a CHAR component";

    reg.remove_component<int>(e3);

    ASSERT_TRUE(reg.has_component<int>(e1))
        << "e1 should have an INT component";
    ASSERT_TRUE(reg.has_component<int>(e2))
        << "e2 should have an INT component";
    ASSERT_FALSE(reg.has_component<int>(e3))
        << "e3 should NOT have an INT component";

    reg.remove_component<float>(e1);

    ASSERT_FALSE(reg.has_component<float>(e1))
        << "e1 should NOT have a FLOAT component";
    ASSERT_FALSE(reg.has_component<float>(e2))
        << "e2 should NOT have a FLOAT component";
    ASSERT_TRUE(reg.has_component<float>(e3))
        << "e3 should have a FLOAT component";

    ASSERT_EQ(reg.get_component<int>(e1), 1) << "e1's INT changed";

    ASSERT_EQ(reg.get_component<int>(e2), 2) << "e2's INT changed";
    ASSERT_EQ(reg.get_component<char>(e2), '2') << "e2's CHAR changed";

    ASSERT_EQ(reg.get_component<float>(e3), 3.0f) << "e3's FLOAT changed";
    ASSERT_EQ(reg.get_component<char>(e3), '3') << "e3's CHAR changed";

    reg.destroy();
}

TEST(Registry, SharingArchetype) {
    Registry reg = Registry::create();
    EntityID e1 = reg.create_entity();
    EntityID e2 = reg.create_entity();

    /*  Getting to the same archetype with different paths:
            1. int -> float -> char
            2. int -> char -> float */
    reg.add_component<int>(e1) = 1;
    reg.add_component<float>(e1) = 1.0f;
    reg.add_component<char>(e1) = '1';

    reg.add_component<int>(e2) = 2;
    reg.add_component<float>(e2) = 2.0f;
    reg.add_component<char>(e2) = '2';

    ASSERT_EQ(reg.get_component<int>(e1), 1) << "e1's INT changed";
    ASSERT_EQ(reg.get_component<float>(e1), 1.0f) << "e1's FLOAT changed";
    ASSERT_EQ(reg.get_component<char>(e1), '1') << "e1's CHAR changed";

    ASSERT_EQ(reg.get_component<int>(e2), 2) << "e2's INT changed";
    ASSERT_EQ(reg.get_component<float>(e2), 2.0f) << "e2's FLOAT changed";
    ASSERT_EQ(reg.get_component<char>(e2), '2') << "e2's CHAR changed";

    reg.remove_component<float>(e1);
    reg.remove_component<float>(e2);

    ASSERT_EQ(reg.get_component<int>(e1), 1) << "e1's INT changed";
    ASSERT_EQ(reg.get_component<char>(e1), '1') << "e1's CHAR changed";

    ASSERT_EQ(reg.get_component<int>(e2), 2) << "e2's INT changed";
    ASSERT_EQ(reg.get_component<char>(e2), '2') << "e2's CHAR changed";

    reg.destroy();
}
