#include <gtest/gtest.h>

#include <random>
#include <set>
#include <vector>
#include <print>

#define private public
#include "PMA.hpp"

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
            std::vector<int> v;
            int i{0};
            for (auto it = ds.begin(); it != ds.end(); ++it) {
                std::println("{}", ++i);
                v.push_back(*it);
            }
            return v;
        }
    };
} // namespace

TEST(PMATests, get_next_live_test) {
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

TEST(PMATests, get_next_gap_test) {
    packed_memory_array<int, int> ds{};
    ASSERT_EQ(ds.get_next_gap(0), 1);
    ASSERT_EQ(ds.get_next_gap(1000), -1);
    ds.set_occupied(15);
    ds.set_occupied(20);
    ASSERT_EQ(ds.get_next_gap(14), 16);
    ASSERT_EQ(ds.get_next_gap(19), 21);
    ASSERT_EQ(ds.get_next_gap(22), 23);
    ASSERT_EQ(ds.get_next_gap(63), -1);
}


TEST(PMATests, get_prev_live_test) {
    packed_memory_array<int, int> ds{};
    ds.set_occupied(15);
    ds.set_occupied(17);
    ASSERT_EQ(ds.get_prev_live(17), 15);
    ASSERT_EQ(ds.get_prev_live(50), 17);
    ASSERT_EQ(ds.get_prev_live(15), -1);
    ASSERT_EQ(ds.get_prev_live(10000), 17);
}

TEST(PMATests, get_prev_gap_test) {
    packed_memory_array<int, int> ds{};
    ds.set_occupied(15);
    ds.set_occupied(20);
    ASSERT_EQ(ds.get_prev_gap(16), 14);
    ASSERT_EQ(ds.get_prev_gap(14), 13);
    ASSERT_EQ(ds.get_prev_gap(21), 19);
    ASSERT_EQ(ds.get_prev_gap(0), -1);
    ds.set_occupied(0);
}


TEST(PMATests, EmptyContainer) {
    IntSet s;
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.ds.empty());
    EXPECT_EQ(s.ds.begin(), s.ds.end());
    EXPECT_FALSE(s.contains(42));
}

TEST(PMATests, InsertKeepsSortedOrder) {
    IntSet s;
    for (int x: {5, 1, 9, 3, 7, 2, 8, 4, 6, 0}) s.insert(x);
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(s.toVector(), (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(PMATests, DuplicateInsertRejected) {
    packed_memory_array<int, int> ds;
    EXPECT_TRUE(ds.insert(5, 5).second);
    auto [it, inserted] = ds.insert(5, 5);
    EXPECT_FALSE(inserted);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(ds.size(), 1u);
}

TEST(PMATests, FindAndLowerBound) {
    packed_memory_array<int, int> ds;
    for (int x: {10, 20, 30, 40, 50}) ds.insert(x, x);
    EXPECT_EQ(*ds.find(30), 30);
    EXPECT_EQ(ds.find(35), ds.end());
    EXPECT_EQ(*ds.lower_bound(30), 30);
    EXPECT_EQ(*ds.lower_bound(31), 40);
    EXPECT_EQ(*ds.lower_bound(0), 10);
    EXPECT_EQ(ds.lower_bound(51), ds.end());
}

TEST(PMATests, EraseIsO1AndIterationSkipsGaps) {
    IntSet s;
    for (int i = 0; i < 20; ++i) s.insert(i);
    for (int i = 0; i < 20; i += 2)
        ASSERT_TRUE(s.erase(i));
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(s.toVector(), (std::vector<int>{1, 3, 5, 7, 9, 11, 13, 15, 17, 19}));
}

TEST(PMATests, EraseReturnsNextLive) {
    packed_memory_array<int, int> ds;
    for (int x: {1, 2, 3, 4, 5}) ds.insert(x, x);
    auto next = ds.erase(ds.find(3));
    ASSERT_NE(next, ds.end());
    EXPECT_EQ(*next, 4);
}

TEST(PMATests, GrowsThroughManyInserts) {
    IntSet s;
    constexpr int N = 50000;
    for (int i = N - 1; i >= 0; --i) s.insert(i);
    EXPECT_EQ(s.size(), static_cast<std::size_t>(N));
    int expected = 0;
    for (int v: s.ds)
        EXPECT_EQ(v, expected++);
    EXPECT_EQ(expected, N);
}

TEST(PMATests, ReverseAndMiddleInsertStress) {
    IntSet s;
    for (int i = 0; i < 2000; i += 2) s.insert(i);
    for (int i = 1; i < 2000; i += 2) s.insert(i);
    ASSERT_EQ(s.size(), 2000u);
    int expected = 0;
    for (int v: s.ds)
        ASSERT_EQ(v, expected++);
}

TEST(PMATests, RandomizedOracleVsStdSet) {
    IntSet s;
    std::set<int> oracle;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> keyDist(0, 400);
    std::uniform_int_distribution<int> opDist(0, 2);

    for (int step = 0; step < 20000; ++step) {
        int op = opDist(rng);
        int key = keyDist(rng);
        if (op == 0) {
            ASSERT_EQ(s.insert(key), oracle.insert(key).second) << "step " << step << " key " << key;
        } else if (op == 1) {
            bool present = s.contains(key);
            ASSERT_EQ(present, oracle.count(key) == 1) << "step " << step;
            if (present) {
                s.erase(key);
                oracle.erase(key);
            }
        } else {
            ASSERT_EQ(s.contains(key), oracle.count(key) == 1);
        }
        ASSERT_EQ(s.size(), oracle.size()) << "step " << step;
        if (step % 200 == 0 || step == 19999) {
            ASSERT_EQ(s.toVector(), std::vector<int>(oracle.begin(), oracle.end())) << "step " << step;
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

TEST(PMATests, ConstructDestroyBalance) {
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
            ASSERT_EQ(g_live, static_cast<int>(ds.size())) << "step " << step; // no temporaries linger
        }
    }
    EXPECT_EQ(g_live, 0);
}

TEST(PMATests, RandomizedOracleDescendingWideRange) {
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
            ASSERT_EQ(a, b) << "step " << step << " key " << key;
        } else if (op == 2) {
            auto it = ds.find(key);
            bool present = it != ds.end();
            ASSERT_EQ(present, oracle.count(key) == 1) << "step " << step << " key " << key;
            if (present) {
                ASSERT_EQ(*it, key);
                ds.erase(it);
                oracle.erase(key);
            }
        } else {
            // lower_bound agreement
            auto dsit = ds.lower_bound(key);
            auto orit = oracle.lower_bound(key);
            if (orit == oracle.end())
                ASSERT_EQ(dsit, ds.end()) << "step " << step << " key " << key;
            else {
                ASSERT_NE(dsit, ds.end());
                ASSERT_EQ(*dsit, *orit) << "step " << step << " key " << key;
            }
        }
        ASSERT_EQ(ds.size(), oracle.size()) << "step " << step;
        std::vector<int> got;
        for (int v: ds) got.push_back(v);
        ASSERT_EQ(got, std::vector<int>(oracle.begin(), oracle.end())) << "step " << step;
    }
}

TEST(PMATests, ValueLookupByKey) {
    packed_memory_array<int, Counted> ds;
    g_live = 0;
    for (int x: {100, 50, 150, 25, 75}) ds.insert(x, Counted{x});
    EXPECT_EQ(ds.find(75)->v, 75);
    EXPECT_EQ(ds.find(99), ds.end());
    EXPECT_EQ(ds.lower_bound(60)->v, 75);
}
