#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <iostream>
#include <atomic>
#include <pthread.h>
#include <thread>

template<typename T>
class ThreadLocal {
 private:
  struct Node {
    Node(Node* next)
        : next(next), data() {
    }
    std::atomic<Node*> next;
    T data;
  };

 public:
  ThreadLocal() {
    pthread_key_create(&object_key_, nullptr);
    head_ = new Node(nullptr);
    tail_.store(head_.load());
  }

  ~ThreadLocal() {
    clear_list();
  }

  T& operator*() {
    return *check_or_create_new_vertex();
  }

  T* operator->() {
    return check_or_create_new_vertex();
  }

  class Iterator {
   public:
    Iterator(Node* current)
        : current_(current) {
    }

    T& operator*() const {
      return current_->data;
    }

    T* operator->() const {
      return &(current_->data);
    }

    void operator++() {
      current_ = current_->next;
    }

    void operator++(int) {
      current_ = current_->next;
    }

    bool operator!=(const Iterator& rhs) const {
      return current_ != rhs.current_;
    }

   private:
    Node* current_;
  };

  Iterator begin() {
    return Iterator(head_.load()->next.load());
  }

  Iterator end() {
    return Iterator(nullptr);
  }

  void clear() {
    clear_list();
    head_ = new Node(nullptr);
    tail_.store(head_.load());
    pthread_key_create(&object_key_, nullptr);
  }

 private:
  void clear_list() {
    while (head_ != nullptr) {
      Node* next = head_.load()->next.load();
      delete head_;
      head_ = next;
    }
  }

  Node* get_pointer() {
    return static_cast<Node*>(pthread_getspecific(object_key_));
  }

  void set_pointer(Node* pointer) {
    pthread_setspecific(object_key_, static_cast<void*>(pointer));
  }

  T* check_or_create_new_vertex() {
    Node* pointer = get_pointer();
    if (pointer == nullptr) {
      Node* new_node_ptr = new Node(nullptr);
      pointer = new_node_ptr;
      set_pointer(new_node_ptr);
      Node* expected = nullptr;
      while (!tail_.load()->next.compare_exchange_strong(expected, new_node_ptr)) {
        expected = nullptr;
        std::this_thread::yield();
      }
      tail_.store(tail_.load()->next.load());
    }
    return &(pointer->data);
  }

  std::atomic<Node*> head_{nullptr};
  std::atomic<Node*> tail_{nullptr};
  pthread_key_t object_key_{0};
};
