#pragma once

#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>

#include "thread_local.h"

namespace rcu_lock_internal {

template <typename T>
class CopyableAtomic : public std::atomic<T> {
 public:
  using std::atomic<T>::atomic;

  CopyableAtomic(const CopyableAtomic& rhs)
      : std::atomic<T>::atomic(rhs.load()) {}
};

}  // namespace rcu_lock_internal

class RCULock {
 public:
  void lock() { ReadLock(); }

  void unlock() { ReadUnlock(); }

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
  ThreadLocal<rcu_lock_internal::CopyableAtomic<uint64_t>> last_read_;
};

class RCUPerBucketLock {
 public:
  explicit RCUPerBucketLock(size_t bucket_count) : last_read_(bucket_count) {}

  void lock(size_t bucket_number) { ReadLock(bucket_number); }

  void unlock(size_t bucket_number) { ReadUnlock(bucket_number); }

  void ReadLock(size_t bucket_number) {
    assert((*last_read_)[bucket_number] % 2 == 0);
    ++(*last_read_)[bucket_number];
  }

  void ReadUnlock(size_t bucket_number) {
    assert((*last_read_)[bucket_number] % 2 == 1);
    ++(*last_read_)[bucket_number];
  }

  void Synchronize(size_t bucket_number) {
    std::vector<std::atomic<uint64_t>*> current_timestamp;
    std::vector<uint64_t> synced_timestamp;
    for (auto& element : last_read_) {
      synced_timestamp.push_back(element[bucket_number].load());
      current_timestamp.push_back(&element[bucket_number]);
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
  ThreadLocal<std::vector<rcu_lock_internal::CopyableAtomic<uint64_t>>>
      last_read_;
};
