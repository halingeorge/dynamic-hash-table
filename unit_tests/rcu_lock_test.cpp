#include "rcu_lock.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

TEST(RCULock, SynchronizeWithReaders) {
    const size_t kReaders = 17;
    const int32_t kMinSleep = 100;
    const int32_t kMaxSleep = 1000;

    RCULock rcu_lock;

    std::atomic<size_t> readers_completed{0};

    auto reader_routine = [&]() {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_int_distribution<int> distribution(kMinSleep, kMaxSleep);

        rcu_lock.ReadLock();

        std::this_thread::sleep_for(std::chrono::milliseconds(distribution(mt)));

        readers_completed.fetch_add(1);

        rcu_lock.ReadUnlock();
    };

    std::vector<std::thread> threads;
    threads.reserve(kReaders);
    for (size_t i = 0; i < kReaders; ++i) {
        threads.emplace_back(reader_routine);
    }
    for (size_t i = 0; i < kReaders; i++) {
        threads[i].join();
    }

    rcu_lock.Synchronize();
    ASSERT_EQ(readers_completed.load(), kReaders);
}
