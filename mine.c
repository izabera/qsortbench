#include <string.h>
#include <stdint.h>
#include <sys/types.h>

static inline void swap(void *a, void *b, size_t size) {
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
  type *array = (void *)base;

  for (size_t child; top * 2 + 1 <= bottom; top = child) {
    child = top * 2 + 1;
    if (child + 1 <= bottom && cmp(array[child], array[child+1]) < 0) child++;
    if (cmp(array[child], array[top]) < 0) return;
    swap(&array[child], &array[top]);
  }
}

static void actual_qsort(void *base, size_t nmemb, size_t size,
    int (*f1)(const void *, const void *),
    int (*f2)(const void *, const void *, void *),
    void *arg) {

  typedef char type[size];
  type *array = (void *)base;

  // optimize a few cases
  if (nmemb < 2) return;
  if (nmemb == 2) {
    if (cmp(array[0], array[1]) < 0) swap(&array[0], &array[1]);
    return;
  }
  if (nmemb < 10) { // insertion sort
    for (size_t i = 1; i < nmemb; i++)
      for (size_t j = i; j > 0 && cmp(array[j-1], array[j]) > 0; j--)
        swap(&array[j-1], &array[j]);
    return;
  }

#define sift_down(arr, top, bottom) sift_down(arr, size, top, bottom, f1, f2, arg)

  // heap sort
  for (ssize_t top = (nmemb-2) / 2; top >= 0; top--) // signed
    sift_down(array, top, nmemb-1);

  for (size_t i = nmemb-1; i > 0; sift_down(array, 0, --i))
    swap(&array[0], &array[i]);

#undef sift_down
}

#undef swap
#undef cmp

void my_qsort(void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *)) {
  actual_qsort(base, nmemb, size, compar, NULL, NULL);
}
