#include <gtest/gtest.h>

#include <random>
#include <thread>
#include <vector>

#include "hash_table.h"

class StressTest
    : public testing::TestWithParam<std::tuple<size_t, size_t, size_t>> {};

TEST_P(StressTest, BasicStressTest) {
  const auto buckets = std::get<0>(GetParam());
  const auto thread_number = std::get<1>(GetParam());
  const auto iterations = std::get<2>(GetParam());

  HashTable<size_t, size_t> hash_table(buckets);

  auto get_key = [buckets](size_t thread_index, size_t bucket) {
    return thread_index * buckets + bucket;
  };

  auto test_routine = [&](size_t thread_index) {
    std::vector<size_t> added;
    for (size_t i = 0; i < iterations; ++i) {
      for (size_t k = 0; k < buckets; k += 2) {
        auto key = get_key(thread_index, k);
        ASSERT_TRUE(hash_table.Insert(key, i));

        size_t value;
        auto lookup_key = get_key(i % thread_number, i % buckets);
        hash_table.Lookup(lookup_key, value);
      }

      for (size_t k = 0; k < buckets; ++k) {
        auto key = get_key(thread_index, k);
        size_t value;

        if (k % 2 == 0) {
          ASSERT_TRUE(hash_table.Lookup(key, value));
          ASSERT_EQ(value, i);
          ASSERT_TRUE(hash_table.Remove(key));

          if (i % 7 == 0) {
            ASSERT_FALSE(hash_table.Remove(key));
          }
        } else {
          ASSERT_FALSE(hash_table.Lookup(key, value));
          ASSERT_TRUE(hash_table.Insert(key, k));

          if (i % 13 == 0) {
            ASSERT_FALSE(hash_table.Insert(key, -1));
          }
        }
      }

      for (size_t k = 1; k < buckets; k += 2) {
        ASSERT_TRUE(hash_table.Remove(get_key(thread_index, k)));
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(thread_number);
  for (size_t t = 0; t < thread_number; ++t) {
    threads.emplace_back(test_routine, t);
  }
  for (size_t t = 0; t < thread_number; ++t) {
    threads[t].join();
  }
}

TEST_P(StressTest, StressTestWithMoreResizes) {
  const auto buckets = std::get<0>(GetParam());
  const auto thread_number = std::get<1>(GetParam());
  const auto iterations = std::get<2>(GetParam());

  HashTable<size_t, size_t> hash_table(buckets);

  std::atomic<size_t> counter;

  auto test_routine = [&](size_t thread_index) {
    std::random_device rd;
    std::mt19937 mt(rd());
    std::vector<size_t> added;
    for (size_t i = 0; i < iterations; ++i) {
      int key = counter.fetch_add(1);
      ASSERT_TRUE(hash_table.Insert(key, i));
      added.push_back(key);
      size_t value;
      ASSERT_TRUE(hash_table.Lookup(key, value));
      ASSERT_EQ(value, i);
      if (i % 7 == 0) {
        std::uniform_int_distribution<int32_t> distribution(0,
                                                            added.size() - 1);
        size_t index = distribution(mt);
        ASSERT_TRUE(hash_table.Remove(added[index]));
        ASSERT_FALSE(hash_table.Lookup(added[index], value));
        std::swap(added[index], added.back());
        added.pop_back();
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(thread_number);
  for (size_t t = 0; t < thread_number; ++t) {
    threads.emplace_back(test_routine, t);
  }
  for (size_t t = 0; t < thread_number; ++t) {
    threads[t].join();
  }
}

INSTANTIATE_TEST_SUITE_P(
    StressTestSuite, StressTest,
    testing::Values(std::tuple(10, 10, 1000), std::tuple(15, 17, 1000)),
    [](const testing::TestParamInfo<StressTest::ParamType>& info) {
      const auto buckets = std::get<0>(info.param);
      const auto thread_number = std::get<1>(info.param);
      const auto iterations = std::get<2>(info.param);

      return std::to_string(buckets) + "_buckets_" +
             std::to_string(thread_number) + "_threads_" +
             std::to_string(iterations) + "_iterations";
    });
