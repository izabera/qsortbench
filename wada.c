/*  a-wada/qsortlib/ccg/qsortsp.c                                Sep. 9, 2008 */
/*  Copyright (c) 2008  Akira Wada <a-wada@mars.dti.ne.jp>                    */

/*      Quick sort compatible with "qsort (); in <stdlib.h>"                  */
/*          efficiency by reducing the times of key comparisons and           */
/*              word_mode_swapping for word_aligned object                    */
/*          prevention from the degeneration caused by peculiar input         */
/*      This qsort is not stable, but could avoid the bad behavior by many    */
/*      equal sort-keys. For stability, add a unique-key to compare-function. */
#include <time.h>
#define COMP(i, j) (*cf)(i, j)
#define SZI        sizeof (int)
#define WD(i)      *((int *) (i + w))
#define CH(i)      *(i + w)
#define SWAP(i, j) do {                                                        \
            w = es; if (al <= 0) do w -= SZI,                                  \
                t = WD(i), WD(i) = WD(j), WD(j) = t; while (w > 0);            \
            else do --w,                                                       \
                t = CH(i), CH(i) = CH(j), CH(j) = t; while (w > 0);} while (0)
#define ROTATE(i, j, k) do {                                                   \
            w = es; if (al <= 0) do w -= SZI,                                  \
                t = WD(i), WD(i) = WD(j), WD(j) = WD(k), WD(k) = t;            \
                while (w > 0);                                                 \
            else do --w,                                                       \
                t = CH(i), CH(i) = CH(j), CH(j) = CH(k), CH(k) = t;            \
                while (w > 0);} while (0)
#define STACK      s[STKSZ], *p = s + STKSZ
#define STKSZ      sizeof (size_t) * 16
#define PUSH(x, y) p -= 2, *(p + 1) = x, *p = y
#define EMPTY      (p >= s + STKSZ)
#define POP(y, x)  y = *p, x = *(p + 1), p += 2
#define MDT    16  /* threshold for median of the 5 or the more,     MDT >= 8 */
#define MDM     2  /* multiplier for threshold,                      MDM >= 2 */
#define DGT   128  /* threshold of ps for checking degeneration               */
#define DGV    16  /* degeneration assumed if the smaller < (ps / DGV)        */
#define DGS     0  /* threshold for samples distributed uniformly             */
#define DGL        ((unsigned) ps / DGV) * es
typedef int (*comp_t) (const void *, const void *);
typedef char *  ptrtyp;

void wada_qsort (void *A, size_t N, size_t es, comp_t cf) {
    ptrtyp  l, r, m, i, j, k, n, STACK;
    int ps, th, c, al, t, w, np, iv, dgn = 0;

    if (N > 1 && es > 0) {
        al = ((int) A | es) & (SZI - 1);
        l = i = (ptrtyp) A, r = j = i + (N - 1) * es;
        for ( ; ; ) {                     /* initialize for current partition */
            if ((ps = (j - i) / es) > 2) m = l, i += es;
            else m = i + (ps - 1) * es;
            ps++, iv = es, th = MDT;
            if (dgn > DGS && ps > MDT) {           /* if degeneration assumed */
                np = 3; while (ps > th) th *= MDM, np += 2; th = MDT;
                iv = (ps / (np - 1)) * es; k = i + (time (0) % (ps - 2)) * es;
                if (i < k) SWAP (i, k);}
                                 /* if ps <= 3, sort the partition else bring */
            for ( ; ; ) {                     /* median of samples to leftend */
                if (COMP (m, j) <= 0) {
                    if (ps > 2 && COMP (i, m) > 0) {
                        n = i, k = j; do
                            if (COMP (k, n) < 0) n = k; while ((k += iv) <= r);
                        if (n == i) SWAP (m, i); else ROTATE (m, n, i);}}
                else {
                    if (ps == 2 || COMP (i, m) > 0) SWAP (i, j); else {
                        n = j, k = i; do
                            if (COMP (k, n) > 0) n = k; while ((k -= iv) > l);
                        if (n == j) SWAP (m, j); else ROTATE (m, n, j);}}
                i += iv, j -= iv;
                if (ps > th) th *= MDM; else break;}
                             /* do partitioning by leftend as pivot gathering */
            if (ps > 3) {                   /* equal keys adjacent to leftend */
                if (iv - es) i = l + es * 2, j = r - es;
                for (k = l + es; ; ) {
                    while (i <= j && (c = COMP (i, m)) <= 0) {
                        if (c == 0) {
                            SWAP (k, i); k += es;}
                        i += es;}
                    while (i < j && (c = COMP (m, j)) < 0) j -= es;
                    if (i >= j) break;
                    if (c != 0) SWAP (i, j); else {
                        ROTATE (k, j, i); k += es;}
                    j -= es, i += es;}
                j = i - es;       /* move pivot and equal keys to where to be */
                while (m < k && k <= j) {
                    SWAP (m, j); m += es, j -= es;}
                if (j < k) j = m - es;
                                                        /* check degeneration */
                if (ps >= DGT && (l + DGL > i || j + DGL > r)) dgn++;
                if (j - l < r - i) {             /* select for next iteration */
                    if (l >= j) l = i; else PUSH (i, r), r = j;}
                else {
                    if (i >= r) r = j; else PUSH (l, j), l = i;}
                if ((i = l) < (j = r)) continue;}
                                                    /* pop up next from stack */
            if (!EMPTY) POP (j, i), r = j, l = i; else break;}}}

/*  a-wada/qsortlib/ccg/qsortsp.c  end  */
/*
   *** Tested on Homemade PC cpu:850Mhz mm:256MB,  Oct 22, 2008 ***

D:\a-wada\borlandc>qstest
 P = qsort-opensolaris.c
 N = 100000 SEED= 0
 c = 1699958 1699958   57559       0   46428       0       0  828165  767806
 m =          803074   66918       0       0       0       0  736156       0
 t = 1.29 sec dgx = 0 SEED = 1224649877

D:\a-wada\borlandc>qstest
 P = qsortskb.c
 N = 100000 SEED= 1224649877
 c = 1598596 1598596       0   73699   59300   57015     350  719123  689109
 m =          913088       0   35588   34534   62178       0  780788       0
 t = 1.21 sec dgx = 0 SEED = 1224649877

D:\a-wada\borlandc>qstest
 P = qsortsn.c
 N = 100000 SEED= 1224649877
 c = 1595345 1595345       0   73555   59346   57181     388  744583  660292
 m =          828124   52482   35328   34326   62334       0  643654       0
 t = 1.20 sec dgx = 0 SEED = 1224649877

D:\a-wada\borlandc>qstest
 P = qsortsp.c
 N = 100000 SEED= 1224649877
 c = 1591989 1591989       0   75434   61159   59951       0  725704  669741
 m =          846859       0   36240   35348   64425   68612  642234       0
 t = 1.18 sec dgx = 0 SEED = 1224649877

D:\a-wada\borlandc>
*/
