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
    friend class HashTable;

   public:
    Bucket() : head_(new Node()) {
    }

    ~Bucket() {
      while (head_) {
        auto next = head_.load()->next[hash_table_->current_index_].load();
        delete head_;
        head_.store(next);
      }
    }

    bool Insert(Key key, Value value, size_t index) {
      if (Find(key, index)) {
        return false;
      }
      std::atomic<Node*> new_node{new Node()};
      new_node.load()->next[index].store(head_.load()->next[index]);
      new_node.load()->key = std::move(key);
      new_node.load()->value = std::move(value);
      head_.load()->next[index].store(new_node);
      return true;
    }

    bool Remove(const Key& key, size_t index) {
      if (head_ == nullptr) {
        return false;
      }
      auto head = head_.load();
      while (head->next[index].load() != nullptr && head->next[index].load()->key != key) {
        head = head->next[index].load();
      }
      if (head->next[index].load() == nullptr) {
        return false;
      }
      auto next = head->next[index].load();
      head->next[index].store(head->next[index].load()->next[index].load());
      lock_.Synchronize();
      delete next;
      return true;
    }

    bool Lookup(const Key& key, Value& value, int index) {
      std::lock_guard<RCULock> lock(lock_);
      std::optional<Value> result;
      auto head = head_.load()->next[index].load();
      while (head != nullptr) {
        if (head->key == key) {
          result = head->value;
          break;
        }
        head = head->next[index].load();
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
      std::array<std::atomic<Node*>, 2> next{nullptr, nullptr};
    };

   private:
    bool Find(const Key& key, size_t index) {
      auto head = head_.load()->next[index].load();
      uint32_t scanned_count = 0;
      bool found = false;
      while (head != nullptr) {
        ++scanned_count;
        if (head->key == key) {
          found = true;
          break;
        }
        head = head->next[index];
      }
      if (scanned_count >= kBucketNodeCountBeforeResize) {
        hash_table_->TryToResize(hash_table_->bucket_count_ * 2 + 1);
      }
      return found;
    }

   private:
    static constexpr uint32_t kBucketNodeCountBeforeResize = 5;

    HashTable* hash_table_ = nullptr;
    std::atomic<Node*> head_ = nullptr;
    RCULock lock_;
    std::mutex mutex_;
  };

 public:
  explicit HashTable(size_t bucket_count)
      : buckets_(InitBuckets(bucket_count)), bucket_count_(bucket_count), current_table_(this) {
  }

  bool Insert(Key key, Value value) {
    auto [bucket, index] = GetBucket(current_table_.load(), key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->Insert(std::move(key), std::move(value), index);
  }

  bool Remove(const Key& key) {
    auto [bucket, index] = GetBucket(current_table_.load(), key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->Remove(key, index);
  }

  bool Lookup(const Key& key, Value& value) {
    auto [bucket, index] = GetBucket(current_table_.load(), key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->Lookup(key, value, index);
  }

  void clear() {
    auto temp = InitBuckets(bucket_count_);
    buckets_.swap(temp);
  }

 private:
  using HashResultType = typename std::hash<Key>::result_type;

  std::vector<Bucket> InitBuckets(size_t bucket_count) {
    std::vector<Bucket> buckets(bucket_count);
    for (size_t i = 0; i < bucket_count; i++) {
      buckets[i].hash_table_ = this;
    }
    return buckets;
  }

  static std::pair<Bucket*, int32_t> GetBucketInSpecifiedHashTable(HashTable* hash_table, const Key& key) {
    auto hash = hash_table->hasher_(key) % hash_table->buckets_.size();
    return {&hash_table->buckets_[hash], hash};
  }

  static std::pair<Bucket*, int32_t> GetBucket(HashTable* hash_table, const Key& key) {
    HashTable* current_table = hash_table->current_table_.load();
    auto [bucket, bucket_number] = GetBucketInSpecifiedHashTable(current_table, key);
    auto index = current_table->current_index_;
    if (bucket_number <= current_table->resize_index_.load()) {
      HashTable* new_table = hash_table->new_table_.load();
      auto [new_bucket, new_bucket_number] = GetBucketInSpecifiedHashTable(new_table, key);
      bucket = new_bucket;
      index = new_table->current_index_;
    }
    return {bucket, index};
  }

  void TryToResize(size_t new_bucket_count) {
  }

 private:
  std::vector<Bucket> buckets_;
  std::hash<Key> hasher_;
  size_t bucket_count_ = 0;
  size_t current_index_ = 0;

 private:
  std::atomic<HashTable*> current_table_ = nullptr;
  std::atomic<HashTable*> new_table_ = nullptr;
  std::atomic<int32_t> resize_index_ = -1;
  std::uint32_t resize_count_ = 0;
  std::mutex resize_mutex_;
  RCULock lock_;
};
