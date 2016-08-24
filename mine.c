#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

static inline void swap(void *a, void *b, size_t size) {
  // unaligned accesses on x86_64 are cheap
  uint8_t u8; uint16_t u16; uint32_t u32; uint64_t u64;
  switch (size) {
    case 1:  u8 = *( uint8_t*)a; *( uint8_t*)a = *( uint8_t*)b; *( uint8_t*)b =  u8; return;
    case 2: u16 = *(uint16_t*)a; *(uint16_t*)a = *(uint16_t*)b; *(uint16_t*)b = u16; return;
    case 4: u32 = *(uint32_t*)a; *(uint32_t*)a = *(uint32_t*)b; *(uint32_t*)b = u32; return;
    case 8: u64 = *(uint64_t*)a; *(uint64_t*)a = *(uint64_t*)b; *(uint64_t*)b = u64; return;
  }
#define min(a, b) ((a) < (b) ? (a) : (b))
  for (char tmp[256]; size; size -= min(size, sizeof(tmp))) {
    memcpy(tmp, a, min(size, sizeof(tmp)));
    memcpy(a,   b, min(size, sizeof(tmp)));
    memcpy(b, tmp, min(size, sizeof(tmp)));
  }
#undef min
}

// macros make the code more natural to read (imo)
#define swap(a, b) swap(a, b, size)
#define cmp(a, b) (f1 ? f1(&a, &b) : f2(&a, &b, arg))

static void sift_down(void *base, size_t size, size_t top, size_t bottom,
    int (*f1)(const void *, const void *),
    int (*f2)(const void *, const void *, void *),
    void *arg) {

  typedef char type[size];
  type *array = base;

  for (size_t child; top * 2 + 1 <= bottom; top = child) {
    child = top * 2 + 1;
    if (child + 1 <= bottom && cmp(array[child], array[child+1]) < 0) child++;
    if (cmp(array[child], array[top]) < 0) return;
    swap(&array[child], &array[top]);
  }
}

#define SWAP(a, b) do { if (cmp(array[a], array[b]) > 0) swap(&array[a], &array[b]); } while (0)
#define sort2(a, b)       do { SWAP(a, b);                                                 } while (0)
#define sort3(a, b, c)    do { SWAP(b, c); SWAP(a, c); SWAP(a, b);                         } while (0)
#define sort4(a, b, c, d) do { SWAP(a, b); SWAP(c, d); SWAP(a, c); SWAP(b, d); SWAP(b, c); } while (0)

static void actual_qsort(void *base, size_t nmemb, size_t size, size_t recur,
    int (*f1)(const void *, const void *),
    int (*f2)(const void *, const void *, void *),
    void *arg) {

  typedef char type[size];
  type *array = base, *pivot;
  size_t i, j;

  // introsort
  while (nmemb > 10) {
    // switch to heap sort if recursion is too deep
    if (!recur) {
#define sift_down(arr, top, bottom) sift_down(arr, size, top, bottom, f1, f2, arg)
      for (ssize_t top = (nmemb-2) / 2; top >= 0; top--) // signed
        sift_down(array, top, nmemb-1);

      for (size_t i = nmemb-1; i > 0; sift_down(array, 0, --i))
        swap(&array[0], &array[i]);
#undef sift_down
      return;
    }

    // recursive quicksort (todo: 3 way)
    sort3(0, nmemb-1, nmemb/2); // puts middle in array[nmemb-1]
    pivot = &array[nmemb-1];

    // hoare partition
    for (i = 0, j = nmemb - 1; ; i++, j--) {
      while (cmp(array[i], *pivot) < 0) i++;
      while (cmp(array[j], *pivot) > 0) j--;
      if (i >= j) break;
      swap(&array[i], &array[j]);
    }

    // recursion in the smallest partition => O(log n) space
    if (i > size / 2) {
      actual_qsort(array+i, nmemb-i, size, --recur, f1, f2, arg);
      nmemb = i;
    }
    else {
      actual_qsort(array, i, size, --recur, f1, f2, arg);
      array += i;
      nmemb -= i;
    }
  }

  // final pass with small arrays
  switch (nmemb) {
    case 0: case 1:            return;
    case 2: sort2(0, 1      ); return;
    case 3: sort3(0, 1, 2   ); return;
    case 4: sort4(0, 1, 2, 3); return;
  }
  // insertion sort up to 10
  for (size_t i = 1; i < nmemb; i++)
    for (size_t j = i; j > 0 && cmp(array[j-1], array[j]) > 0; j--)
      swap(&array[j-1], &array[j]);
}

#undef swap
#undef cmp

void my_qsort(void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *)) {
  size_t depth = nmemb, log2 = 0;
  while (depth >>= 1) log2++;
  actual_qsort(base, nmemb, size, 2*log2, compar, NULL, NULL);
}







#ifdef test


static int cmp(const void *n1, const void *n2) {
  int *a = (int *)n1, *b = (int *)n2;
  return *a - *b;
}

#include <stdlib.h>
int main() {
  /*srand(time(0));*/
  int arr[] = { 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1 }, nelem = 12;
  /*for (int i = 0; i < nelem; i++) arr[i] = rand() % 1000;*/
  my_qsort(arr, nelem, sizeof(int), cmp);
  for (int i = 0; i < nelem; i++) printf("%d ", arr[i]);
  putchar('\n');
  return 0;
}
#endif
