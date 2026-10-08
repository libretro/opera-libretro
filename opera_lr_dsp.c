#if defined(THREADED_DSP) && defined(HAVE_THREADS)
#include "opera_lr_dsp_threaded.ic"
#else
#include "opera_lr_dsp_regular.ic"
#endif
