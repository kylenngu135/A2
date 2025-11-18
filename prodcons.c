/*
 *  prodcons module
 *  Producer Consumer module
 *
 *  Implements routines for the producer consumer module based on
 *  chapter 30, section 2 of Operating Systems: Three Easy Pieces
 *
 *  University of Washington, Tacoma
 *  TCSS 422 - Operating Systems
 */

// Include only libraries for this module
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "counter.h"
#include "matrix.h"
#include "pcmatrix.h"
#include "prodcons.h"

// Define Locks, Condition variables, and so on here

/// Protects bigmatrix
pthread_mutex_t bounded_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
/// Protects bigmatrix, wait on if you are trying to put
pthread_cond_t bounded_buffer_put_cond = PTHREAD_COND_INITIALIZER;
/// Protects bigmatrix, wait on if you are trying to get
pthread_cond_t bounded_buffer_get_cond = PTHREAD_COND_INITIALIZER;

/// Write head into the ring buffer
size_t bounded_buffer_write_idx = 0;
/// The number of entries readable behind the `bounded_buffer_write_idx`
size_t bounded_buffer_readable = 0;

// Bounded buffer put() get()
/// Thread safe
/// Whomever `get`s the value owns it, do not free it until then.
int put(Matrix * value) {
  assert(value != NULL);
  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

  while (bounded_buffer_readable == (size_t) BOUNDED_BUFFER_SIZE) {
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1  && "write_idx must be within the buffer");
    // no space to write, wait until a space opens up.
    pthread_cond_wait(&bounded_buffer_put_cond, &bounded_buffer_mutex);
  }
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exist");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have an extra slot open for the read");

  assert(bigmatrix[bounded_buffer_write_idx] == NULL && "overridden entry must have been cleared");
  bigmatrix[bounded_buffer_write_idx] = value;

  bounded_buffer_write_idx = (bounded_buffer_write_idx + 1) % (size_t) BOUNDED_BUFFER_SIZE;
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "new index must be within buffer");

  bounded_buffer_readable += 1;
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than space available");

  pthread_cond_signal(&bounded_buffer_get_cond);
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return 0;
}

/// Thread safe
/// Caller takes ownership of return, and it just be freed.
Matrix * get() {
  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

  while (bounded_buffer_readable <= 0) {
    // nothing to read, wait until a slot fills.
    pthread_cond_wait(&bounded_buffer_get_cond, &bounded_buffer_mutex);
  }
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exist");
  assert(bounded_buffer_readable >= 1 && "must have at least 1 slot to read");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have an extra slot open for the read");

  // TODO(Elijah): Does this math, maths?
  size_t idx = (bounded_buffer_write_idx >= bounded_buffer_readable)
    ? bounded_buffer_write_idx - bounded_buffer_readable
    : (size_t) BOUNDED_BUFFER_SIZE - (bounded_buffer_readable - bounded_buffer_write_idx);

  assert(idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "retreival index just be be within the buffer");

  assert(bigmatrix[idx] != NULL && "Entry read be filled (i.e. not NULL)");
  Matrix *value = bigmatrix[idx];
  bigmatrix[idx] = NULL; // clear position we just took

  bounded_buffer_readable -= 1;
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have less entries than buffer size (-1 since we just took one)");

  pthread_cond_signal(&bounded_buffer_put_cond);
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return value;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg) {
  (void) arg;
  return NULL;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg) {
  (void) arg;
  return NULL;
}
