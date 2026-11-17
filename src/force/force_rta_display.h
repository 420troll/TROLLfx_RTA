#ifndef TROLLFX_FORCE_RTA_DISPLAY_H
#define TROLLFX_FORCE_RTA_DISPLAY_H
/* UI-only 31/16 band adapter. No ALSA, sockets, or audio-thread work. */
#define FORCE_RTA_INPUT_BANDS 31
#define FORCE_RTA_MAX_DISPLAY_BANDS 31
#define FORCE_RTA_LED_STEPS 24

typedef struct {
    int band_count;
    int led[FORCE_RTA_MAX_DISPLAY_BANDS];
    int peak_led[FORCE_RTA_MAX_DISPLAY_BANDS];
} force_rta_display;

int force_rta_render(const double levels[FORCE_RTA_INPUT_BANDS],
                     const double peaks[FORCE_RTA_INPUT_BANDS],
                     int mode, double display_gain_db, double floor_db,
                     force_rta_display *out);
#endif
