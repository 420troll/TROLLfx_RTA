# SPL reference setup for UMIK-1 7033298 (V0.2)

The RTA already applies the individual 0° / 90° frequency-response correction. Both downloaded files specify `Sens Factor = 2.7673 dB`, but the raw UMIK PCM signal at `hw:U18dB,0` still requires a known **reference acoustic level** to establish absolute SPL. The sensitivity value alone is not a verified Force ALSA capture offset.

## Using the browser reference control

1. Leave Force's audio interface on **Internal Audio**.
2. Run V0.2 using `bash scripts/build-armv7.sh && bash scripts/install-test-v02.sh`. Connect via SSH tunnel on port 8096.
3. Present a **known, stable Z-weighted SPL** at the UMIK's capsule. Preferred: a compatible 94 dB SPL / 1 kHz acoustic calibrator. An independently measured steady sound level can be used for an *approximate* comparison, but is not traceable.
4. When the reference is stable, type the known dB SPL value into **SPL REFERENCE (Z, dB)** and click **SET SPL REFERENCE**. The app records the currently measured response-corrected Z dBFS and computes:
   `dBFS_at_94 = 94 + current_Z_dBFS - reference_dB_SPL`
5. Read **LZ** (Z-weighted level) and **LA** (A-weighted level). The UI labels measurements from the browser control **USER REF**; this is not automatically verified calibration.
6. Click **CLEAR SPL REF** to revert to the provisional state if the reference is uncertain or the microphone input gain changes. Browser reference is intentionally not saved across application restarts.

The API exposes `spl_reference_applied`, `spl_reference_type` (`none`, `user`, `cli`), and `sensitivity_dbfs_at_94`. The `calibrated` flag remains false for an unverified user reference.

An independently verified 94 dB calibration value can also be supplied at startup with `--sensitivity-dbfs-at-94 <measured_dBFS>`; the API identifies this as a CLI-supplied anchor.

## Limitations

- The SPL readout uses the existing windowed FFT-derived Z / A levels integrated over 20 Hz–20 kHz. It is not yet a standards-certified sound-level meter, and is not time-weighted FAST/SLOW or IEC 61672 verified.
- The spectrum UI's display gain (+11 dB default), display floor, averaging, and LED speeds never alter the SPL calibration anchor.
- A REW reading made using the **same single UMIK** cannot be measured simultaneously with the Force. A REW comparison after moving the microphone between hosts is a rough check unless the acoustic field and microphone position remain sufficiently stable.
- Never use the mic calibration sensitivity factor of +2.7673 dB as though it directly represented the Force's captured dBFS at 94 dB SPL.
- A USB gain or interface-volume change invalidates the reference and requires recalibration.

References: [REW cal file reference](https://roomeqwizard.com/betahelp/help/html/calfiles.html), [miniDSP UMIK with SPLMeter](https://www.minidsp.com/applications/acoustic-measurements/umik-1-splmeter).
