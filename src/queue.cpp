// Simple lock-free queue for one-producer and one-consumer only.
// Code by Herb Sutters.
// Taken from https://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448?pgno=2 .

#include <atomic>
#include "queue.h"

template <typename T>
LockFreeQueue<T>::LockFreeQueue() {
  first = divider = last =
    new Node( T() );           // add dummy separator
}

template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
  while (first != nullptr) {   // release the list
    Node* tmp = first;
    first = tmp->next;
    delete tmp;
  }
}

template <typename T>
void LockFreeQueue<T>::produce(const T& t) {
  last->next = new Node(t); // add the new item
  last = last->next; // publish it
  while( first != divider ) { // trim unused nodes
    Node* tmp = first;
    first = first->next;
    delete tmp;
  }
}

template <typename T>
T* LockFreeQueue<T>::consume() {
  if( divider != last ) {         // if queue is nonempty
    T* result = divider->next->value;  // C: copy it back
    divider = divider->next;   // D: publish that we took it
    return result;              // and report success
  }
  return nullptr;               // else report empty
}
