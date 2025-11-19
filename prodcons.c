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

/// Protects bigmatrix, bounded_buffer_write_idx, and bounded_buffer_readable
pthread_mutex_t bounded_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
/// Protects bigmatrix, wait on if you are trying to put
pthread_cond_t bounded_buffer_put_cond = PTHREAD_COND_INITIALIZER;
/// Protects bigmatrix, wait on if you are trying to get
pthread_cond_t bounded_buffer_get_cond = PTHREAD_COND_INITIALIZER;

/// Write head into the ring buffer
/// protected by bounded_buffer_mutex
size_t bounded_buffer_write_idx = 0;
/// The number of entries readable behind the `bounded_buffer_write_idx`
/// protected by bounded_buffer_mutex
size_t bounded_buffer_readable = 0;

// Bounded buffer put() get()
/// Thread safe
/// Whomever `get`s the value owns it, do not free it until then.
/// Returns -1 if it failed to put the value.
int put(Matrix *value) {
  assert(value != NULL);
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");

  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

  if (bounded_buffer_readable == (size_t) BOUNDED_BUFFER_SIZE) {
    // no space to write, wait until a space opens up.
    pthread_cond_wait(&bounded_buffer_put_cond, &bounded_buffer_mutex);
    if (bounded_buffer_readable == (size_t) BOUNDED_BUFFER_SIZE) goto error;
  }

  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "cannot have more readable than slots exist and it musn't be full");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have an extra slot open for the read");

  assert(bigmatrix[bounded_buffer_write_idx] == NULL && "overridden entry must have been cleared");
  bigmatrix[bounded_buffer_write_idx] = value;

  bounded_buffer_readable += 1;
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than space available");

  bounded_buffer_write_idx = (bounded_buffer_write_idx + 1) % (size_t) BOUNDED_BUFFER_SIZE;
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "new index must be within buffer");

  pthread_cond_signal(&bounded_buffer_get_cond);
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return 0;

error:
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return -1;
}

/// Thread safe
/// Caller takes ownership of return, and it just be freed.
/// returns NULL if it failed to reserve a slot
Matrix * get() {
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");

  pthread_mutex_lock(&bounded_buffer_mutex);
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
  assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

  if (bounded_buffer_readable == 0) {
    // nothing to read, wait until a slot fills.
    pthread_cond_wait(&bounded_buffer_get_cond, &bounded_buffer_mutex);

    if (bounded_buffer_readable == 0) goto error;
  }
  assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exist");
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
error:
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return NULL;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg) {
  DEBUG("Starting producer");
  counter_t *prod_count = arg;

  while (get_cnt(prod_count) < NUMBER_OF_MATRICES) {
    increment_cnt(prod_count);
    Matrix *matrix = GenMatrixRandom();
    assert(matrix != NULL && "generated matrix musn't be NULL");
    int ret;
    while (get_cnt(prod_count) <= NUMBER_OF_MATRICES && (ret = put(matrix)) == -1);
    if (ret == -1) return NULL;

    DEBUG("prod cnt: %d", get_cnt(prod_count));
  }

  return NULL;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg) {
  DEBUG("Starting consumer");
  counter_t *cons_count = arg;

  while (get_cnt(cons_count) < NUMBER_OF_MATRICES) {

    Matrix *mat;
    while (get_cnt(cons_count) < NUMBER_OF_MATRICES && (mat = get()) == NULL);
    increment_cnt(cons_count);
    if (mat == NULL) return NULL;

    FreeMatrix(mat);

    DEBUG("cons cnt: %d", get_cnt(cons_count));
  }

  return NULL;
}
