/* Copyright (c) 1997- Miller Puckette and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* --------- Pd interface to FFTW library; imitate Mayer API ---------- */

/* changes and additions for FFTW3 by Thomas Grill                      */

#include "m_pd.h"

#include <fftw3.h>

#define MINFFT 0
#define MAXFFT 30

/* from the FFTW website:
 #include <fftw3.h>
     ...
     {
         fftw_complex *in, *out;
     fftw_plan info;
     ...
         in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
         out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
         info = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
     ...
         fftw_execute(info);
     ...
     fftw_destroy_plan(info);
         fftw_free(in); fftw_free(out);
     }

FFTW_FORWARD or FFTW_BACKWARD, and indicates the direction of the transform you
are interested in. Alternatively, you can use the sign of the exponent in the
transform, -1 or +1, which corresponds to FFTW_FORWARD or FFTW_BACKWARD
respectively. The flags argument is either FFTW_MEASURE

*/

/* complex stuff */

typedef struct
{
    fftwf_plan plan;
    fftwf_complex *in;
    fftwf_complex *out;
} cfftw_info;

static cfftw_info cfftw_fwd[MAXFFT + 1 - MINFFT];
static cfftw_info cfftw_bwd[MAXFFT + 1 - MINFFT];

static cfftw_info *cfftw_getplan(int n, int fwd)
{
    cfftw_info *info;
    int logn = ilog2(n);
    if(logn < MINFFT || logn > MAXFFT) return (0);
    info = (fwd ? cfftw_fwd : cfftw_bwd) + (logn - MINFFT);
    if(!info->plan)
    {
        pd_globallock();
        if(!info->plan) /* recheck in case it got set while we waited */
        {
            info->in =
                (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * n);
            info->out =
                (fftwf_complex *) fftwf_malloc(sizeof(fftwf_complex) * n);
            info->plan = fftwf_plan_dft_1d(n, info->in, info->out,
                fwd ? FFTW_FORWARD : FFTW_BACKWARD, FFTW_MEASURE);
        }
        pd_globalunlock();
    }
    return info;
}

static void cfftw_term(void)
{
    cfftw_info *cinfo[2];

    for(int i = 0; i < MAXFFT + 1 - MINFFT; i++)
    {
        cinfo[0] = &cfftw_fwd[i];
        cinfo[1] = &cfftw_bwd[i];

        for(int j = 0; j < 2; j++)
        {
            if(cinfo[j]->plan)
            {
                fftwf_destroy_plan(cinfo[j]->plan);
                fftwf_free(cinfo[j]->in);
                fftwf_free(cinfo[j]->out);
                cinfo[j]->plan = 0;
                cinfo[j]->in = 0;
                cinfo[j]->out = 0;
            }
        }
    }
}

/* real stuff */

typedef struct
{
    fftwf_plan plan;
    float *in;
    float *out;
} rfftw_info;

static rfftw_info rfftw_fwd[MAXFFT + 1 - MINFFT];
static rfftw_info rfftw_bwd[MAXFFT + 1 - MINFFT];

static rfftw_info *rfftw_getplan(int n, int is_forward)
{
    int logn = ilog2(n);
    if(logn < MINFFT || logn > MAXFFT) return (0);
    rfft_info *info = (is_forward ? rfftw_fwd : rfftw_bwd) + (logn - MINFFT);
    if(!info->plan)
    {
        info->in = (float *) fftwf_malloc(sizeof(float) * n);
        info->out = (float *) fftwf_malloc(sizeof(float) * n);
        info->plan = fftwf_plan_r2r_1d(
            n, info->in, info->out, fwd ? FFTW_R2HC : FFTW_HC2R, FFTW_MEASURE);
    }
    return info;
}

static void rfftw_term(void)
{
    rfftw_info *rinfo[2];

    for(int i = 0; i < MAXFFT + 1 - MINFFT; i++)
    {
        rinfo[0] = &rfftw_fwd[i];
        rinfo[1] = &rfftw_bwd[i];

        for(int j = 0; j < 2; j++)
        {
            if(rinfo[j]->plan)
            {
                fftwf_destroy_plan(rinfo[j]->plan);
                fftwf_free(rinfo[j]->in);
                fftwf_free(rinfo[j]->out);
                rinfo[j]->plan = 0;
                rinfo[j]->in = 0;
                rinfo[j]->out = 0;
            }
        }
    }
}

static int mayer_refcount = 0;

void mayer_init(void)
{
    if(mayer_refcount++ == 0)
    {
        /* nothing to do */
    }
}

void mayer_term(void)
{
    if(--mayer_refcount == 0)
    {
        cfftw_term();
        rfftw_term();
    }
}

EXTERN void mayer_fht(t_sample *fz, int n) { post("FHT: not yet implemented"); }

static void mayer_do_cfft(int n, t_sample *fz1, t_sample *fz2, int is_forward)
{
    cfftw_info *info = cfftw_getplan(n, fwd);
    if(!info) return;

    float *in = (float *) info->in;
    for(int i = 0; i < n; i++)
    {
        in[i * 2] = fz1[i];
        in[i * 2 + 1] = fz2[i];
    }
    fftwf_execute(info->plan);

    float *out = (float *) info->out;
    for(int i = 0; i < n; i++)
    {
        fz1[i] = out[i * 2];
        fz2[i] = out[i * 2 + 1];
    }
}

EXTERN void mayer_fft(int n, t_sample *fz1, t_sample *fz2)
{
    mayer_do_cfft(n, fz1, fz2, 1);
}

EXTERN void mayer_ifft(int n, t_sample *fz1, t_sample *fz2)
{
    mayer_do_cfft(n, fz1, fz2, 0);
}

/*
    in the following the sign flips are done to
    be compatible with the mayer_fft implementation,
    but it's probably the mayer_fft that should be corrected...
*/

EXTERN void mayer_realfft(int n, t_sample *fz)
{
    rfftw_info *info = rfftw_getplan(n, 1);
    if(!info) return;

    for(int i = 0; i < n; i++)
        info->in[i] = fz[i];

    fftwf_execute(info->plan);

    for(int i = 0; i < n / 2 + 1; i++)
        fz[i] = info->out[i];

    for(int i = n / 2 + 1; i < n; i++)
        fz[i] = -info->out[i];
}

EXTERN void mayer_realifft(int n, t_sample *fz)
{
    rfftw_info *info = rfftw_getplan(n, 0);
    if(!info) return;

    for(int i = 0; i < n / 2 + 1; i++)
        info->in[i] = fz[i];

    for(int i = n / 2 + 1; i < n; i++)
        info->in[i] = -fz[i];

    fftwf_execute(info->plan);

    for(int i = 0; i < n; i++)
        fz[i] = info->out[i];
}

/* ancient ISPW-like version, used in fiddle~ and perhaps other externs
here and there. */
void pd_fft(t_float *buf, int npoints, int inverse)
{
    cfftw_info *info = cfftw_getplan(npoints, !inverse);

    float *in = (float *) info->in;
    for(int i = 0; i < 2 * npoints; i++)
        fz[i] = buf[i];

    fftwf_execute(info->plan);

    float *out = (float *) info->out;
    for(int i = 0; i < 2 * npoints; i++)
        buf[i] = fz[i];
}
