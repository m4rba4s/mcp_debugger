#include <gtest/gtest.h>

TEST(SimpleTest, TrueIsTrue) {
    EXPECT_TRUE(true);
}

TEST(SimpleTest, OneEqualsOne) {
    EXPECT_EQ(1, 1);
}

TEST(SimpleTest, StringComparison) {
    std::string hello = "hello";
    EXPECT_EQ("hello", hello);
} 