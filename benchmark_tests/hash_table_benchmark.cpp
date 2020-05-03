#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include "hash_table.h"

namespace {

constexpr int32_t kMinNumber = 1;
constexpr int32_t kMaxNumber = 1000000;

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

}  // namespace

struct HashTableFixture : public benchmark::Fixture {
 public:
  HashTableFixture()
      : added_keys_(kMaxAddedNumbers),
        hash_table(kHashTableSize) {}

  void ManyLookups(benchmark::State& state, bool measure_lookup,
                   bool measure_insert, bool measure_remove);

  HashTable<int32_t, int32_t> hash_table;

 private:
  std::vector<int32_t> added_keys_;
};

void HashTableFixture::ManyLookups(benchmark::State& state, bool measure_lookup,
                                   bool measure_insert, bool measure_remove) {
  static constexpr size_t kBatchSize = 1000;

  Timer timer(state);

  int current_key = 0;
  int32_t temp;

  while (state.KeepRunning()) {
    for (size_t i = 0; i < kBatchSize; i++) {
      auto key = kMinNumber + current_key++ % (kMaxNumber - kMinNumber);
      if (state.thread_index == 0) {
        hash_table.Lookup(key, temp);
      } else if (state.thread_index <= state.threads / 2) {
        hash_table.Insert(key, /*value =*/ 0);
      } else {
        hash_table.Remove(key);
      }
    }

    if (state.thread_index == 0 && measure_lookup || state.thread_index <= state.threads / 2 && measure_insert
        || state.thread_index == state.threads / 2 + 1 && measure_remove) {
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
    ->Threads(4)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureLookup)
    ->Threads(4)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureRemove)
    ->Threads(4)
    ->UseManualTime();