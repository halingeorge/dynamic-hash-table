#include "hash_table.h"
#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <iostream>
#include <random>
#include <chrono>
#include <atomic>

namespace {

const int32_t kMinNumber = 1;
const int32_t kMaxNumber = 100;

const int32_t kHashTableSize = 1;

const int32_t kMaxAddedNumbers = 1000 * 1000 * 100;

std::vector<std::atomic<int32_t>> added_keys(kMaxAddedNumbers);
std::atomic<size_t> put{0};
std::atomic<size_t> get{0};

HashTable<int32_t, int32_t> hash_table(kHashTableSize);

}  // namespace

void ManyLookups(benchmark::State& state, bool measure_lookup, bool measure_insert, bool measure_remove) {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int32_t> distribution(kMinNumber, kMaxNumber);

  auto set_iteration_time = [&state](auto start) {
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    state.SetIterationTime(elapsed_seconds.count());
  };

  while (state.KeepRunning()) {
    int32_t temp;
    auto start = std::chrono::high_resolution_clock::now();
    if (state.thread_index == 0) {
      hash_table.Lookup(distribution(mt), temp);
      if (measure_lookup) {
        set_iteration_time(start);
      }
    } else if (state.thread_index <= state.threads / 2) {
      auto added_key = distribution(mt);
      hash_table.Insert(added_key, distribution(mt));
      added_keys[put.fetch_add(1)].store(added_key);
      if (measure_insert) {
        set_iteration_time(start);
      }
    } else {
      hash_table.Remove(added_keys[get.fetch_add(1)].load());
      if (measure_remove) {
        set_iteration_time(start);
      }
    }
  }

  if (state.thread_index == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    hash_table.clear();
    put.store(0);
    get.store(0);
  }
}

void MeasureLookup(benchmark::State& state) {
  ManyLookups(
      state,
      /*measure_lookup =*/ true,
      /*measure_insert =*/ false,
      /*measure_remove =*/ false
  );
}

void MeasureInsert(benchmark::State& state) {
  ManyLookups(
      state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ true,
      /*measure_remove =*/ false
  );
}

void MeasureRemove(benchmark::State& state) {
  ManyLookups(
      state,
      /*measure_lookup =*/ false,
      /*measure_insert =*/ false,
      /*measure_remove =*/ true
  );
}

BENCHMARK(MeasureLookup)->Threads(4)->UseManualTime();
BENCHMARK(MeasureInsert)->Threads(4)->UseManualTime();
BENCHMARK(MeasureRemove)->Threads(4)->UseManualTime();
