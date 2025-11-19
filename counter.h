/*
 *  counter header
 *  Function prototypes, data, and constants for synchronized counter module
 *
 *  University of Washington, Tacoma
 *  TCSS 422 - Operating Systems
 */

// SYNCHRONIZED COUNTER

// counter structures
typedef struct __counter_t {
  int value;
  pthread_mutex_t  lock;
} counter_t;

typedef struct __counters_t {
  counter_t *prod;
  counter_t *cons;
} counters_t;

// counter methods
void init_cnt(counter_t *c);
void increment_cnt(counter_t *c);
/// increments the counter by `n` iff it won't go past `limit` (can equal)
/// returns 1 if increments, 0 if it doesn't
int claim_cnt(counter_t *c, int limit, int n);
int get_cnt(counter_t *c);
