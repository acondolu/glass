// Simple lock-free queue for one-producer and one-consumer only.
// Code by Herb Sutters.
// Taken from https://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448?pgno=2 .
#pragma once
#include <atomic>

template <typename T>
class LockFreeQueue {
  private:
  struct Node {
    Node( T* val ) : value(val), next(nullptr) { }
    T* value;
    Node* next;
  };
  Node* first; // for producer only
  std::atomic<Node*> divider;
  std::atomic<Node*> _last; // shared

  public:
  LockFreeQueue() {
    first = divider = _last = new Node(nullptr);
  };
  ~LockFreeQueue() {
    while (first != nullptr) {
      Node* tmp = first;
      first = tmp->next;
      delete tmp;
    }
  };
  // Queue takes ownership of pointer.
  void produce(T* t) {
    Node* l = _last.load();
    l->next = new Node(t);
    _last.store(l->next);
    while( first != divider ) {
      Node* tmp = first;
      first = first->next;
      delete tmp;
    }
  };
  // Caller is responsible of freeing the result.
  T* consume() {
    if (divider != _last) {
      T* result = divider->next->value;
      divider = divider->next;
      return result;
    }
    return nullptr;
  };
};
