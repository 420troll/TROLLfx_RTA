TROLLfx RTA V0.2 measurement service bundled for V0.4.
The VST launches this executable from the installed plugin's Service directory.
It binds only to 127.0.0.1:8096 and captures hw:U18dB,0 as S24_3LE at 48 kHz.
The service claims the port before opening ALSA, so competing launchers do not
open a second capture stream. Calibration curves for 0 and 90 degrees are in
calibration/. It retries ALSA capture after device errors. Force playback routing
is not modified. Logs are written to /tmp/trollfx-rta-v04.log.
