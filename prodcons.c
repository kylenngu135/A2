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
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "counter.h"
#include "matrix.h"
#include "pcmatrix.h"
#include "prodcons.h"

// Define Locks, Condition variables, and so on here
/// Locks stdout so only one prints at a time.
pthread_mutex_t stdout_lock = PTHREAD_MUTEX_INITIALIZER;

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

    while (bounded_buffer_readable == (size_t) BOUNDED_BUFFER_SIZE) {
      // no space to write, wait until a space opens up.
      pthread_cond_wait(&bounded_buffer_put_cond, &bounded_buffer_mutex);
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
}

/// Thread safe
/// Caller takes ownership of return, and it just be freed.
/// returns NULL if it failed to reserve a slot
Matrix * get() {
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");

  pthread_mutex_lock(&bounded_buffer_mutex);
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

    while (bounded_buffer_readable == 0) {
      // nothing to read, wait until a slot fills.
      pthread_cond_wait(&bounded_buffer_get_cond, &bounded_buffer_mutex);
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
}

/// do an atomic RMW iff the condition passes
bool claim(counter_t *cnt) {
  pthread_mutex_lock(&cnt->lock);
  bool ret = cnt->value < NUMBER_OF_MATRICES;
  if (ret) cnt->value += 1;
  pthread_mutex_unlock(&cnt->lock);

  return ret;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg) {
  counter_t *prod_count = arg;
  ProdConsStats *prodcons = calloc(1, sizeof(ProdConsStats));
  if (prodcons == NULL) return NULL;

  while (claim(prod_count)) {
    prodcons->matrixtotal += 1;
    Matrix *matrix = GenMatrixRandom();
    assert(matrix != NULL && "generated matrix musn't be NULL");
    assert(matrix->m != NULL && "generated matrix's elements cannot be NULL");

    prodcons->sumtotal += SumMatrix(matrix);

    put(matrix);
  }

  return prodcons;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg) {
  Matrix *mult = NULL, *lhs = NULL, *rhs = NULL;
  counter_t *cons_count = arg;
  ProdConsStats *prodcons = calloc(1, sizeof(ProdConsStats));
  if (prodcons == NULL) return NULL;

  while (claim(cons_count)) {
    if ((lhs = get()) == NULL) return prodcons;
    prodcons->matrixtotal += 1;
    prodcons->sumtotal += SumMatrix(lhs);

    while (claim(cons_count)) {
      if ((rhs = get()) == NULL) {
        FreeMatrix(lhs);
        return prodcons;
      };
      prodcons->matrixtotal += 1;
      prodcons->sumtotal += SumMatrix(rhs);

      assert(lhs != NULL);
      assert(rhs != NULL);

      pthread_mutex_lock(&stdout_lock);
        // I wish `MatrixMultiply` didn't print, because having this
        // behind the lock removes 90% of the point of having multiple
        // consumers
        if ((mult = MatrixMultiply(lhs, rhs)) != NULL) {
          DisplayMatrix(lhs, stdout);
          printf("    X\n");
          DisplayMatrix(rhs, stdout);
          printf("    =\n");
          DisplayMatrix(mult, stdout);
        };
      pthread_mutex_unlock(&stdout_lock);

      if (mult != NULL) goto finish;

      FreeMatrix(rhs);
    }
    FreeMatrix(lhs);
    return prodcons;

  finish:
    prodcons->multtotal += 1;

    FreeMatrix(lhs);
    FreeMatrix(rhs);
    FreeMatrix(mult);
  }

ret:
  return prodcons;
}
