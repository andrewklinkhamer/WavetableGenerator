/*
 * Kiss FFT - A simple, efficient FFT implementation
 * Public Domain
 */

#include "kiss_fft.h"
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Internal FFT state structure */
struct kiss_fft_state {
    int nfft;
    int inverse;
    kiss_fft_cpx* twiddles;
    int* factors;
};

struct kiss_fftr_state {
    kiss_fft_cfg substate;
    kiss_fft_cpx* tmpbuf;
    kiss_fft_cpx* super_twiddles;
    int nfft;
};

/* Complex multiplication */
static void C_MUL(kiss_fft_cpx* m, const kiss_fft_cpx a, const kiss_fft_cpx b) {
    m->r = a.r * b.r - a.i * b.i;
    m->i = a.r * b.i + a.i * b.r;
}

/* Complex addition */
static void C_ADD(kiss_fft_cpx* res, const kiss_fft_cpx a, const kiss_fft_cpx b) {
    res->r = a.r + b.r;
    res->i = a.i + b.i;
}

/* Complex subtraction */
static void C_SUB(kiss_fft_cpx* res, const kiss_fft_cpx a, const kiss_fft_cpx b) {
    res->r = a.r - b.r;
    res->i = a.i - b.i;
}

/* Check if n is power of 2 */
static int is_power_of_2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/* Bit reversal for FFT reordering */
static unsigned int bit_reverse(unsigned int x, int bits) {
    unsigned int result = 0;
    for (int i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/* Get log2 of power of 2 */
static int log2_int(int n) {
    int log = 0;
    while (n > 1) {
        n >>= 1;
        log++;
    }
    return log;
}

/* Allocate FFT configuration */
kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft, void* mem, size_t* lenmem) {
    if (!is_power_of_2(nfft)) {
        return NULL;
    }

    kiss_fft_cfg st = (kiss_fft_cfg)malloc(sizeof(struct kiss_fft_state));
    if (!st) return NULL;

    st->nfft = nfft;
    st->inverse = inverse_fft;

    /* Allocate twiddle factors */
    st->twiddles = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
    if (!st->twiddles) {
        free(st);
        return NULL;
    }

    /* Compute twiddle factors */
    for (int i = 0; i < nfft; i++) {
        double phase = -2.0 * M_PI * i / nfft;
        if (inverse_fft) phase = -phase;
        st->twiddles[i].r = (float)cos(phase);
        st->twiddles[i].i = (float)sin(phase);
    }

    st->factors = NULL;
    return st;
}

/* Free FFT configuration */
void kiss_fft_free(kiss_fft_cfg cfg) {
    if (cfg) {
        free(cfg->twiddles);
        free(cfg->factors);
        free(cfg);
    }
}

/* Radix-2 FFT implementation (Cooley-Tukey) */
void kiss_fft(kiss_fft_cfg st, const kiss_fft_cpx* fin, kiss_fft_cpx* fout) {
    int n = st->nfft;
    int logn = log2_int(n);

    /* Bit-reversal permutation */
    for (int i = 0; i < n; i++) {
        int j = bit_reverse(i, logn);
        fout[j] = fin[i];
    }

    /* Iterative FFT */
    for (int s = 1; s <= logn; s++) {
        int m = 1 << s;
        int m2 = m >> 1;

        for (int k = 0; k < n; k += m) {
            for (int j = 0; j < m2; j++) {
                int t_idx = (n / m) * j;
                kiss_fft_cpx t, u;

                C_MUL(&t, st->twiddles[t_idx], fout[k + j + m2]);
                u = fout[k + j];
                C_ADD(&fout[k + j], u, t);
                C_SUB(&fout[k + j + m2], u, t);
            }
        }
    }

    /* Scale for inverse FFT */
    if (st->inverse) {
        float scale = 1.0f / n;
        for (int i = 0; i < n; i++) {
            fout[i].r *= scale;
            fout[i].i *= scale;
        }
    }
}

/* Allocate real FFT configuration */
kiss_fftr_cfg kiss_fftr_alloc(int nfft, int inverse_fft, void* mem, size_t* lenmem) {
    kiss_fftr_cfg st = (kiss_fftr_cfg)malloc(sizeof(struct kiss_fftr_state));
    if (!st) return NULL;

    st->nfft = nfft;
    st->substate = kiss_fft_alloc(nfft / 2, inverse_fft, NULL, NULL);

    if (!st->substate) {
        free(st);
        return NULL;
    }

    st->tmpbuf = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * (nfft / 2));
    st->super_twiddles = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * (nfft / 2));

    if (!st->tmpbuf || !st->super_twiddles) {
        kiss_fft_free(st->substate);
        free(st->tmpbuf);
        free(st->super_twiddles);
        free(st);
        return NULL;
    }

    /* Compute super twiddles for real FFT */
    for (int i = 0; i < nfft / 2; i++) {
        double phase = -M_PI * i / (nfft / 2);
        if (inverse_fft) phase = -phase;
        st->super_twiddles[i].r = (float)cos(phase);
        st->super_twiddles[i].i = (float)sin(phase);
    }

    return st;
}

/* Free real FFT configuration */
void kiss_fftr_free(kiss_fftr_cfg cfg) {
    if (cfg) {
        kiss_fft_free(cfg->substate);
        free(cfg->tmpbuf);
        free(cfg->super_twiddles);
        free(cfg);
    }
}

/* Real forward FFT */
void kiss_fftr(kiss_fftr_cfg st, const float* timedata, kiss_fft_cpx* freqdata) {
    int ncfft = st->nfft / 2;

    /* Pack real data into complex (interleaved even/odd samples) */
    for (int i = 0; i < ncfft; i++) {
        st->tmpbuf[i].r = timedata[2 * i];
        st->tmpbuf[i].i = timedata[2 * i + 1];
    }

    /* Perform complex FFT on half-size */
    kiss_fft(st->substate, st->tmpbuf, freqdata);

    /* Post-process to get full spectrum */
    freqdata[ncfft].r = freqdata[0].r - freqdata[0].i;
    freqdata[ncfft].i = 0.0f;
    freqdata[0].r = freqdata[0].r + freqdata[0].i;
    freqdata[0].i = 0.0f;

    for (int k = 1; k < ncfft; k++) {
        kiss_fft_cpx fpk = freqdata[k];
        kiss_fft_cpx fpnk = freqdata[ncfft - k];
        kiss_fft_cpx f1k, f2k, tw;

        /* Unpack to get actual spectrum */
        f1k.r = (fpk.r + fpnk.r) * 0.5f;
        f1k.i = (fpk.i - fpnk.i) * 0.5f;
        f2k.r = (fpk.i + fpnk.i) * 0.5f;
        f2k.i = (fpnk.r - fpk.r) * 0.5f;

        C_MUL(&tw, f2k, st->super_twiddles[k]);
        freqdata[k].r = f1k.r + tw.r;
        freqdata[k].i = f1k.i + tw.i;
        freqdata[ncfft - k].r = f1k.r - tw.r;
        freqdata[ncfft - k].i = tw.i - f1k.i;
    }
}

/* Real inverse FFT */
void kiss_fftri(kiss_fftr_cfg st, const kiss_fft_cpx* freqdata, float* timedata) {
    int ncfft = st->nfft / 2;

    /* Pre-process spectrum */
    st->tmpbuf[0].r = freqdata[0].r + freqdata[ncfft].r;
    st->tmpbuf[0].i = freqdata[0].r - freqdata[ncfft].r;

    for (int k = 1; k < ncfft; k++) {
        kiss_fft_cpx fk = freqdata[k];
        kiss_fft_cpx fnkc;
        fnkc.r = freqdata[ncfft - k].r;
        fnkc.i = -freqdata[ncfft - k].i;

        kiss_fft_cpx f1k, f2k, tw;
        f1k.r = (fk.r + fnkc.r) * 0.5f;
        f1k.i = (fk.i + fnkc.i) * 0.5f;
        f2k.r = (fk.i - fnkc.i) * 0.5f;
        f2k.i = (fnkc.r - fk.r) * 0.5f;

        /* Conjugate twiddle for inverse */
        kiss_fft_cpx tw_conj = st->super_twiddles[k];
        tw_conj.i = -tw_conj.i;
        C_MUL(&tw, f2k, tw_conj);

        st->tmpbuf[k].r = f1k.r + tw.r;
        st->tmpbuf[k].i = f1k.i + tw.i;
    }

    /* Perform complex inverse FFT */
    kiss_fft_cpx* tmp2 = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * ncfft);
    kiss_fft(st->substate, st->tmpbuf, tmp2);

    /* Unpack back to real */
    for (int i = 0; i < ncfft; i++) {
        timedata[2 * i] = tmp2[i].r;
        timedata[2 * i + 1] = tmp2[i].i;
    }

    free(tmp2);
}

