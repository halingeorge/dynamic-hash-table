#pragma once

#include "thread_local.h"

#include <vector>
#include <iostream>
#include <atomic>

class RCULock {
 public:
  void ReadLock() {
    ++(*last_read_);
  }

  void ReadUnlock() {
    ++(*last_read_);
  }

  void Synchronize() {
    std::vector<std::atomic<int>*> cur_ts;
    std::vector<int> sync_ts;
    for (auto& el : last_read_) {
      sync_ts.push_back(el.load());
      cur_ts.push_back(&el);
    }
    for (int i = 0; i < cur_ts.size(); i++) {
	  while ((sync_ts[i] & 1) && cur_ts[i]->load() == sync_ts[i]) {
	    std::this_thread::yield();
	  }
    }
  }

  ThreadLocal<std::atomic<int>> last_read_;
};

