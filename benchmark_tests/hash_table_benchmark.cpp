#include <benchmark/benchmark.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include "hash_table.h"

namespace {

constexpr int32_t kMinNumber = 1;
constexpr int32_t kMaxNumber = 100000;

constexpr int32_t kHashTableSize = 1;
constexpr int32_t kMaxAddedNumbers = kMaxNumber - kMinNumber + 1;

}  // namespace

struct HashTableFixture : public benchmark::Fixture {
 public:
  HashTableFixture() : added_keys_(kMaxAddedNumbers) {
  }

  void SetUp(const benchmark::State& state) override {
    put = 0;
    get = 0;
    count = 0;
    hash_table = std::make_unique<HashTable<int32_t, int32_t>>(kHashTableSize);
  }

  void TearDown(const benchmark::State& state) override {}

  void ManyLookups(benchmark::State& state, bool measure_lookup,
                   bool measure_insert, bool measure_remove);

  void PutElement(int32_t key) {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(count < kMaxAddedNumbers);
    added_keys_[put++ % added_keys_.size()] = key;
    ++count;
  }

  std::optional<int32_t> GetElement() {
    std::unique_lock lock(mutex);
    if (count > 0) {
      --count;
      return added_keys_[get++ % added_keys_.size()];
    }
    return std::nullopt;
  }

  uint64_t put;
  uint64_t get;
  uint64_t count;
  std::mutex mutex;

  std::unique_ptr<HashTable<int32_t, int32_t>> hash_table;

 private:
  std::vector<int32_t> added_keys_;
};

void HashTableFixture::ManyLookups(benchmark::State& state, bool measure_lookup,
                                   bool measure_insert, bool measure_remove) {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int32_t> distribution(kMinNumber, kMaxNumber);

  auto set_iteration_time = [&state](auto start) {
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    state.SetIterationTime(elapsed_seconds.count());
  };

  while (state.KeepRunning()) {
    int32_t temp;
    if (state.thread_index == 0) {
      auto key = distribution(mt);
      auto start = std::chrono::high_resolution_clock::now();
      hash_table->Lookup(key, temp);
      if (measure_lookup) {
        set_iteration_time(start);
      }
    } else if (state.thread_index <= state.threads / 2) {
      auto added_key = distribution(mt);
      auto start = std::chrono::high_resolution_clock::now();
      bool added = hash_table->Insert(added_key, distribution(mt));
      if (measure_insert) {
        set_iteration_time(start);
      }
      if (added) {
        PutElement(added_key);
      }
    } else {
      // Make sure to change this statement if more than 1 removers.
      auto key = GetElement();
      if (!key.has_value()) {
        auto start = std::chrono::high_resolution_clock::now();
        bool result = hash_table->Remove(kMaxNumber + 1);
        if (measure_remove) {
          set_iteration_time(start);
        }
        ASSERT_FALSE(result);
        continue;
      }
      auto start = std::chrono::high_resolution_clock::now();
      bool result = hash_table->Remove(*key);
      if (measure_remove) {
        set_iteration_time(start);
      }
      ASSERT_TRUE(result);
    }
  }
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureLookup)(benchmark::State& state) {
  ManyLookups(state,
              /*measure_lookup =*/true,
              /*measure_insert =*/false,
              /*measure_remove =*/false);
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureInsert)(benchmark::State& state) {
  ManyLookups(state,
              /*measure_lookup =*/false,
              /*measure_insert =*/true,
              /*measure_remove =*/false);
}

BENCHMARK_DEFINE_F(HashTableFixture, MeasureRemove)(benchmark::State& state) {
  ManyLookups(state,
              /*measure_lookup =*/false,
              /*measure_insert =*/false,
              /*measure_remove =*/true);
}

BENCHMARK_REGISTER_F(HashTableFixture, MeasureLookup)
    ->Threads(4)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureInsert)
    ->Threads(4)
    ->UseManualTime();
BENCHMARK_REGISTER_F(HashTableFixture, MeasureRemove)
    ->Threads(4)
    ->UseManualTime();