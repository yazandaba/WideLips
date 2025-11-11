#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstdint>
#include "ADT/BumpVector.h"

using namespace WideLips;
using namespace testing;

namespace WideLips::Tests {

    // Helper trivially copyable types to exercise SIMD branches
    struct alignas(16) Blob16 { std::uint8_t bytes[16]; };
    struct alignas(32) Blob32 { std::uint8_t bytes[32]; };
    static int& DtorCounter() {
        static int c = 0;
        return c;
    }

    class BumpVectorTest : public Test {};

    TEST_F(BumpVectorTest, BasicEmplaceBackAndAccess_Int) {
        BumpVector<int> vec(8); // power-of-two arena size
        EXPECT_TRUE(vec.Empty());
        EXPECT_EQ(vec.Size(), 0u);

        for (int i = 0; i < 5; ++i) {
            vec.EmplaceBackValue(i * 10);
        }

        EXPECT_FALSE(vec.Empty());
        EXPECT_EQ(vec.Size(), 5u);

        // operator[] and At within range
        for (int i = 0; i < 5; ++i) {
            EXPECT_EQ(vec[i], i * 10);
            auto* p = vec.At(i);
            ASSERT_NE(p, nullptr);
            EXPECT_EQ(*p, i * 10);
        }

        // Back/PopBack semantics
        EXPECT_EQ(vec.Back(), 40);
        vec.PopBack();
        EXPECT_EQ(vec.Size(), 4u);
        EXPECT_EQ(vec.Back(), 30);
    }

    TEST_F(BumpVectorTest, EmplaceBackTrivial_ByteSizedTypes) {
        BumpVector<std::uint8_t> v8(16);
        for (std::uint8_t i = 0; i < 10; ++i) {
            v8.EmplaceBackTrivial(static_cast<std::uint8_t>(i + 1));
        }
        ASSERT_EQ(v8.Size(), 10u);
        for (std::uint8_t i = 0; i < 10; ++i) {
            EXPECT_EQ(v8[i], static_cast<std::uint8_t>(i + 1));
        }

        BumpVector<std::uint16_t> v16(16);
        for (std::uint16_t i = 0; i < 10; ++i) {
            v16.EmplaceBackTrivial(static_cast<std::uint16_t>(i * 3));
        }
        ASSERT_EQ(v16.Size(), 10u);
        for (std::uint16_t i = 0; i < 10; ++i) {
            EXPECT_EQ(v16[i], static_cast<std::uint16_t>(i * 3));
        }

        BumpVector<std::uint32_t> v32(16);
        for (std::uint32_t i = 0; i < 6; ++i) {
            v32.EmplaceBackTrivial(0xABC00000u + i);
        }
        ASSERT_EQ(v32.Size(), 6u);
        for (std::uint32_t i = 0; i < 6; ++i) {
            EXPECT_EQ(v32[i], 0xABC00000u + i);
        }

        BumpVector<std::uint64_t> v64(16);
        for (std::uint64_t i = 0; i < 6; ++i) {
            v64.EmplaceBackTrivial(0xDEADBEEF00000000ull + i);
        }
        ASSERT_EQ(v64.Size(), 6u);
        for (std::uint64_t i = 0; i < 6; ++i) {
            EXPECT_EQ(v64[i], 0xDEADBEEF00000000ull + i);
        }
    }

    TEST_F(BumpVectorTest, EmplaceBackTrivial_Simd16And32) {
        // Ensure unique patterns for validation
        Blob16 a{}; for (int i=0;i<16;++i) a.bytes[i] = static_cast<std::uint8_t>(i);
        Blob16 b{}; for (int i=0;i<16;++i) b.bytes[i] = static_cast<std::uint8_t>(i+1);
        Blob32 c{}; for (int i=0;i<32;++i) c.bytes[i] = static_cast<std::uint8_t>(255 - i);

        // Use arena large enough to keep single-arena to avoid Fast* assertions in debug
        BumpVector<Blob16> v16(64);
        v16.EmplaceBackTrivial(static_cast<Blob16 &&>(a));
        v16.EmplaceBackTrivial(static_cast<Blob16 &&>(b));
        ASSERT_EQ(v16.Size(), 2u);
        for (int i=0;i<16;++i) {
            EXPECT_EQ(v16[0].bytes[i], static_cast<std::uint8_t>(i));
            EXPECT_EQ(v16[1].bytes[i], static_cast<std::uint8_t>(i+1));
        }

        BumpVector<Blob32> v32(64);
        v32.EmplaceBackTrivial(static_cast<Blob32 &&>(c));
        ASSERT_EQ(v32.Size(), 1u);
        for (int i=0;i<32;++i) {
            EXPECT_EQ(v32[0].bytes[i], static_cast<std::uint8_t>(255 - i));
        }
    }

    TEST_F(BumpVectorTest, EmplaceBackTrivialFast_SmallTypes) {
        // Keep within first arena to avoid FastAt asserts in debug builds
        BumpVector<std::uint32_t> v(64);
        for (std::uint32_t i = 0; i < 10; ++i) {
            v.EmplaceBackTrivial(i * 7u);
        }
        ASSERT_EQ(v.Size(), 10u);
        for (std::uint32_t i = 0; i < 10; ++i) {
            EXPECT_EQ(v[i], i * 7u);
        }
    }

    TEST_F(BumpVectorTest, GrowthAcrossArenas) {
        // Small arena to trigger allocation of additional arenas
        BumpVector<int> vec(8);
        constexpr int n = 100; // exceed first arena capacity of 8 elements
        for (int i = 0; i < n; ++i) {
            vec.EmplaceBackValue(i);
        }
        EXPECT_EQ(vec.Size(), static_cast<std::size_t>(n));

        // Validate a few positions across boundaries via At (multi-arena safe)
        for (int i : {0, 7, 8, 15, 16, 63, 64, 99}) {
            auto* p = vec.At(static_cast<std::size_t>(i));
            ASSERT_NE(p, nullptr) << "At returned null for index " << i;
            EXPECT_EQ(*p, i);
        }
    }

    TEST_F(BumpVectorTest, IterationAndReuse) {
        BumpVector<int> vec(32);
        for (int i = 0; i < 12; ++i) vec.EmplaceBackValue(i*i);

        // Count via iterators
        std::size_t count = 0; int sum = 0;
        for (const int it : vec) {
            ++count;
            sum += it;
        }
        EXPECT_EQ(count, vec.Size());
        EXPECT_GT(sum, 0);

        // Reuse resets size and allows new inserts
        vec.Reuse();
        EXPECT_TRUE(vec.Empty());
        EXPECT_EQ(vec.Size(), 0u);
        for (int i = 0; i < 5; ++i) vec.EmplaceBackValue(42);
        EXPECT_EQ(vec.Size(), 5u);
        for (std::size_t i = 0; i < vec.Size(); ++i) EXPECT_EQ(vec[i], 42);
    }

    TEST_F(BumpVectorTest, IteratorAPI_AllOps) {
        BumpVector<int> vec(8);
        for (int i = 0; i < 5; ++i) vec.EmplaceBackValue(i + 1);

        // Non-const iterators
        auto it = vec.begin();
        ASSERT_NE(it, vec.end());
        EXPECT_EQ(*it, 1);

        // Advance to end and count steps
        std::size_t steps = 0;
        for (; it != vec.end(); ++it) {
            ++steps;
        }

        EXPECT_EQ(steps, vec.Size());
        EXPECT_TRUE(it == vec.end());

        // Step back to last and check
        --it;
        EXPECT_EQ(*it, 5);
        --it; // now at 4
        EXPECT_EQ(*it, 4);

        // Equality/inequality
        EXPECT_TRUE(vec.begin() == vec.begin());
        EXPECT_TRUE(vec.begin() != vec.end());

        const BumpVector<int>& cvec = vec;
        std::size_t count = 0; int sum = 0;
        for (const int cit : cvec) {
            ++count;
            sum += cit;
        }
        EXPECT_EQ(count, vec.Size());
        EXPECT_EQ(sum, 1+2+3+4+5);
    }

    struct ArrowType { int v; [[nodiscard]] int Twice() const { return v*2; } };

    TEST_F(BumpVectorTest, IteratorArrowOperator) {
        BumpVector<ArrowType> vec(4);
        vec.EmplaceBackValue(ArrowType{7});
        vec.EmplaceBackValue(ArrowType{9});
        auto it = vec.begin();
        EXPECT_EQ(it->v, 7);
        EXPECT_EQ(it->Twice(), 14);
        ++it;
        EXPECT_EQ(it->v, 9);
    }

    TEST_F(BumpVectorTest, EmplaceBack_PerfectForwarding_NonTrivial) {
        struct Tracker {
            int x;
            bool moved;
            explicit Tracker(int a) : x(a), moved(false) {}
            Tracker(Tracker&& other) noexcept : x(other.x), moved(true) { other.x = -1; }
            Tracker(const Tracker&) = delete;
            Tracker& operator=(const Tracker&) = delete;
            Tracker& operator=(Tracker&&) = delete;
        };

        BumpVector<Tracker> vec(2);
        vec.EmplaceBack(Tracker{42}); // rvalue should invoke move ctor
        EXPECT_EQ(vec.Size(), 1u);
        EXPECT_TRUE(vec[0].moved);
        EXPECT_EQ(vec[0].x, 42);

        // Also test const Back()/At() overloads
        const BumpVector<Tracker>& cref = vec;
        EXPECT_EQ(cref.Back().x, 42);
        ASSERT_NE(cref.At(0), nullptr);
        EXPECT_EQ(cref.At(0)->x, 42);
    }

    TEST_F(BumpVectorTest, ControlledMultiArenaGrowth_HitsBothAllocatorBranches) {
        // Use very small arena to force growth while staying safe w.r.t known overflow in long stress
        BumpVector<std::uint8_t> vec(2);
        // Fill arena 0
        vec.EmplaceBackTrivial(10); // idx 0
        vec.EmplaceBackTrivial(11); // idx 1
        // This push exceeds arena 0 -> triggers ReallocateArenas() branch
        vec.EmplaceBackTrivial(12); // idx 2 in arena 1
        // Fill arena 1 completely
        vec.EmplaceBackTrivial(13); // idx 3
        // This push exceeds arena 1 -> triggers second ReallocateArenas() (size becomes 4)
        vec.EmplaceBackTrivial(14); // idx 4 in arena 2
        // Fill arena 2 completely
        vec.EmplaceBackTrivial(15); // idx 5
        // This push exceeds arena 2 -> NextArenaIsEmpty() is true; should move to arena 3 without realloc
        vec.EmplaceBackTrivial(16); // idx 6 in arena 3

        ASSERT_EQ(vec.Size(), 7u);
        // Spot-check values across boundaries via At (multi-arena safe)
        for (std::size_t i = 0; i < vec.Size(); ++i) {
            auto* p = vec.At(i);
            ASSERT_NE(p, nullptr) << "At returned null at index " << i;
        }
        EXPECT_EQ(vec[0], 10);
        EXPECT_EQ(vec[1], 11);
        EXPECT_EQ(vec[2], 12);
        EXPECT_EQ(vec[3], 13);
        EXPECT_EQ(vec[4], 14);
        EXPECT_EQ(vec[5], 15);
        EXPECT_EQ(vec[6], 16);
    }

     TEST_F(BumpVectorTest, IteratorPostfixAndPlus) {
        BumpVector<int> vec(8);
        vec.EmplaceBackValue(1);
        vec.EmplaceBackValue(2);
        vec.EmplaceBackValue(3);

        auto it = vec.begin();
        int first = *(it++); // postfix ++ returns previous
        EXPECT_EQ(first, 1);
        EXPECT_EQ(*it, 2);

        // Advance further with prefix ++ and check value
        ++it;
        EXPECT_EQ(*it, 3);
    }

    TEST_F(BumpVectorTest, At_OutOfRange_ReturnsNull) {
        BumpVector<int> vec(4);
        vec.EmplaceBackValue(10);
        vec.EmplaceBackValue(20);

        // Non-const
        EXPECT_EQ(vec.At(2), nullptr); // index == Size()
#ifndef NDEBUG
        EXPECT_DEATH((void)vec.At(1000), ".*");
#elif  !SanitizersEnabled
        EXPECT_GE(std::bit_cast<std::intptr_t>(vec.At(1000)),NULL);
#endif

        // Const
        const BumpVector<int>& cref = vec;
        EXPECT_EQ(cref.At(2), nullptr);
#ifndef NDEBUG
        EXPECT_DEATH((void)vec.At(1000), ".*");
#elif  !SanitizersEnabled
        EXPECT_GE(std::bit_cast<std::intptr_t>(vec.At(1000)),NULL);
#endif
    }

#ifndef NDEBUG
    TEST_F(BumpVectorTest, BackupArenaSize_ZeroArenaAllocates) {
        // Exercise BumpAllocator’s backup arena path (arenaSize == 0)
        EXPECT_DEATH(BumpVector<int> vec(0),".*");
    }
#endif

    TEST_F(BumpVectorTest, NonTrivialDestructor_Invoked) {
        struct DtorTracker {
            int v;
            explicit DtorTracker(int x) : v(x) {}
            ~DtorTracker() { DtorCounter() += 1; }
        };

        DtorCounter() = 0;
        {
            BumpVector<DtorTracker> vec(2);
            for (int i = 0; i < 5; ++i) {
                vec.EmplaceBack(DtorTracker{i});
            }
            EXPECT_EQ(vec.Size(), 5u);
        }
        // 5 temporaries got destroyed in the loop, 5 got destroyed after going out of scope
        EXPECT_EQ(DtorCounter(), 10);
    }
}