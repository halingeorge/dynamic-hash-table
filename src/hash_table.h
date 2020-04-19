#pragma once
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcu_lock.h"

template <typename Key, typename Value>
class HashTable;

namespace hash_table_internals {

template <typename Key, typename Value>
class HashTableImpl {
 private:
  class Bucket {
    friend class HashTableImpl;

   public:
    struct Node {
      Key key;
      Value value;
      std::array<std::atomic<Node*>, 2> next{nullptr, nullptr};
    };

   public:
    Bucket() : head_(new Node()) {}

    ~Bucket() {
      auto head = head_.load();
      while (head) {
        auto next = head->next[index_to_cleanup_].load();
        delete head;
        head = next;
      }
    }

    bool Insert(const Key& key, const Value& value, size_t index) {
      if (Find(key, index)) {
        return false;
      }
      Node* new_node{new Node()};
      new_node->key = key;
      new_node->value = value;
      LinkNode(new_node, index);
      total_element_count_->fetch_add(1);
      return true;
    }

    void LinkNode(Node* new_node, size_t index) {
      new_node->next[index].store(head_.load()->next[index]);
      head_.load()->next[index].store(new_node);
    }

    bool Remove(const Key& key, size_t index) {
      auto head = head_.load();
      while (head->next[index].load() != nullptr &&
             head->next[index].load()->key != key) {
        head = head->next[index].load();
      }
      if (head->next[index].load() == nullptr) {
        return false;
      }
      auto next = head->next[index].load();
      head->next[index].store(head->next[index].load()->next[index].load());
      lock_.Synchronize();
      delete next;
      total_element_count_->fetch_add(-1);
      return true;
    }

    bool Lookup(const Key& key, Value& value, int index) {
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
    bool Find(const Key& key, size_t index) {
      std::unique_lock<RCULock> lock(lock_);
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
        hash_table_->NeedResize(hash_table_->BucketCount() * 2 + 1);
      }
      return found;
    }

   private:
    static constexpr uint32_t kBucketNodeCountBeforeResize = 3;

    HashTable<Key, Value>* hash_table_ = nullptr;
    std::atomic<Node*> head_ = nullptr;
    std::atomic<size_t>* total_element_count_ = nullptr;
    RCULock lock_;
    size_t index_to_cleanup_ = std::numeric_limits<size_t>::max();
    std::mutex mutex_;
  };

 public:
  explicit HashTableImpl(size_t bucket_count, HashTable<Key, Value>* hash_table,
                         size_t current_index = 0)
      : master_hash_table_(hash_table),
        current_index_(current_index),
        buckets_(InitBuckets(bucket_count)) {}

  void LinkNode(typename Bucket::Node* node) {
    auto [bucket, bucket_number] =
        GetBucketInSpecifiedHashTable(this, node->key);
    std::unique_lock<std::mutex> lock(bucket->mutex_);
    return bucket->LinkNode(node, current_index_);
  }

  bool Insert(const Key& key, const Value& value) {
    UpdateModeOn(key);
    auto [bucket, index] = GetBucket(this, key);
    auto result = bucket->Insert(key, value, index);
    UpdateModeOff(key);
    return result;
  }

  bool Remove(const Key& key) {
    UpdateModeOn(key);
    auto [bucket, index] = GetBucket(this, key);
    auto result = bucket->Remove(key, index);
    if (Bucket::kBucketNodeCountBeforeResize * BucketCount() >=
        element_count_ * 2) {
      master_hash_table_->NeedResize(BucketCount() / 2 + 1);
    }
    UpdateModeOff(key);
    return result;
  }

  bool Lookup(const Key& key, Value& value) {
    auto [bucket, bucket_number] = GetBucketInSpecifiedHashTable(this, key);
    {
      std::unique_lock lock(bucket->lock_);
      if (bucket_number >= resize_index_.load() &&
          bucket->Lookup(key, value, current_index_)) {
        return true;
      }
    }

    auto [new_bucket, new_index] = GetBucket(this, key);
    if (new_bucket != bucket) {
      std::unique_lock lock(new_bucket->lock_);
      return new_bucket->Lookup(key, value, new_index);
    }

    return false;
  }

  void Clear() {
    auto temp = InitBuckets(buckets_.size());
    buckets_.swap(temp);
    element_count_ = 0;
  }

 public:
  using HashResultType = typename std::hash<Key>::result_type;

  void UpdateModeOn(const Key& key) {
    auto [bucket, bucket_number] = GetBucketInSpecifiedHashTable(this, key);
    bucket->mutex_.lock();
    if (bucket_number > resize_index_.load()) {
      return;
    }
    auto [new_bucket, new_index] =
        GetBucketInSpecifiedHashTable(new_table_.load(), key);
    new_bucket->mutex_.lock();
    bucket->mutex_.unlock();
  }

  void UpdateModeOff(const Key& key) {
    auto [bucket, index] = GetBucket(this, key);
    bucket->mutex_.unlock();
  }

  size_t BucketCount() const { return buckets_.size(); }

  std::vector<Bucket> InitBuckets(size_t bucket_count) {
    std::vector<Bucket> buckets(bucket_count);
    for (size_t i = 0; i < bucket_count; i++) {
      buckets[i].hash_table_ = master_hash_table_;
      buckets[i].index_to_cleanup_ = current_index_;
      buckets[i].total_element_count_ = &element_count_;
    }
    return buckets;
  }

  static std::pair<Bucket*, int32_t> GetBucketInSpecifiedHashTable(
      HashTableImpl* hash_table, const Key& key) {
    auto hash = hash_table->hasher_(key) % hash_table->buckets_.size();
    return {&hash_table->buckets_[hash], hash};
  }

  static std::pair<Bucket*, int32_t> GetBucket(HashTableImpl* hash_table,
                                               const Key& key) {
    auto [bucket, bucket_number] =
        GetBucketInSpecifiedHashTable(hash_table, key);
    auto index = hash_table->current_index_;
    if (bucket_number <= hash_table->resize_index_.load()) {
      HashTableImpl* new_table = hash_table->new_table_.load();
      auto [new_bucket, new_bucket_number] =
          GetBucketInSpecifiedHashTable(new_table, key);
      bucket = new_bucket;
      index = new_table->current_index_;
    }
    return {bucket, index};
  }

  HashTableImpl* ReallocateToNewHashTable(size_t new_bucket_count) {
    auto* new_table = new HashTableImpl(new_bucket_count, master_hash_table_,
                                        current_index_ ^ 1u);
    new_table_.store(new_table);
    master_hash_table_->lock_.Synchronize();
    for (size_t i = 0; i < buckets_.size(); i++) {
      auto* bucket = &buckets_[i];
      std::unique_lock<std::mutex> lock(bucket->mutex_);
      resize_index_.store(i);
      auto* current_node = bucket->head_.load()->next[current_index_].load();
      while (current_node) {
        new_table->LinkNode(current_node);
        current_node = current_node->next[current_index_].load();
      }
      bucket->head_.load()->next[current_index_] = nullptr;
      bucket->lock_.Synchronize();
    }
    ++resize_index_;
    return new_table;
  }

 private:
  HashTable<Key, Value>* const master_hash_table_;
  size_t current_index_ = 0;
  std::vector<Bucket> buckets_;
  std::hash<Key> hasher_;

 private:
  std::atomic<HashTableImpl*> new_table_ = nullptr;
  std::atomic<int32_t> resize_index_ = -1;
  std::atomic<size_t> element_count_ = 0;
};

}  // namespace hash_table_internals

template <typename Key, typename Value>
class HashTable {
  using HashTableImpl = hash_table_internals::HashTableImpl<Key, Value>;

  friend class hash_table_internals::HashTableImpl<Key, Value>;

 public:
  explicit HashTable(size_t bucket_count)
      : hash_table_impl_(new HashTableImpl(bucket_count, this)) {}

  ~HashTable() {
    std::unique_lock<RCULock> rcu_lock(lock_);
    delete hash_table_impl_.load();
  }

  bool Insert(const Key& key, const Value& value) {
    bool result;
    {
      std::unique_lock<RCULock> rcu_lock(lock_);
      result = hash_table_impl_.load()->Insert(key, value);
    }
    if (resize_bucket_count_.load() != -1) {
      Resize(resize_bucket_count_.load());
    }
    return result;
  }

  bool Remove(const Key& key) {
    bool result;
    {
      std::unique_lock<RCULock> rcu_lock(lock_);
      result = hash_table_impl_.load()->Remove(key);
    }
    if (resize_bucket_count_.load() != -1) {
      Resize(resize_bucket_count_.load());
    }
    return result;
  }

  bool Lookup(const Key& key, Value& value) {
    std::unique_lock<RCULock> rcu_lock(lock_);
    return hash_table_impl_.load()->Lookup(key, value);
  }

  void Clear() {
    std::unique_lock<RCULock> rcu_lock(lock_);
    hash_table_impl_.load()->Clear();
  }

 private:
  size_t BucketCount() const { return hash_table_impl_.load()->BucketCount(); }

  void Resize(size_t bucket_count) {
    if (!resize_mutex_.try_lock()) {
      return;
    }
    auto* old_hash_table = hash_table_impl_.load();
    if (old_hash_table->BucketCount() == bucket_count) {
      resize_bucket_count_ = -1;
      resize_mutex_.unlock();
      return;
    }
    if (old_hash_table->BucketCount() < bucket_count) {
      std::cout << "Increase from " << old_hash_table->BucketCount() << " to "
                << bucket_count << std::endl;
    } else {
      std::cout << "Decrease from " << old_hash_table->BucketCount() << " to "
                << bucket_count << std::endl;
    }
    hash_table_impl_.store(
        old_hash_table->ReallocateToNewHashTable(bucket_count));
    lock_.Synchronize();
    resize_bucket_count_ = -1;
    resize_mutex_.unlock();
    delete old_hash_table;
    ++resize_count_;
  }

  void NeedResize(size_t bucket_count) {
    if (bucket_count >= kMaxBucketNumber) {
      return;
    }
    if (resize_bucket_count_.load() != -1) {
      return;
    }
    int32_t expected = -1;
    resize_bucket_count_.compare_exchange_strong(expected, bucket_count);
  }

 private:
  static constexpr size_t kMaxBucketNumber = 512;

  std::atomic<HashTableImpl*> hash_table_impl_;
  RCULock lock_;
  std::mutex resize_mutex_;
  std::atomic<std::uint32_t> resize_count_ = 0;
  std::atomic<int32_t> resize_bucket_count_ = -1;
};