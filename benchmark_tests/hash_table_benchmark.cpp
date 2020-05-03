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

class Timer {
 public:
  explicit Timer(benchmark::State& state) : state_(state), start_clock_(std::chrono::high_resolution_clock::now()) {
  }

  void Flush() {
    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start_clock_);
    state_.SetIterationTime(elapsed.count());
    start_clock_ = end;
  }

 private:
  benchmark::State& state_;
  std::chrono::high_resolution_clock::time_point start_clock_;
};

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

struct HashTableFixture : public benchmark::Fixture {
 public:
  HashTableFixture() : hash_table(kHashTableSize) {
  }

  void ManyLookups(benchmark::State& state, bool measure_lookup,
                   bool measure_insert, bool measure_remove);

  HashTable<int32_t, int32_t> hash_table;
};

void HashTableFixture::ManyLookups(benchmark::State& state, bool measure_lookup,
                                   bool measure_insert, bool measure_remove) {
  static constexpr size_t kBatchSize = 1000;

  Timer timer(state);
  FastKeyGenerator generator;

  int32_t temp;

  while (state.KeepRunning()) {
    for (size_t i = 0; i < kBatchSize; i++) {
      if (state.thread_index == 0) {
        auto key = generator.GenerateLookupKey();
        hash_table.Lookup(key, temp);
      } else if (state.thread_index <= state.threads / 2) {
        auto key = generator.GenerateInsertKey();
        hash_table.Insert(key, /*value =*/ 0);
      } else {
        auto key = generator.GenerateRemoveKey();
        hash_table.Remove(key);
      }
    }

    if ((state.thread_index == 0 && measure_lookup) || (state.thread_index <= state.threads / 2 && measure_insert)
        || (state.thread_index == state.threads / 2 + 1 && measure_remove)) {
      timer.Flush();
    }
  }
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureLookup)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ true,
      /*measure_insert =*/ false,
      /*measure_remove =*/ false);
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureInsert)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ true,
      /*measure_remove =*/ false);
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureRemove)(benchmark::State& state) {
  ManyLookups(state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ false,
      /*measure_remove =*/ true);
}

BENCHMARK_REGISTER_F(HashTableFixture, MeasureInsert)
    ->Threads(3)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureLookup)
    ->Threads(3)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureRemove)
    ->Threads(3)
    ->UseManualTime();