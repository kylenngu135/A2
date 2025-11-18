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
int put(Matrix * value) {
  assert(value != NULL);
  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE);
  assert(bounded_buffer_readable >= 0);
  assert(bounded_buffer_write_idx <= BOUNDED_BUFFER_SIZE - 1);
  assert(bounded_buffer_write_idx >= 0);

  while (bounded_buffer_readable == BOUNDED_BUFFER_SIZE) {
    pthread_cond_wait(&bounded_buffer_put_cond, &bounded_buffer_mutex);
  }
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE);
  assert(bounded_buffer_readable >= 0);
  assert(bounded_buffer_write_idx <= BOUNDED_BUFFER_SIZE - 1);
  assert(bounded_buffer_write_idx >= 0);

  assert(bigmatrix[bounded_buffer_write_idx] == NULL);
  bigmatrix[bounded_buffer_write_idx] = value;

  bounded_buffer_write_idx = (bounded_buffer_write_idx + 1) % BOUNDED_BUFFER_SIZE;
  assert(bounded_buffer_write_idx <= BOUNDED_BUFFER_SIZE - 1);
  assert(bounded_buffer_write_idx >= 0);

  bounded_buffer_readable += 1;
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE);
  assert(bounded_buffer_readable >= 1);

  pthread_cond_signal(&bounded_buffer_get_cond);
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return 0;
}

Matrix * get() {
  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE);
  assert(bounded_buffer_readable >= 0);
  assert(bounded_buffer_write_idx <= BOUNDED_BUFFER_SIZE - 1);
  assert(bounded_buffer_write_idx >= 0);

  while (bounded_buffer_readable <= 0) {
    pthread_cond_wait(&bounded_buffer_get_cond, &bounded_buffer_mutex);
  }
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE);
  assert(bounded_buffer_readable >= 1);

  size_t idx;
  // TODO(Elijah): Does this math, maths?
  if (bounded_buffer_write_idx >= bounded_buffer_readable) idx = bounded_buffer_write_idx - bounded_buffer_readable;
  else idx = BOUNDED_BUFFER_SIZE - (bounded_buffer_readable - bounded_buffer_write_idx);

  assert(idx <= BOUNDED_BUFFER_SIZE - 1);
  assert(idx >= 0);

  assert(bigmatrix[idx] != NULL);
  Matrix *value = bigmatrix[idx];
  bigmatrix[idx] = NULL;

  bounded_buffer_readable -= 1;
  assert(bounded_buffer_readable <= BOUNDED_BUFFER_SIZE - 1);
  assert(bounded_buffer_readable >= 0);

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
