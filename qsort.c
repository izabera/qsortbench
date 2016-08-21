#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



void bsd_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void diet_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void illumos_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void klibc_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void musl_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void my_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void plan9_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void uclibc_qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));







static int cmp(const void *n1, const void *n2) {
  uint32_t *a = (uint32_t *)n1, *b = (uint32_t *)n2;
  return (*a > *b) - (*a < *b);
}

#include <sys/time.h>

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng) {
  uint64_t oldstate = rng->state;
  rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

pcg32_random_t start = { 1234567890987654321, 1234567890987654321 };

void random_elements(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums; i++) array[i] = pcg32_random_r(&r);
}

void few_unique(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums; i++) array[i] = pcg32_random_r(&r) % (nums / 100 + 1);
}

void sorted(uint32_t *array, size_t nums) {
  for (size_t i = 0; i < nums; i++) array[i] = i;
}

void reverse(uint32_t *array, size_t nums) {
  for (size_t i = 0; i < nums; i++) array[i] = nums - i;
}

void one_percent(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums; i++) array[i] = i;
  for (size_t i = 0; i < nums / 100; i++) array[pcg32_random_r(&r)%nums] = pcg32_random_r(&r);
}

void one_percent_nsz(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums; i++) array[i] = i;
  for (size_t i = 0; i < nums / 200; i++) {
    uint32_t idx1 = pcg32_random_r(&r) % nums, idx2 = pcg32_random_r(&r) % nums, tmp;
    tmp = array[idx1];
    array[idx1] = array[idx2];
    array[idx2] = tmp;
  }
}

void fifty_percent(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums/2; i++) array[2*i] = i, array[2*i+1] = pcg32_random_r(&r);
}

void triangle(uint32_t *array, size_t nums) {
  for (size_t i = 0; i < nums; i++) array[i] = i % ((nums % 100)+1);
}

void allequal(uint32_t *array, size_t nums) {
  for (size_t i = 0; i < nums; i++) array[i] = 123456789;
}

struct score {
  int idx;
  double time;
};

int scoresort(const void *elem1, const void *elem2) {
  struct score *a = (struct score *)elem1, *b = (struct score *)elem2;
  return (a->time > b->time) - (a->time < b->time);
}

typedef void (*sorttype)(void *, size_t, size_t, int (*)(const void *, const void *));
struct qs {
  sorttype q;
  char *name;
  int final_score;
  double total_time;
};

int leadersort(const void *elem1, const void *elem2) {
  struct qs *a = (struct qs *)elem1, *b = (struct qs *)elem2;
  return a->final_score - b->final_score;
}

int main(int argc, char *argv[]) {
  uint32_t nums = argc > 1 ? strtoul(argv[1], NULL, 10) : 10000000;
  uint32_t *array = malloc(nums * sizeof(uint32_t));
  for (size_t i = 0; i < nums; i++) array[i] = 7;
  struct timeval t1, t2;

  typedef void (*shuftype)(uint32_t *, size_t);

  struct qs qsorts[] = {
    {         qsort, "glibc"  , 0, 0 },
    {    musl_qsort, "musl"   , 0, 0 },
    {    diet_qsort, "diet"   , 0, 0 },
    {     bsd_qsort, "bsd"    , 0, 0 },
    {  uclibc_qsort, "uclibc" , 0, 0 },
    {   plan9_qsort, "plan9"  , 0, 0 },
    { illumos_qsort, "illumos", 0, 0 },
    /*{   klibc_qsort, "klibc"  , 0, 0 },*/  // quadratic time on 50% sorted input
    {      my_qsort, "mine"   , 0, 0 },
  };

  struct {
    shuftype s;
    char *desc;
  } shufs[] = {
    { random_elements, "random elements" },
    {      few_unique, "few unique"      },
    {          sorted, "sorted"          },
    {         reverse, "reverse"         },
    {     one_percent, "1% ouf of order" },
    { one_percent_nsz, "1% the nsz way"  },
    {   fifty_percent, "50% sorted"      },
    {        triangle, "triangle"        },
    {        allequal, "all equal"       },
  };


  struct score scores[sizeof(qsorts)/sizeof(qsorts[0])];
  for (size_t i = 0; i < sizeof(shufs)/sizeof(shufs[0]); i++) {
    puts(shufs[i].desc);
    for (size_t j = 0; j < sizeof(qsorts)/sizeof(qsorts[0]); j++) {
      shufs[i].s(array, nums);
      gettimeofday(&t1, NULL);
      qsorts[j].q(array, nums, sizeof(int), cmp);
      gettimeofday(&t2, NULL);
      scores[j].time = (double) (t2.tv_sec  - t1.tv_sec) + (double) (t2.tv_usec - t1.tv_usec) / 1000000;
      scores[j].idx = j;
      printf("%10s %12.6f\n", qsorts[j].name, scores[j].time);
      qsorts[scores[j].idx].total_time += scores[j].time;
    }
    qsort(scores, sizeof(scores)/sizeof(scores[0]), sizeof(scores[0]), scoresort);
    for (size_t j = 0; j < sizeof(qsorts)/sizeof(qsorts[0]); j++) {
      printf("%zu. %s ", j+1, qsorts[scores[j].idx].name);
      qsorts[scores[j].idx].final_score += j;
    }
    puts("\n----");
  }

  puts("leaderboard");
  qsort(qsorts, sizeof(qsorts)/sizeof(qsorts[0]), sizeof(qsorts[0]), leadersort);
  for (size_t j = 0; j < sizeof(qsorts)/sizeof(qsorts[0]); j++)
    printf("%zu. %-10s (total: %12.6f)\n", j+1, qsorts[j].name, qsorts[j].total_time);

  return 0;
}
