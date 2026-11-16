#ifndef TROLLFX_DSP_H
#define TROLLFX_DSP_H
#include <stddef.h>

#define DSP_FFT_SIZE 32768
#define DSP_BANDS 31

typedef struct { double hz, db; } calibration_point;

int dsp_init(void);
void dsp_analyze(const float *samples, size_t count, double sample_rate,
                 const calibration_point *calibration, size_t calibration_count,
                 double band_db[DSP_BANDS], double *rms_dbfs,
                 double *z_corrected_dbfs, double *a_weighted_dbfs);
size_t dsp_load_calibration(const char *path, calibration_point *points,
                            size_t capacity, double *sensitivity_factor_db);
double dsp_calibration_db(const calibration_point *points, size_t count,
                          double hz);
double dsp_a_weight_db(double hz);
double dsp_spl_db(double rms_dbfs, double sensitivity_dbfs_at_94_db_spl);

#endif
