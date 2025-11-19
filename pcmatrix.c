/*
 *  pcmatrix module
 *  Primary module providing control flow for the pcMatrix program
 *
 *  Producer consumer bounded buffer program to produce random matrices in parallel
 *  and consume them while searching for valid pairs for matrix multiplication.
 *  Matrix multiplication requires the first matrix column count equal the
 *  second matrix row count.
 *
 *  A matrix is consumed from the bounded buffer.  Then matrices are consumed
 *  from the bounded buffer, ONE AT A TIME, until an eligible matrix for multiplication
 *  is found.
 *
 *  Totals are tracked using the ProdConsStats Struct for each thread separately:
 *  - the total number of matrices multiplied (multtotal from each consumer thread)
 *  - the total number of matrices produced (matrixtotal from each producer thread)
 *  - the total number of matrices consumed (matrixtotal from each consumer thread)
 *  - the sum of all elements of all matrices produced and consumed (sumtotal from each producer and consumer thread)
 *
 *  Then, these values from each thread are aggregated in main thread for output
 *
 *  Correct programs will produce and consume the same number of matrices, and
 *  report the same sum for all matrix elements produced and consumed.
 *
 *  Each thread produces a total sum of the value of
 *  randomly generated elements.  Producer sum and consumer sum must match.
 *
 *  University of Washington, Tacoma
 *  TCSS 422 - Operating Systems
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include "matrix.h"
#include "counter.h"
#include "prodcons.h"
#include "pcmatrix.h"

int main (int argc, char *argv[]) {
  // Process command line arguments
  int numw = NUMWORK;
  BOUNDED_BUFFER_SIZE = MAX;
  NUMBER_OF_MATRICES = LOOPS;
  MATRIX_MODE = DEFAULT_MATRIX_MODE;

  if (argc == 1) {
    printf("USING DEFAULTS: worker_threads=%d bounded_buffer_size=%d matricies=%d matrix_mode=%d\n", numw, BOUNDED_BUFFER_SIZE, NUMBER_OF_MATRICES, MATRIX_MODE);
  } else {
    if (argc >= 2) numw=atoi(argv[1]);
    if (argc >= 3) BOUNDED_BUFFER_SIZE=atoi(argv[2]);
    if (argc >= 4) NUMBER_OF_MATRICES=atoi(argv[3]);
    if (argc >= 5) MATRIX_MODE=atoi(argv[4]);
    printf("USING: worker_threads=%d bounded_buffer_size=%d matricies=%d matrix_mode=%d\n", numw, BOUNDED_BUFFER_SIZE, NUMBER_OF_MATRICES, MATRIX_MODE);
  }

  // Seed the random number generator with the system time
  srand((unsigned) time(NULL)); // the time arg should be NULL by man page

  printf("Producing %d matrices in mode %d.\n", NUMBER_OF_MATRICES, MATRIX_MODE);
  printf("Using a shared buffer of size=%d\n", BOUNDED_BUFFER_SIZE);
  printf("With %d producer and consumer thread(s).\n", numw);
  printf("\n");

  bigmatrix = calloc(BOUNDED_BUFFER_SIZE, sizeof(Matrix *));
  // no free, lives to end of program
  if (bigmatrix == NULL) {
    perror("pcmatrix: calloc");
    return 1;
  }

  pthread_t *pr = calloc(numw, sizeof(pthread_t));
  // no free, lives to end of program
  if (pr == NULL) {
    perror("pcmatrix: calloc");
    return 1;
  }
  pthread_t *co = calloc(numw, sizeof(pthread_t));
  // no free, lives to end of program
  if (co == NULL) {
    perror("pcmatrix: calloc");
    return 1;
  }

  counter_t producer_counter, consumer_counter;

  DEBUG("making counters");
  init_cnt(&producer_counter);
  init_cnt(&consumer_counter);

  for (int worker = 0; worker < numw; worker++) {
    DEBUG("creating worker group %d", worker);
    // Add your code here to create threads and so on
    pthread_create(&pr[worker], NULL, prod_worker, &producer_counter);
    pthread_create(&co[worker], NULL, cons_worker, &consumer_counter);
  }

  for (int worker = 0; worker < numw; worker++) {
    DEBUG("joining producer %d", worker);
    pthread_join(pr[worker], NULL);
    DEBUG("joining consumer %d", worker);
    pthread_join(co[worker], NULL);
  }

  // These are used to aggregate total numbers for main thread output
  int prs = producer_counter.value; // total #matrices produced
  int cos = consumer_counter.value; // total #matrices consumed
  int prodtot = 0; // total sum of elements for matrices produced
  int constot = 0; // total sum of elements for matrices consumed
  int consmul = 0; // total # multiplications

  // consume ProdConsStats from producer and consumer threads [HINT: return from join]
  // add up total matrix stats in prs, cos, prodtot, constot, consmul

  printf("Sum of Matrix elements --> Produced=%d = Consumed=%d\n", prodtot, constot);
  printf("Matrices produced=%d consumed=%d multiplied=%d\n", prs, cos, consmul);

  for (int i = 0; i < BOUNDED_BUFFER_SIZE; i++) assert(bigmatrix[i] == NULL);

  return 0;
}
