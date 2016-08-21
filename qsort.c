#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* mine */

static void swap(void *a, void *b, size_t size) {
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




/* musl */

static inline int a_ctz_64(uint64_t x)
{
	__asm__( "bsf %1,%0" : "=r"(x) : "r"(x) );
	return x;
}

static inline int a_ctz_l(unsigned long x)
{
	static const char debruijn32[32] = {
		0, 1, 23, 2, 29, 24, 19, 3, 30, 27, 25, 11, 20, 8, 4, 13,
		31, 22, 28, 18, 26, 10, 7, 12, 21, 17, 9, 6, 16, 5, 15, 14
	};
	if (sizeof(long) == 8) return a_ctz_64(x);
	return debruijn32[(x&-x)*0x076be629 >> 27];
}

// this is src/stdlib/qsort.c

#define ntz(x) a_ctz_l((x))

typedef int (*cmpfun)(const void *, const void *);

static inline int pntz(size_t p[2]) {
	int r = ntz(p[0] - 1);
	if(r != 0 || (r = 8*sizeof(size_t) + ntz(p[1])) != 8*sizeof(size_t)) {
		return r;
	}
	return 0;
}

static void cycle(size_t width, unsigned char* ar[], int n)
{
	unsigned char tmp[256];
	size_t l;
	int i;

	if(n < 2) {
		return;
	}

	ar[n] = tmp;
	while(width) {
		l = sizeof(tmp) < width ? sizeof(tmp) : width;
		memcpy(ar[n], ar[0], l);
		for(i = 0; i < n; i++) {
			memcpy(ar[i], ar[i + 1], l);
			ar[i] += l;
		}
		width -= l;
	}
}

/* shl() and shr() need n > 0 */
static inline void shl(size_t p[2], int n)
{
	if(n >= 8 * sizeof(size_t)) {
		n -= 8 * sizeof(size_t);
		p[1] = p[0];
		p[0] = 0;
	}
	p[1] <<= n;
	p[1] |= p[0] >> (sizeof(size_t) * 8 - n);
	p[0] <<= n;
}

static inline void shr(size_t p[2], int n)
{
	if(n >= 8 * sizeof(size_t)) {
		n -= 8 * sizeof(size_t);
		p[0] = p[1];
		p[1] = 0;
	}
	p[0] >>= n;
	p[0] |= p[1] << (sizeof(size_t) * 8 - n);
	p[1] >>= n;
}

static void sift(unsigned char *head, size_t width, cmpfun cmp, int pshift, size_t lp[])
{
	unsigned char *rt, *lf;
	unsigned char *ar[14 * sizeof(size_t) + 1];
	int i = 1;

	ar[0] = head;
	while(pshift > 1) {
		rt = head - width;
		lf = head - width - lp[pshift - 2];

		if((*cmp)(ar[0], lf) >= 0 && (*cmp)(ar[0], rt) >= 0) {
			break;
		}
		if((*cmp)(lf, rt) >= 0) {
			ar[i++] = lf;
			head = lf;
			pshift -= 1;
		} else {
			ar[i++] = rt;
			head = rt;
			pshift -= 2;
		}
	}
	cycle(width, ar, i);
}

static void trinkle(unsigned char *head, size_t width, cmpfun cmp, size_t pp[2], int pshift, int trusty, size_t lp[])
{
	unsigned char *stepson,
	              *rt, *lf;
	size_t p[2];
	unsigned char *ar[14 * sizeof(size_t) + 1];
	int i = 1;
	int trail;

	p[0] = pp[0];
	p[1] = pp[1];

	ar[0] = head;
	while(p[0] != 1 || p[1] != 0) {
		stepson = head - lp[pshift];
		if((*cmp)(stepson, ar[0]) <= 0) {
			break;
		}
		if(!trusty && pshift > 1) {
			rt = head - width;
			lf = head - width - lp[pshift - 2];
			if((*cmp)(rt, stepson) >= 0 || (*cmp)(lf, stepson) >= 0) {
				break;
			}
		}

		ar[i++] = stepson;
		head = stepson;
		trail = pntz(p);
		shr(p, trail);
		pshift += trail;
		trusty = 0;
	}
	if(!trusty) {
		cycle(width, ar, i);
		sift(head, width, cmp, pshift, lp);
	}
}

void musl_qsort(void *base, size_t nel, size_t width, cmpfun cmp)
{
	size_t lp[12*sizeof(size_t)];
	size_t i, size = width * nel;
	unsigned char *head, *high;
	size_t p[2] = {1, 0};
	int pshift = 1;
	int trail;

	if (!size) return;

	head = base;
	high = head + size - width;

	/* Precompute Leonardo numbers, scaled by element width */
	for(lp[0]=lp[1]=width, i=2; (lp[i]=lp[i-2]+lp[i-1]+width) < size; i++);

	while(head < high) {
		if((p[0] & 3) == 3) {
			sift(head, width, cmp, pshift, lp);
			shr(p, 2);
			pshift += 2;
		} else {
			if(lp[pshift - 1] >= high - head) {
				trinkle(head, width, cmp, p, pshift, 0, lp);
			} else {
				sift(head, width, cmp, pshift, lp);
			}
			
			if(pshift == 1) {
				shl(p, 1);
				pshift = 0;
			} else {
				shl(p, pshift - 1);
				pshift = 1;
			}
		}
		
		p[0] |= 1;
		head += width;
	}

	trinkle(head, width, cmp, p, pshift, 0, lp);

	while(pshift != 1 || p[0] != 1 || p[1] != 0) {
		if(pshift <= 1) {
			trail = pntz(p);
			shr(p, trail);
			pshift += trail;
		} else {
			shl(p, 2);
			pshift -= 2;
			p[0] ^= 7;
			shr(p, 1);
			trinkle(head - lp[pshift] - width, width, cmp, p, pshift + 1, 1, lp);
			shl(p, 1);
			p[0] |= 1;
			trinkle(head - width, width, cmp, p, pshift, 1, lp);
		}
		head -= width;
	}
}








/* diet libc */

static void exch(char* base,size_t size,size_t a,size_t b) {
  char* x=base+a*size;
  char* y=base+b*size;
  while (size) {
    char z=*x;
    *x=*y;
    *y=z;
    --size; ++x; ++y;
  }
}

#define RAND

/* Quicksort with 3-way partitioning, ala Sedgewick */
/* Blame him for the scary variable names */
/* http://www.cs.princeton.edu/~rs/talks/QuicksortIsOptimal.pdf */
static void quicksort(char* base,size_t size,ssize_t l,ssize_t r,
		      int (*compar)(const void*,const void*)) {
  ssize_t i=l-1, j=r, p=l-1, q=r, k;
  char* v=base+r*size;
  if (r<=l) return;

#ifdef RAND
  /*
     We chose the rightmost element in the array to be sorted as pivot,
     which is OK if the data is random, but which is horrible if the
     data is already sorted.  Try to improve by exchanging it with a
     random other pivot.
   */
  exch(base,size,l+(rand()%(r-l)),r);
#elif defined MID
  /*
     We chose the rightmost element in the array to be sorted as pivot,
     which is OK if the data is random, but which is horrible if the
     data is already sorted.  Try to improve by chosing the middle
     element instead.
   */
  exch(base,size,l+(r-l)/2,r);
#endif

  for (;;) {
    while (++i != r && compar(base+i*size,v)<0) ;
    while (compar(v,base+(--j)*size)<0) if (j == l) break;
    if (i >= j) break;
    exch(base,size,i,j);
    if (compar(base+i*size,v)==0) exch(base,size,++p,i);
    if (compar(v,base+j*size)==0) exch(base,size,j,--q);
  }
  exch(base,size,i,r); j = i-1; ++i;
  for (k=l; k<p; k++, j--) exch(base,size,k,j);
  for (k=r-1; k>q; k--, i++) exch(base,size,i,k);
  quicksort(base,size,l,j,compar);
  quicksort(base,size,i,r,compar);
}

void diet_qsort(void* base,size_t nmemb,size_t size,int (*compar)(const void*,const void*)) {
  /* check for integer overflows */
  if (nmemb >= (((size_t)-1)>>1) ||
      size >= (((size_t)-1)>>1)) return;
#if 0
  if (sizeof(size_t) < sizeof(unsigned long long)) {
    if ((unsigned long long)size * nmemb > (size_t)-1) return;
  } else {
    if (size*nmemb/nmemb != size) return;
  }
#endif
  if (nmemb>1)
    quicksort(base,size,0,nmemb-1,compar);
}





/* bsd */

static __inline char	*med3(char *, char *, char *, int (*)(const void *, const void *));
static __inline void	 swapfunc(char *, char *, size_t, int);

#define min(a, b)	(a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	size_t i = (n) / sizeof (TYPE); 		\
	TYPE *pi = (TYPE *) (parmi); 			\
	TYPE *pj = (TYPE *) (parmj); 			\
	do { 						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static __inline void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
	if (swaptype <= 1) 
		swapcode(long, a, b, n)
	else
		swapcode(char, a, b, n)
}

#define swap(a, b)					\
	if (swaptype == 0) {				\
		long t = *(long *)(a);			\
		*(long *)(a) = *(long *)(b);		\
		*(long *)(b) = t;			\
	} else						\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n) 	if ((n) > 0) swapfunc(a, b, n, swaptype)

static __inline char *
med3(char *a, char *b, char *c, int (*cmp)(const void *, const void *))
{
	return cmp(a, b) < 0 ?
	       (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
              :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

void
bsd_qsort(void *aa, size_t n, size_t es, int (*cmp)(const void *, const void *))
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int cmp_result, swaptype;
	size_t d, r;
	char *a = aa;

loop:	SWAPINIT(a, es);
	if (n < 7) {
		for (pm = (char *)a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *)a + (n / 2) * es;
	if (n > 7) {
		pl = (char *)a;
		pn = (char *)a + (n - 1) * es;
		if (n > 40) {
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp);
			pm = med3(pm - d, pm, pm + d, cmp);
			pn = med3(pn - 2 * d, pn - d, pn, cmp);
		}
		pm = med3(pl, pm, pn, cmp);
	}
	swap(a, pm);
	pa = pb = (char *)a + es;

	pc = pd = (char *)a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (cmp_result = cmp(pb, a)) <= 0) {
			if (cmp_result == 0) {
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (cmp_result = cmp(pc, a)) >= 0) {
			if (cmp_result == 0) {
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
		}
		if (pb > pc)
			break;
		swap(pb, pc);
		pb += es;
		pc -= es;
	}

	pn = (char *)a + n * es;
	r = min(pa - (char *)a, pb - pa);
	vecswap(a, pb - r, r);
	r = min(pd - pc, pn - pd - es);
	vecswap(pb, pn - r, r);
	if ((r = pb - pa) > es)
		qsort(a, r / es, es, cmp);
	if ((r = pd - pc) > es) { 
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		n = r / es;
		goto loop;
	}
/*		qsort(pn - r, r / es, es, cmp);*/
}






/* uclibc */

typedef int (*__compar_fn_t) (const void *, const void *);
typedef int (*__compar_d_fn_t) (const void *, const void *, void *);
void uclibc_qsort_r(void  *base,
           size_t nel,
           size_t width,
           __compar_d_fn_t comp,
		   void *arg)
{
	size_t wgap, i, j, k;
	char tmp;

	if ((nel > 1) && (width > 0)) {
		/*assert(nel <= ((size_t)(-1)) / width); [> check for overflow <]*/
		wgap = 0;
		do {
			wgap = 3 * wgap + 1;
		} while (wgap < (nel-1)/3);
		/* From the above, we know that either wgap == 1 < nel or */
		/* ((wgap-1)/3 < (int) ((nel-1)/3) <= (nel-1)/3 ==> wgap <  nel. */
		wgap *= width;			/* So this can not overflow if wnel doesn't. */
		nel *= width;			/* Convert nel to 'wnel' */
		do {
			i = wgap;
			do {
				j = i;
				do {
					register char *a;
					register char *b;

					j -= wgap;
					a = j + ((char *)base);
					b = a + wgap;
					if ((*comp)(a, b, arg) <= 0) {
						break;
					}
					k = width;
					do {
						tmp = *a;
						*a++ = *b;
						*b++ = tmp;
					} while (--k);
				} while (j >= wgap);
				i += width;
			} while (i < nel);
			wgap = (wgap - width)/3;
		} while (wgap);
	}
}


void uclibc_qsort(void  *base,
           size_t nel,
           size_t width,
           __compar_fn_t comp)
{
	return uclibc_qsort_r (base, nel, width, (__compar_d_fn_t) comp, NULL);
}







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

void fifty_percent(uint32_t *array, size_t nums) {
  pcg32_random_t r;
  memcpy(&r, &start, sizeof(pcg32_random_t));
  for (size_t i = 0; i < nums/2; i++) array[2*i] = i, array[2*i+1] = pcg32_random_r(&r);
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
    {        qsort, "glibc" , 0 },
    {   musl_qsort, "musl"  , 0 },
    {   diet_qsort, "diet"  , 0 },
    {    bsd_qsort, "bsd"   , 0 },
    { uclibc_qsort, "uclibc", 0 },
    {     my_qsort, "mine"  , 0 },
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
    {   fifty_percent, "50% sorted"      },
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
    printf("%zu. %s\n", j+1, qsorts[j].name);

  return 0;
}
