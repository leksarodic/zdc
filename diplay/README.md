# DiPlay port — branch scope

This directory belongs to the `diplay-port` branch. It is a **feasibility spike**, not a working CarPlay receiver.

* Read `docs/DIPLAY_PORT.md` first.
* `include/diplay_session.h` is a transport/session contract for a future C implementation.
* No DiPlay source is copied. DiPlay is GPL-3.0 (xcertplay) and its UI portions are AGPL-3.0 (DiAuto); any reuse must preserve those licenses.
* No accessory identity or private key is included. CarPlay requires a lawful MFi/accessory identity; the upstream project describes its own bundled identity as experimental and extractable.
* The stock 7010B runs Melis/ePDK on an ARMv5 F1C200s-class SoC. It cannot run the Android APK and has no Wi-Fi for wireless CarPlay.

## Why the header exists

If a port is attempted later, the UI and the receiver should meet at a small boundary:

```
Strada UI  ←  diplay_session.h  →  transport + CarPlay + H.264 + audio
```

The UI would only need:

* session state (idle/connecting/connected/streaming/error),
* an H.264 frame callback (to the decoder),
* an audio callback,
* touch/key forwarding.

That keeps a future receiver replaceable and prevents the UI from depending on DiPlay internals.

## Recommended next step

Do not start on the 7010B. First build a **wired-only Linux proof of concept** on a board with:

* USB host/device role control,
* hardware H.264 decode,
* enough RAM and flash,
* a lawful CarPlay identity.

If that works, then evaluate whether the 7010B hardware can run the same pipeline. See the hardware gates in `docs/DIPLAY_PORT.md`.
