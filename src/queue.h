// Simple lock-free queue for one-producer and one-consumer only.
// Code by Herb Sutters.
// Taken from https://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448?pgno=2 .

#include <atomic>

template <typename T>
class LockFreeQueue {
  private:
  struct Node {
    Node( T val ) : value(val), next(nullptr) { }
    T value;
    Node* next;
  };
  Node* first; // for producer only
  std::atomic<Node*> divider, last; // shared

  public:
  LockFreeQueue();
  ~LockFreeQueue();
  void produce(const T&);
  T* consume();
};
