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
  
  // assert the matrix isn't null
  assert(value != NULL);
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");

  // lock
  pthread_mutex_lock(&bounded_buffer_mutex);

    // assert that readable and index sizes for valid sizes
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

    while (bounded_buffer_readable == (size_t) BOUNDED_BUFFER_SIZE) {
      // no space to write, wait until a space opens up.
      pthread_cond_wait(&bounded_buffer_put_cond, &bounded_buffer_mutex);
    }

    // assert that readable and index sizes for valid sizes
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "cannot have more readable than slots exist and it musn't be full");
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have an extra slot open for the read");
    
    // assert than an overridden entry must be cleared
    assert(bigmatrix[bounded_buffer_write_idx] == NULL && "overridden entry must have been cleared");

    // insert into bounded buffer
    bigmatrix[bounded_buffer_write_idx] = value;

    // increment readable
    bounded_buffer_readable += 1;

    // assert that readable size is valid
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than space available");

    // increment buffer index
    bounded_buffer_write_idx = (bounded_buffer_write_idx + 1) % (size_t) BOUNDED_BUFFER_SIZE;

    // assert index size is valid
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "new index must be within buffer");

    // signal to stop wait
    pthread_cond_signal(&bounded_buffer_get_cond);

  // unlock
  pthread_mutex_unlock(&bounded_buffer_mutex);
  return 0;
}

/// Thread safe
/// Caller takes ownership of return, and it just be freed.
/// returns NULL if it failed to reserve a slot
Matrix * get() {
  // asserts that buffer size is greater than 0
  assert(BOUNDED_BUFFER_SIZE > 0 && "Buffer must be a valid size");

  // lock
  pthread_mutex_lock(&bounded_buffer_mutex);

    // assert that readable and index are valid
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exists");
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "write_idx must be within the buffer");

    while (bounded_buffer_readable == 0) {
      // nothing to read, wait until a slot fills.
      pthread_cond_wait(&bounded_buffer_get_cond, &bounded_buffer_mutex);
    }

    // assert that readable and index are valid
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE && "cannot have more readable than slots exist");
    assert(bounded_buffer_write_idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have an extra slot open for the read");

    // get index 
    size_t idx = (bounded_buffer_write_idx >= bounded_buffer_readable)
      ? bounded_buffer_write_idx - bounded_buffer_readable
      : (size_t) BOUNDED_BUFFER_SIZE - (bounded_buffer_readable - bounded_buffer_write_idx);

    // assert that index is within the buffer size
    assert(idx <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "retreival index just be be within the buffer");

    // assert that the entry is filled
    assert(bigmatrix[idx] != NULL && "Entry read be filled (i.e. not NULL)");

    // get the value from the matrix
    Matrix *value = bigmatrix[idx];

    // clear position we just took
    bigmatrix[idx] = NULL; 

    // decrement readable
    bounded_buffer_readable -= 1;

    // assert readable is valid
    assert(bounded_buffer_readable <= (size_t) BOUNDED_BUFFER_SIZE - 1 && "must have less entries than buffer size (-1 since we just took one)");
  
    // signal
    pthread_cond_signal(&bounded_buffer_put_cond);

  // unlock
  pthread_mutex_unlock(&bounded_buffer_mutex);

  // return matrix
  return value;
}

// Matrix PRODUCER worker thread
void *prod_worker(void *arg) {
  // initialize prod_count
  counter_t *prod_count = arg;

  // initialize prodcon
  ProdConsStats *prodcons = calloc(1, sizeof(ProdConsStats));
  if (prodcons == NULL) return NULL;

  // 
  while (claim_cnt(prod_count, NUMBER_OF_MATRICES, 1)) {

    // increment matrixtotal
    prodcons->matrixtotal += 1;

    // generate random matrix
    Matrix *matrix = GenMatrixRandom();

    // assert that matrix can't be null
    assert(matrix != NULL && "generated matrix musn't be NULL");
    assert(matrix->m != NULL && "generated matrix's elements cannot be NULL");

    // add sumMatrix to sumtotal
    prodcons->sumtotal += SumMatrix(matrix);
    
    // put matrix in bounded buffer
    put(matrix);
  }

  // return prodcons
  return prodcons;
}

// Matrix CONSUMER worker thread
void *cons_worker(void *arg) {

  // initialize matrices, lhs, and rhs
  Matrix *mult = NULL, *lhs = NULL, *rhs = NULL;

  // initialize cons_count
  counter_t *cons_count = arg;

  // allocate to prodcons
  ProdConsStats *prodcons = calloc(1, sizeof(ProdConsStats));

  // return null if we failed to allocate
  if (prodcons == NULL) return NULL;

  // claim matrices
  while (claim_cnt(cons_count, NUMBER_OF_MATRICES, 1)) {

    // get lhs, returns if nothing in bounded buffer
    if ((lhs = get()) == NULL) return prodcons;

    // increment matrix and sumMatrix
    prodcons->matrixtotal += 1;
    prodcons->sumtotal += SumMatrix(lhs);

    // get 2nd matrix
    while (claim_cnt(cons_count, NUMBER_OF_MATRICES, 1)) {
      
      // get rhs, returns if nothing in bounded buffer
      if ((rhs = get()) == NULL) {
        // frees lhs
        FreeMatrix(lhs);
        return prodcons;
      };

      // increment matrix total and sumtotal
      prodcons->matrixtotal += 1;
      prodcons->sumtotal += SumMatrix(rhs);

      // asserts lhs and rhs aren't null
      assert(lhs != NULL);
      assert(rhs != NULL);

      // lock
      pthread_mutex_lock(&stdout_lock);
        // multiply matrices, prints if not null
        if ((mult = MatrixMultiply(lhs, rhs)) != NULL) {
          DisplayMatrix(lhs, stdout);
          printf("    X\n");
          DisplayMatrix(rhs, stdout);
          printf("    =\n");
          DisplayMatrix(mult, stdout);
        };

      // unlock
      pthread_mutex_unlock(&stdout_lock);

      // if mult wasn't null goto finish
      if (mult != NULL) goto finish;

      // free rhs if mult was null
      FreeMatrix(rhs);
    }

    // free lhs if mult was null
    FreeMatrix(lhs);
    return prodcons;

  finish:
    // increment multtotal
    prodcons->multtotal += 1;

    // free lhs, rhs, and mult
    FreeMatrix(lhs);
    FreeMatrix(rhs);
    FreeMatrix(mult);
  }

  return prodcons;
}
