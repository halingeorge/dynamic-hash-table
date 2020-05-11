#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include "hash_table.h"

namespace {

constexpr int32_t kMinNumber = 0;
constexpr int32_t kMaxNumber = (1 << 20) - 1;

constexpr int32_t kHashTableSize = 1;
constexpr int32_t kMaxAddedNumbers = kMaxNumber - kMinNumber + 1;

class FastKeyGenerator {
 public:
  FastKeyGenerator()
      : lookup_order_(kMaxAddedNumbers), insert_order_(kMaxAddedNumbers), remove_order_(kMaxAddedNumbers) {
    for (int32_t i = kMinNumber; i <= kMaxNumber; i++) {
      lookup_order_.push_back(i);
      insert_order_.push_back(i);
      remove_order_.push_back(i);
    }

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(lookup_order_.begin(), lookup_order_.end(), g);
    std::shuffle(insert_order_.begin(), insert_order_.end(), g);
    std::shuffle(remove_order_.begin(), remove_order_.end(), g);
  }

  int32_t GenerateLookupKey() {
    return GetNext(lookup_order_, lookup_number_);
  }

  int32_t GenerateInsertKey() {
    return GetNext(insert_order_, insert_number_);
  }

  int32_t GenerateRemoveKey() {
    return GetNext(remove_order_, remove_number_);
  }

 private:
  static int32_t GetNext(const std::vector<int32_t>& keys, uint64_t& index) {
    return keys[index++ & (keys.size() - 1)];
  }

 private:
  std::vector<int32_t> lookup_order_;
  std::vector<int32_t> insert_order_;
  std::vector<int32_t> remove_order_;

  uint64_t lookup_number_ = 0;
  uint64_t insert_number_ = 0;
  uint64_t remove_number_ = 0;
};

}  // namespace

template<typename HashTableImpl>
struct HashTableFixture : public benchmark::Fixture {
 public:
  HashTableFixture() : hash_table(kHashTableSize) {
  }

  void ManyLookups(benchmark::State& state, bool measure_lookup,
                   bool measure_insert, bool measure_remove);

  void Lookup(std::unordered_map<int32_t, int32_t>& hash_table, int32_t key, int32_t& /*value*/) {
    std::unique_lock lock(mutex);
    hash_table.find(key);
  }

  void Lookup(HashTable<int32_t, int32_t>& hash_table, int32_t key, int32_t& value) {
    hash_table.Lookup(key, value);
  }

  void Insert(std::unordered_map<int32_t, int32_t>& hash_table, int32_t key, int32_t value) {
    std::unique_lock lock(mutex);
    hash_table.emplace(key, value);
  }

  void Insert(HashTable<int32_t, int32_t>& hash_table, int32_t key, int32_t value) {
    hash_table.Insert(key, value);
  }

  void Remove(std::unordered_map<int32_t, int32_t>& hash_table, int32_t key) {
    std::unique_lock lock(mutex);
    hash_table.erase(key);
  }

  void Remove(HashTable<int32_t, int32_t>& hash_table, int32_t key) {
    hash_table.Remove(key);
  }

  HashTableImpl hash_table;
  std::mutex mutex;
};

template<typename HashTable>
void HashTableFixture<HashTable>::ManyLookups(benchmark::State& state, bool measure_lookup,
                                              bool measure_insert, bool measure_remove) {
  std::atomic<bool> stopped = false;

  auto func = [&](size_t thread_index) {
    FastKeyGenerator generator;
    if (thread_index < state.range(0)) {
      int32_t temp;
      while (!stopped) {
        auto key = generator.GenerateLookupKey();
        Lookup(hash_table, key, temp);
      }
    } else if (state.range(0) <= thread_index && thread_index < state.range(0) + state.range(1)) {
      while (!stopped) {
        auto key = generator.GenerateInsertKey();
        Insert(hash_table, key, /*value =*/ 0);
      }
    } else {
      while (!stopped) {
        auto key = generator.GenerateRemoveKey();
        Remove(hash_table, key);
      }
    }
  };

  std::vector<std::thread> threads;
  for (size_t i = 0; i < state.range(0) + state.range(1) + state.range(2); i++) {
    threads.emplace_back(func, i);
  }

  FastKeyGenerator generator;
  if (measure_lookup) {
    int32_t temp;
    for (auto _ : state) {
      auto key = generator.GenerateLookupKey();
      Lookup(hash_table, key, temp);
    }
  } else if (measure_insert) {
    for (auto _ : state) {
      auto key = generator.GenerateInsertKey();
      Insert(hash_table, key, /*value =*/ 0);
    }
  } else if (measure_remove) {
    for (auto _ : state) {
      auto key = generator.GenerateRemoveKey();
      Remove(hash_table, key);
    }
  }

  stopped = true;

  for (auto& t : threads) {
    t.join();
  }
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureLookupMyHashTable,
                            HashTable<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ true,
      /*measure_insert =*/ false,
      /*measure_remove =*/ false);
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureInsertMyHashTable,
                            HashTable<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ true,
      /*measure_remove =*/ false);
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureRemoveMyHashTable,
                            HashTable<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ false,
      /*measure_remove =*/ true);
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureLookupStdHashTable,
                            std::unordered_map<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ true,
      /*measure_insert =*/ false,
      /*measure_remove =*/ false);
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureInsertStdHashTable,
                            std::unordered_map<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ true,
      /*measure_remove =*/ false);
}

BENCHMARK_TEMPLATE_DEFINE_F(HashTableFixture,
                            MeasureRemoveStdHashTable,
                            std::unordered_map<int32_t, int32_t>)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ false,
      /*measure_remove =*/ true);
}

BENCHMARK_REGISTER_F(HashTableFixture, MeasureInsertStdHashTable)
->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureLookupStdHashTable)
->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureRemoveStdHashTable)
->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();

BENCHMARK_REGISTER_F(HashTableFixture, MeasureInsertMyHashTable)
    ->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureLookupMyHashTable)
    ->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureRemoveMyHashTable)
    ->Args({1, 1, 1})->Args({1, 2, 1})->Args({2, 2, 2})->Args({6, 2, 2})->UseRealTime();

