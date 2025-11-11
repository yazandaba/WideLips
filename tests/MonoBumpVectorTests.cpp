#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <type_traits>
#include "MonoBumpVector.h"

using namespace WideLips;
using namespace testing;

namespace WideLips::Tests {

    struct TrivialPOD { int a; int b; };

    class MonoBumpVectorTest : public Test {};

    TEST_F(MonoBumpVectorTest, AlignmentAndTypeTraits) {
        EXPECT_EQ(alignof(MonoBumpVector<int>), 16);
        EXPECT_TRUE(std::is_trivially_copyable_v<TrivialPOD>);
        EXPECT_TRUE(std::is_trivially_destructible_v<TrivialPOD>);
    }

    TEST_F(MonoBumpVectorTest, ConstructionAndEmptySize) {
        MonoBumpVector<int> vec(8);
        EXPECT_TRUE(vec.Empty());
        EXPECT_EQ(vec.Size(), 0u);
    }

    TEST_F(MonoBumpVectorTest, EmplaceBackBackAndPop) {
        MonoBumpVector<int> vec(16);
        auto* p0 = vec.EmplaceBack(10);
        ASSERT_NE(p0, nullptr);
        EXPECT_FALSE(vec.Empty());
        EXPECT_EQ(vec.Size(), 1u);
        EXPECT_EQ(vec.Back(), 10);

        auto* p1 = vec.EmplaceBack(20);
        ASSERT_NE(p1, nullptr);
        EXPECT_EQ(vec.Size(), 2u);
        EXPECT_EQ(vec.Back(), 20);

        // At within range
        EXPECT_EQ(*vec.At(0), 10);
        EXPECT_EQ(*vec.At(1), 20);

        vec.PopBack();
        EXPECT_EQ(vec.Size(), 1u);
        EXPECT_EQ(vec.Back(), 10);
    }

    TEST_F(MonoBumpVectorTest, EmplaceMultipleAndReuse) {
        MonoBumpVector<TrivialPOD> vec(32);
        for (int i = 0; i < 5; ++i) {
            auto* p = vec.EmplaceBack(TrivialPOD{i, i*i});
            ASSERT_NE(p, nullptr);
        }
        EXPECT_EQ(vec.Size(), 5u);
        EXPECT_EQ(vec.Back().a, 4);
        EXPECT_EQ(vec.Back().b, 16);

        // Verify contiguous storage via At
        for (int i = 0; i < 5; ++i) {
            auto* p = vec.At(static_cast<std::size_t>(i));
            ASSERT_NE(p, nullptr);
            EXPECT_EQ(p->a, i);
            EXPECT_EQ(p->b, i*i);
        }

        // Reuse resets to empty
        vec.Reuse();
        EXPECT_TRUE(vec.Empty());
        EXPECT_EQ(vec.Size(), 0u);

        // Insert again after reuse
        vec.EmplaceBack({42, 1764});
        EXPECT_FALSE(vec.Empty());
        EXPECT_EQ(vec.Size(), 1u);
        EXPECT_EQ(vec.Back().a, 42);
        EXPECT_EQ(vec.Back().b, 1764);
    }
}
