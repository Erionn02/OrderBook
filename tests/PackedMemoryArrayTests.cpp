#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <unordered_map>
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
    for (int x: {500, 100, 900, 300, 700, 200, 800, 400, 600, 0}) {
        s.insert(x);
        s.print(x);
    }
    EXPECT_EQ(s.size(), 10u);
    EXPECT_EQ(s.toVector(), (std::vector<int>{0, 100, 200, 300, 400, 500, 600, 700, 800, 900}));
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
    const std::size_t max_steps{20000};
    for (std::size_t step = 0; step < max_steps; ++step) {
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
        if (step % 200 == 0 || step == max_steps - 1) {
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
        for (std::size_t step = 0; step < 8000; ++step) {
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

    const std::size_t max_steps{60000};

    for (std::size_t step = 0; step < max_steps; ++step) {
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
        if (step % 200 == 0 || step == max_steps - 1) {
            std::vector<int> got{ds.begin(), ds.end()};
            ASSERT_EQ(got, std::vector<int>(oracle.begin(), oracle.end()));
        }
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

namespace {
    std::unordered_map<int, std::size_t>* g_pos = nullptr;

    struct RecordPosHook {
        void operator()(int value, std::size_t idx) const {
            if (g_pos) {
                (*g_pos)[value] = idx;
            }
        }
    };
} // namespace

TEST(PMATests, hookTracksPositionsThroughRebalance) {
    std::unordered_map<int, std::size_t> pos;
    g_pos = &pos;
    packed_memory_array<int, int, std::less<>, RecordPosHook> ds;

    std::mt19937 rng(2024);
    std::uniform_int_distribution<int> keyDist(0, 5000);
    for (int step = 0; step < 40000; ++step) {
        int k = keyDist(rng);
        if (step % 3 != 0) {
            ds.insert(k, k);
        } else {
            auto it = ds.find(k);
            if (it != ds.end()) ds.erase(it);
        }
    }

    std::size_t live = 0;
    for (std::size_t idx = 0; idx < ds.capacity_; ++idx) {
        if (ds.test_bit(idx)) {
            ++live;
            int v = ds.values[idx];
            ASSERT_EQ(ds.keys[idx], v);
            auto found = pos.find(v);
            ASSERT_NE(found, pos.end());
            ASSERT_EQ(found->second, idx);
        }
    }
    ASSERT_EQ(live, ds.size());
    g_pos = nullptr;
}

TEST(PMATests, insertReturnsLiveIteratorAndKeepsKeysMonotonic) {
    packed_memory_array<int, int> ds;
    std::vector<int> order(6000);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), std::mt19937(2137));

    for (int k: order) {
        auto [it, inserted] = ds.insert(k, k * 10);
        ASSERT_TRUE(inserted);
        ASSERT_EQ(*it, k * 10);
    }

    for (std::size_t idx = 1; idx < ds.capacity(); ++idx) {
        ASSERT_LE(ds.keys[idx - 1], ds.keys[idx]);
    }

    int expected = 0;
    for (int v: ds) {
        ASSERT_EQ(v, expected * 10);
        ++expected;
    }
    ASSERT_EQ(expected, 6000);
}
