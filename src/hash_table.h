#pragma once
#include "rcu_lock.h"

#include <functional>
#include <vector>
#include <optional>
#include <mutex>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <mutex>

template<typename Key, typename Value>
class HashTable {
 private:
  class Bucket {
   public:
    Bucket() : head_(new Node()) {
    }

    ~Bucket() {
      while (head_) {
        auto next = head_.load()->next.load();
        delete head_;
        head_.store(next);
      }
    }

    bool Insert(Key key, Value value) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (Find(key)) {
        return false;
      }
      std::atomic<Node*> new_node{new Node()};
      new_node.load()->next.store(head_.load()->next);
      new_node.load()->key = std::move(key);
      new_node.load()->value = std::move(value);
      head_.load()->next.store(new_node);
      return true;
    }

    bool Remove(const Key& key) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (head_ == nullptr) {
        return false;
      }
      auto head = head_.load();
      while (head->next.load() != nullptr && head->next.load()->key != key) {
        head = head->next.load();
      }
      if (head->next.load() == nullptr) {
        return false;
      }
      auto next = head->next.load();
      head->next.store(head->next.load()->next.load());
      lock_.Synchronize();
      delete next;
      return true;
    }

    // Wait-free
    bool Lookup(const Key& key, Value& value) {
      std::lock_guard<RCULock> lock(lock_);
      std::optional<Value> result;
      auto head = head_.load()->next.load();
      while (head != nullptr) {
        if (head->key == key) {
          result = head->value;
          break;
        }
        head = head->next.load();
      }
      if (result.has_value()) {
        value = std::move(*result);
        return true;
      }
      return false;
    }

   private:
    struct Node {
      Key key;
      Value value;
      std::atomic<Node*> next = nullptr;
    };

    bool Find(const Key& key) {
      auto head = head_.load()->next.load();
      while (head != nullptr) {
        if (head->key == key) {
          return true;
        }
        head = head->next;
      }
      return false;
    }

    std::mutex mutex_;
    RCULock lock_;
    std::atomic<Node*> head_ = nullptr;
  };

 public:
  explicit HashTable(size_t num_buckets)
      : buckets_(num_buckets), num_buckets_(num_buckets) {
  }

  bool Insert(Key key, Value value) {
    return buckets_[hasher_(key) % buckets_.size()].Insert(std::move(key), std::move(value));
  }

  bool Remove(const Key& key) {
    return buckets_[hasher_(key) % buckets_.size()].Remove(key);
  }

  // Wait-free
  bool Lookup(const Key& key, Value& value) {
    return buckets_[hasher_(key) % buckets_.size()].Lookup(key, value);
  }

  void clear() {
    auto temp = std::vector<Bucket>{num_buckets_};
    buckets_.swap(temp);
  }

 private:
  std::atomic<HashTable*> current_table_ = nullptr;
  std::atomic<HashTable*> new_table_ = nullptr;
  std::atomic<int32_t> resize_index_ = -1;
  std::uint32_t resize_count_ = 0;
  std::mutex resize_mutex_;

 private:
  std::vector<Bucket> buckets_;
  std::hash<Key> hasher_;
  size_t num_buckets_;
};
