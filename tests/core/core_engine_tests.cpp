#include <gtest/gtest.h>

TEST(DummyTest, AlwaysPasses) {
    EXPECT_TRUE(true);
}

TEST(SimpleTest, BasicAssertion) {
    EXPECT_EQ(1, 1);
} 
