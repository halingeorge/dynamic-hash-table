#include "hash_table.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>

void StressTest(size_t buckets, size_t thread_number, size_t iterations) {
    FixedSizeHashTable<size_t, size_t> hash_table(buckets);

    auto get_key = [buckets](size_t thread_index, size_t bucket) {
        return thread_index * buckets + bucket;
    };

    auto test_routine = [&](size_t thread_index) {
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

TEST(HashTable, StressTestMin) {
    StressTest(10, 10, 1000);
}

TEST(HashTable, StressTestMax) {
    StressTest(15, 17, 1000);
}
