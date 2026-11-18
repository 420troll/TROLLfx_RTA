TROLLfx RTA/Sweep Lab V0.5 services. The RTA process captures the UMIK-1 at
48 kHz through the existing ALSA path and remains independent of Force playback.
The sweep CLI analyzes completed recordings only. Sweep playback is started by
the native VST START control, which arms the RTA capture service first and then
outputs the generated synchronized stereo sweep. Results are stored in
Service/sessions/. Calibration curves are bundled in calibration/.
Absolute SPL remains provisional until a reference microphone/SPL verification
is completed. The 2.7673 dB sensitivity factor is preserved as metadata and is
not treated as verified USB gain.
