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
class HashTable;

namespace hash_table_internals {

template<typename Key, typename Value>
class HashTableImpl {
 private:
  class Bucket {
    friend class HashTableImpl;

   public:
    Bucket() : head_(new Node()) {
    }

    ~Bucket() {
      for (size_t index = 0; index < 2; index++) {
        while (head_) {
          auto next = head_.load()->next[index].load();
          delete head_;
          head_.store(next);
        }
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
        hash_table_->Resize(hash_table_->BucketCount() * 2 + 1);
      }
      return found;
    }

   private:
    static constexpr uint32_t kBucketNodeCountBeforeResize = 5;

    HashTable<Key, Value>* hash_table_ = nullptr;
    std::atomic<Node*> head_ = nullptr;
    RCULock lock_;
    std::mutex mutex_;
  };

 public:
  explicit HashTableImpl(size_t bucket_count, HashTable<Key, Value>* hash_table)
      : master_hash_table_(hash_table), buckets_(InitBuckets(bucket_count)), bucket_count_(bucket_count) {
  }

  bool Insert(Key key, Value value) {
    auto[bucket, index] = GetBucket(this, key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->Insert(std::move(key), std::move(value), index);
  }

  bool Remove(const Key& key) {
    auto[bucket, index] = GetBucket(this, key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->Remove(key, index);
  }

  bool Lookup(const Key& key, Value& value) {
    auto[bucket, index] = GetBucket(this, key);
    return bucket->Lookup(key, value, index);
  }

  void Clear() {
    auto temp = InitBuckets(bucket_count_);
    buckets_.swap(temp);
  }

 public:
  using HashResultType = typename std::hash<Key>::result_type;

  size_t BucketCount() const {
    return buckets_.size();
  }

  std::vector<Bucket> InitBuckets(size_t bucket_count) {
    std::vector<Bucket> buckets(bucket_count);
    for (size_t i = 0; i < bucket_count; i++) {
      buckets[i].hash_table_ = master_hash_table_;
    }
    return buckets;
  }

  static std::pair<Bucket*, int32_t> GetBucketInSpecifiedHashTable(HashTableImpl* hash_table, const Key& key) {
    auto hash = hash_table->hasher_(key) % hash_table->buckets_.size();
    return {&hash_table->buckets_[hash], hash};
  }

  static std::pair<Bucket*, int32_t> GetBucket(HashTableImpl* hash_table, const Key& key) {
    auto[bucket, bucket_number] = GetBucketInSpecifiedHashTable(hash_table, key);
    auto index = hash_table->current_index_;
    if (bucket_number <= hash_table->resize_index_.load()) {
      HashTableImpl* new_table = hash_table->new_table_.load();
      auto[new_bucket, new_bucket_number] = GetBucketInSpecifiedHashTable(new_table, key);
      bucket = new_bucket;
      index = new_table->current_index_;
    }
    return {bucket, index};
  }

  HashTableImpl* ReallocateToNewHashTable(size_t new_bucket_count) {
    return new HashTableImpl(new_bucket_count, master_hash_table_);
  }

 private:
  HashTable<Key, Value>* const master_hash_table_;
  std::vector<Bucket> buckets_;
  std::hash<Key> hasher_;
  size_t bucket_count_ = 0;
  size_t current_index_ = 0;

 private:
  std::atomic<HashTableImpl*> new_table_ = nullptr;
  std::atomic<int32_t> resize_index_ = -1;
};

}  // namespace hash_table_internals

template<typename Key, typename Value>
class HashTable {
  using HashTableImpl = hash_table_internals::HashTableImpl<Key, Value>;

  friend class hash_table_internals::HashTableImpl<Key, Value>;

 public:
  explicit HashTable(size_t bucket_count) : hash_table_impl_(new HashTableImpl(bucket_count, this)) {
  }

  ~HashTable() {
    delete hash_table_impl_.load();
  }

  bool Insert(Key key, Value value) {
    std::unique_lock<RCULock> rcu_lock(lock_);
    return hash_table_impl_.load()->Insert(std::move(key), std::move(value));
  }

  bool Remove(const Key& key) {
    std::unique_lock<RCULock> rcu_lock(lock_);
    return hash_table_impl_.load()->Remove(key);
  }

  bool Lookup(const Key& key, Value& value) {
    std::unique_lock<RCULock> rcu_lock(lock_);
    return hash_table_impl_.load()->Lookup(key, value);
  }

  void Clear() {
    hash_table_impl_.load()->Clear();
  }

 private:
  size_t BucketCount() const {
    return hash_table_impl_.load()->BucketCount();
  }

  void Resize(size_t bucket_count) {
    return;
    if (!resize_mutex_.try_lock()) {
      return;
    }
    auto* old_hash_table = hash_table_impl_.load();
    hash_table_impl_.store(hash_table_impl_.load()->ReallocateToNewHashTable(bucket_count));
    lock_.Synchronize();
    delete old_hash_table;
    ++resize_count_;
  }

 private:
  std::atomic<HashTableImpl*> hash_table_impl_;
  RCULock lock_;
  std::mutex resize_mutex_;
  std::atomic<std::uint32_t> resize_count_ = 0;
};