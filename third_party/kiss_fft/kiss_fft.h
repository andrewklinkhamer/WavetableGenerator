/*
 * Kiss FFT - A simple, efficient FFT implementation
 * Public Domain
 *
 * This is a simplified version optimized for real-valued audio signals.
 */

#ifndef KISS_FFT_H
#define KISS_FFT_H

#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Complex number structure */
typedef struct {
    float r;  /* Real part */
    float i;  /* Imaginary part */
} kiss_fft_cpx;

/* Opaque FFT configuration structure */
typedef struct kiss_fft_state* kiss_fft_cfg;

/*
 * Initialize FFT configuration
 * nfft: size of FFT (must be power of 2)
 * inverse_fft: 0 for forward FFT, 1 for inverse FFT
 * Returns: configuration handle (must be freed with kiss_fft_free)
 */
kiss_fft_cfg kiss_fft_alloc(int nfft, int inverse_fft, void* mem, size_t* lenmem);

/*
 * Free FFT configuration
 */
void kiss_fft_free(kiss_fft_cfg cfg);

/*
 * Perform FFT
 * cfg: configuration from kiss_fft_alloc
 * fin: input array of nfft complex samples
 * fout: output array of nfft complex samples
 */
void kiss_fft(kiss_fft_cfg cfg, const kiss_fft_cpx* fin, kiss_fft_cpx* fout);

/*
 * Real-valued FFT (more efficient for audio)
 * Initialize with kiss_fftr_alloc, use kiss_fftr and kiss_fftri
 */
typedef struct kiss_fftr_state* kiss_fftr_cfg;

kiss_fftr_cfg kiss_fftr_alloc(int nfft, int inverse_fft, void* mem, size_t* lenmem);
void kiss_fftr_free(kiss_fftr_cfg cfg);

/* Real forward FFT: nfft real inputs -> nfft/2+1 complex outputs */
void kiss_fftr(kiss_fftr_cfg cfg, const float* timedata, kiss_fft_cpx* freqdata);

/* Real inverse FFT: nfft/2+1 complex inputs -> nfft real outputs */
void kiss_fftri(kiss_fftr_cfg cfg, const kiss_fft_cpx* freqdata, float* timedata);

#ifdef __cplusplus
}
#endif

#endif /* KISS_FFT_H */

