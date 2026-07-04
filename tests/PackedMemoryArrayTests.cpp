#include <gtest/gtest.h>

#include <random>
#include <set>
#include <vector>
#include <print>

#define private public
#include "packed_memory_array.hpp"

using namespace ::testing;

namespace {
    struct IntSet {
        packed_memory_array<int, int> ds;
        bool insert(int x) { return ds.insert(x, x).second; }

        bool erase(int x) {
            auto it = ds.find(x);
            if (it == ds.end()) return false;
            ds.erase(it);
            return true;
        }

        bool contains(int x) { return ds.find(x) != ds.end(); }

        std::size_t size() const { return ds.size(); }

        std::vector<int> toVector() const {
            return {ds.begin(), ds.end()};
        }

        void print(int x) const {
            std::vector<int> keys_vec(ds.keys, ds.keys + ds.capacity_);
            std::println("after insert [{}], live: {} \n keys: {} \n", x, toVector(), keys_vec);
        }
    };
} // namespace

TEST(PMATests, getNextLive) {
    packed_memory_array<int, int> ds{};
    ASSERT_EQ(ds.get_next_live(0), -1);
    ASSERT_EQ(ds.get_next_live(1000), -1);
    ds.set_occupied(15);
    ds.set_occupied(17);
    ASSERT_EQ(ds.get_next_live(15), 17);
    ASSERT_EQ(ds.get_next_live(50), -1);
    ASSERT_EQ(ds.get_next_live(17), -1);
    ASSERT_EQ(ds.get_next_live(14), 15);
    ds.set_occupied(0);
    ASSERT_EQ(ds.get_next_live(0), 15);
}

TEST(PMATests, getNextGap) {
    packed_memory_array<int, int> ds{};
    ASSERT_EQ(ds.get_next_gap(0), 1);
    ASSERT_EQ(ds.get_next_gap(1000), -1);
    ds.set_occupied(15);
    ds.set_occupied(20);
    ASSERT_EQ(ds.get_next_gap(14), 16);
    ASSERT_EQ(ds.get_next_gap(19), 21);
    ASSERT_EQ(ds.get_next_gap(22), 23);
    ASSERT_EQ(ds.get_next_gap(127), -1);
}


TEST(PMATests, getPrevLive) {
    packed_memory_array<int, int> ds{};
    ds.set_occupied(15);
    ds.set_occupied(17);
    ASSERT_EQ(ds.get_prev_live(17), 15);
    ASSERT_EQ(ds.get_prev_live(50), 17);
    ASSERT_EQ(ds.get_prev_live(15), -1);
    ASSERT_EQ(ds.get_prev_live(10000), 17);
    ASSERT_EQ(ds.get_prev_live(0), -1);
    ds.set_occupied(0);
    ASSERT_EQ(ds.get_prev_live(0), -1);
}

TEST(PMATests, getPrevGap) {
    packed_memory_array<int, int> ds{};
    ds.set_occupied(15);
    ds.set_occupied(20);
    ASSERT_EQ(ds.get_prev_gap(16), 14);
    ASSERT_EQ(ds.get_prev_gap(14), 13);
    ASSERT_EQ(ds.get_prev_gap(21), 19);
    ASSERT_EQ(ds.get_prev_gap(0), -1);
    ds.set_occupied(0);
}


TEST(PMATests, emptyContainer) {
    IntSet s;
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.ds.empty());
    EXPECT_EQ(s.ds.begin(), s.ds.end());
    EXPECT_FALSE(s.contains(42));
}

TEST(PMATests, insertKeepsSortedOrder) {
    IntSet s;
    for (int x: {5, 1, 9, 3, 7, 2, 8, 4, 6, 0}) {
        s.insert(x);
        s.print(x);
    }
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(s.toVector(), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(PMATests, duplicateInsertRejected) {
    packed_memory_array<int, int> ds;
    EXPECT_TRUE(ds.insert(5, 5).second);
    auto [it, inserted] = ds.insert(5, 5);
    EXPECT_FALSE(inserted);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(ds.size(), 1u);
}

TEST(PMATests, findAndLowerBound) {
    packed_memory_array<int, int> ds;
    for (int x: {10, 20, 30, 40, 50}) ds.insert(x, x);
    EXPECT_EQ(*ds.find(30), 30);
    EXPECT_EQ(ds.find(35), ds.end());
    EXPECT_EQ(*ds.lower_bound(30), 30);
    EXPECT_EQ(*ds.lower_bound(31), 40);
    EXPECT_EQ(*ds.lower_bound(0), 10);
    EXPECT_EQ(ds.lower_bound(51), ds.end());
}

TEST(PMATests, eraseIsO1AndIterationSkipsGaps) {
    IntSet s;
    for (int i = 0; i < 20; ++i) s.insert(i);
    for (int i = 0; i < 20; i += 2)
        ASSERT_TRUE(s.erase(i));
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(s.toVector(), (std::vector<int>{1, 3, 5, 7, 9, 11, 13, 15, 17, 19}));
}

TEST(PMATests, eraseReturnsNextLive) {
    packed_memory_array<int, int> ds;
    for (int x: {1, 2, 3, 4, 5}) ds.insert(x, x);
    auto next = ds.erase(ds.find(3));
    ASSERT_NE(next, ds.end());
    EXPECT_EQ(*next, 4);
}

TEST(PMATests, growsThroughManyInserts) {
    IntSet s;
    constexpr int N = 50000;
    for (int i = N - 1; i >= 0; --i) s.insert(i);
    EXPECT_EQ(s.size(), static_cast<std::size_t>(N));
    int expected = 0;
    for (int v: s.ds)
        EXPECT_EQ(v, expected++);
    EXPECT_EQ(expected, N);
}

TEST(PMATests, reverseAndMiddleInsertStress) {
    IntSet s;
    for (int i = 0; i < 2000; i += 2) s.insert(i);
    for (int i = 1; i < 2000; i += 2) s.insert(i);
    ASSERT_EQ(s.size(), 2000u);
    int expected = 0;
    for (int v: s.ds)
        ASSERT_EQ(v, expected++);
}

TEST(PMATests, randomizedOracleVsStdSet) {
    IntSet s;
    std::set<int> oracle;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> keyDist(0, 400);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int step = 0; step < 20000; ++step) {
        int op = opDist(rng);
        int key = keyDist(rng);
        if (op == 0) {
            ASSERT_EQ(s.insert(key), oracle.insert(key).second);
        } else if (op == 1) {
            bool present = s.contains(key);
            ASSERT_EQ(present, oracle.count(key) == 1);
            if (present) {
                s.erase(key);
                oracle.erase(key);
            }
        } else {
            ASSERT_EQ(s.contains(key), oracle.count(key) == 1);
        }
        ASSERT_EQ(s.size(), oracle.size());
        if (step % 200 == 0 || step == 19999) {
            ASSERT_EQ(s.toVector(), std::vector<int>(oracle.begin(), oracle.end()));
        }
    }
}

namespace {
    int g_live = 0;

    struct Counted {
        int v;
        Counted(int x) : v(x) { ++g_live; }
        Counted(const Counted &o) : v(o.v) { ++g_live; }
        Counted(Counted &&o) noexcept : v(o.v) { ++g_live; }

        Counted &operator=(const Counted &) = default;

        Counted &operator=(Counted &&) = default;

        ~Counted() { --g_live; }
    };
} // namespace

TEST(PMATests, constructDestroyBalance) {
    g_live = 0; {
        packed_memory_array<int, Counted> ds;
        std::mt19937 rng(99);
        std::uniform_int_distribution<int> keyDist(0, 300);
        std::set<int> oracle;
        for (int step = 0; step < 8000; ++step) {
            int key = keyDist(rng);
            if (step % 2 == 0) {
                ds.insert(key, Counted{key});
                oracle.insert(key);
            } else {
                auto it = ds.find(key);
                if (it != ds.end()) {
                    ds.erase(it);
                    oracle.erase(key);
                }
            }
            ASSERT_EQ(ds.size(), oracle.size());
            ASSERT_EQ(g_live, static_cast<int>(ds.size()));
        }
    }
    EXPECT_EQ(g_live, 0);
}

TEST(PMATests, randomizedOracleDescendingWideRange) {
    packed_memory_array<int, int, std::greater<int> > ds;
    std::set<int, std::greater<int> > oracle;
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> keyDist(-5000, 5000);
    std::uniform_int_distribution<int> opDist(0, 3);

    for (int step = 0; step < 60000; ++step) {
        int op = opDist(rng);
        int key = keyDist(rng);
        if (op <= 1) {
            bool a = ds.insert(key, key).second;
            bool b = oracle.insert(key).second;
            ASSERT_EQ(a, b);
        } else if (op == 2) {
            auto it = ds.find(key);
            bool present = it != ds.end();
            ASSERT_EQ(present, oracle.count(key) == 1);
            if (present) {
                ASSERT_EQ(*it, key);
                ds.erase(it);
                oracle.erase(key);
            }
        } else {
            auto dsit = ds.lower_bound(key);
            auto orit = oracle.lower_bound(key);
            if (orit == oracle.end())
                ASSERT_EQ(dsit, ds.end());
            else {
                ASSERT_NE(dsit, ds.end());
                ASSERT_EQ(*dsit, *orit);
            }
        }
        ASSERT_EQ(ds.size(), oracle.size());
        std::vector<int> got{ds.begin(), ds.end()};
        ASSERT_EQ(got, std::vector<int>(oracle.begin(), oracle.end()));
    }
}

TEST(PMATests, valueLookupByKey) {
    packed_memory_array<int, Counted> ds;
    g_live = 0;
    for (int x: {100, 50, 150, 25, 75}) ds.insert(x, Counted{x});
    EXPECT_EQ(ds.find(75)->v, 75);
    EXPECT_EQ(ds.find(99), ds.end());
    EXPECT_EQ(ds.lower_bound(60)->v, 75);
}
