#pragma once

#include "thread_local.h"

#include <vector>
#include <iostream>
#include <atomic>
#include <cassert>

class RCULock {
 public:
  void lock() {
    ReadLock();
  }

  void unlock() {
    ReadUnlock();
  }

  void ReadLock() {
    assert(*last_read_ % 2 == 0);
    ++(*last_read_);
  }

  void ReadUnlock() {
    assert(*last_read_ % 2 == 1);
    ++(*last_read_);
  }

  void Synchronize() {
    std::vector<std::atomic<uint64_t>*> current_timestamp;
    std::vector<uint64_t> synced_timestamp;
    for (auto& element : last_read_) {
      synced_timestamp.push_back(element.load());
      current_timestamp.push_back(&element);
    }
    for (int i = 0; i < current_timestamp.size(); i++) {
      if (!(synced_timestamp[i] & 1u)) {
        continue;
      }
      while (current_timestamp[i]->load() == synced_timestamp[i]) {
        std::this_thread::yield();
      }
    }
  }

 private:
  ThreadLocal<std::atomic<uint64_t>> last_read_;
};

