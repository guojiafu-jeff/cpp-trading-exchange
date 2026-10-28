#include "common/mem_pool.h"

#include <cstddef>

#include "gtest/gtest.h"

namespace {

struct Widget {
    int a;
    double b;
    Widget(int a_, double b_) : a(a_), b(b_) {}
};

TEST(MemPool, AllocateConstructsWithArguments) {
    Common::MemPool<Widget> pool(4);
    Widget* w = pool.allocate(7, 2.5);
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->a, 7);
    EXPECT_DOUBLE_EQ(w->b, 2.5);
    EXPECT_EQ(pool.capacity(), 4u);
    EXPECT_EQ(pool.numFree(), 3u);
    pool.deallocate(w);
    EXPECT_EQ(pool.numFree(), 4u);
}

TEST(MemPool, SlotsAreContiguous) {
    Common::MemPool<Widget> pool(3);
    const auto* x = reinterpret_cast<const std::byte*>(pool.allocate(1, 1.0));
    const auto* y = reinterpret_cast<const std::byte*>(pool.allocate(2, 2.0));
    const auto* z = reinterpret_cast<const std::byte*>(pool.allocate(3, 3.0));
    EXPECT_EQ(static_cast<std::size_t>(y - x), sizeof(Widget));
    EXPECT_EQ(static_cast<std::size_t>(z - y), sizeof(Widget));
}

TEST(MemPool, ExhaustedPoolReturnsNullptr) {
    Common::MemPool<Widget> pool(2);
    EXPECT_NE(pool.allocate(1, 1.0), nullptr);
    EXPECT_NE(pool.allocate(2, 2.0), nullptr);
    EXPECT_EQ(pool.allocate(3, 3.0), nullptr);
    EXPECT_EQ(pool.numFree(), 0u);
}

TEST(MemPool, DeallocatedSlotIsReused) {
    Common::MemPool<Widget> pool(1);
    Widget* first = pool.allocate(1, 1.0);
    ASSERT_NE(first, nullptr);
    pool.deallocate(first);
    Widget* second = pool.allocate(2, 2.0);
    EXPECT_EQ(second, first);  // same address handed out again
    EXPECT_EQ(second->a, 2);
}

TEST(MemPool, FreeSlotInTheMiddleIsFound) {
    Common::MemPool<Widget> pool(3);
    pool.allocate(1, 1.0);
    Widget* middle = pool.allocate(2, 2.0);
    pool.allocate(3, 3.0);
    pool.deallocate(middle);                    // a hole in the middle
    Widget* again = pool.allocate(4, 4.0);      // the scan must wrap around and find it
    EXPECT_EQ(again, middle);
    EXPECT_EQ(pool.allocate(5, 5.0), nullptr);  // and now the pool is really full
}

TEST(MemPool, DestructorsRunOnDeallocateAndOnPoolDestruction) {
    static int destroyed = 0;
    struct Probe {
        ~Probe() { ++destroyed; }
    };
    {
        Common::MemPool<Probe> pool(2);
        Probe* p = pool.allocate();
        pool.allocate();  // deliberately left alive
        pool.deallocate(p);
        EXPECT_EQ(destroyed, 1);
    }  // pool goes out of scope here: the leftover object must be destroyed too
    EXPECT_EQ(destroyed, 2);
}

}  // namespace