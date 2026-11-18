TROLLfx Sweep Lab V0.5 offline DSP test candidate.
This ARMv7 hard-float utility analyzes completed mono WAV recordings only.
It does not start a sweep, open ALSA, control Force output routing, or replace
the V0.4 RTA plugin. Use: trollfx-sweep-cli-armv7 analyze reference.wav
capture.wav output-session [clock_ppm [start_hz stop_hz [calibration.txt [0|90]]]].
Use the included calibration/7033298.txt or _90deg.txt for frequency-response
correction. Sens Factor 2.7673 dB is retained as metadata, not used as an
absolute SPL offset; results remain PROVISIONAL for absolute SPL. This
candidate is not an installable VST update.
