#pragma once
#include <atomic>
#include <time.h>

#include "queue.h"

namespace {
  template<typename T>
  struct tlist {
    time_t ts;
    T* content;
    tlist* next;
  };

  template<typename T>
  tlist<T>* merge_tlists(tlist<T>* dest, tlist<T>* src) {
    if (dest == nullptr) {
      return src;
    }
    if (src == nullptr) {
      return dest;
    }
    if (dest->ts <= src->ts) {
      dest->next = merge_tlists(dest->next, src);
      return dest;
    } else {
      src->next = merge_tlists(dest, src->next);
      return src;
    }
  }

  // pre: one is not NULL, and one->next=NULL
  template<typename T>
  tlist<T>* insert(tlist<T>* dest, tlist<T>* one) {
    if (dest == nullptr) return one;
    if (dest->ts <= one->ts) {
      dest->next = merge(dest->next, one);
      return dest;
    } else {
      one->next = dest;
      return one;
    }
  }
}

template<typename T>
class TimeQueue {
  private:
  tlist<T>* tl = nullptr;
  void (*_on_expiry)(T*);
  void __main_loop(void* _arg) {
    struct pollfd fds[1];
    fds[0].fd = Shutdown::fd_in;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    do {
      if (fds[0].revents & POLLIN) {
        return;
      }
      now = time(NULL);
      // pop all enqueued
      struct tlist<T>* tlnew = NULL;
      struct tlist<T>* next = NULL;
      while (true) {
        next = queue->consume();
        if (next != nullptr) {
          next->next = tlnew;
          tlnew = next;
        } else {
          break;
        }
      }
      // merge tl with 
      if (tlnew != nullptr) tl = merge_tlists(tl, tlnew);
      // exec
      while (tl != nullptr) {
        if (tl->ts < now) {
          struct tlist<T>* item = tl;
          tl = tl->next;
          _on_expiry(item->content);
          free(item);
        } else {
          break;
        }
      }
    } while (poll(fds, 1, 500) >= 0);
  }
  LockFreeQueue<tlist<T>> queue;
  public:
  std::atomic<time_t> now;
  // _on_expiry is responsible of freeing the received pointer
  TimeQueue(void (*on_expiry)(T*)): _on_expiry(on_expiry) {};
  // Takes ownership of the enqueued pointer.
  void enqueue(T* t, int expiry) {
    tlist<T>* n = new tlist<T>;
    n->content = t;
    n->next = NULL;
    n->ts = expiry;
    queue.produce(n);
  }
  int main_loop() {
    pthread_t th;
    return pthread_create(&th, NULL, &__main_loop, NULL);
  }
  
};
