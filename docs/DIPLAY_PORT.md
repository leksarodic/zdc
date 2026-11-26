# DiPlay → 7010B port: feasibility report

Branch: `diplay-port`  
Upstream: https://github.com/shihabal3amri/DiPlay  
Target considered: 7010B / ZCD-7089-6905C, Allwinner F1C200s-class ARM926, Melis/ePDK, 800×480, 8 MiB SPI NOR

## Verdict

**A direct port is not possible, and a practical port to the stock 7010B is not realistic.**

DiPlay is an **Android application** (Kotlin + Jetpack Compose, Android SDK 37, minSdk 24) that implements a **CarPlay receiver** for compatible **BYD Android head units**. The 7010B is **not Android** and does not run Linux. It runs **Allwinner Melis/ePDK** on an **ARMv5TE / ARM926EJ-S** SoC with 64 MiB RAM and 8 MiB SPI flash.

To get DiPlay-like CarPlay on this hardware you would not be porting the app. You would be writing a **new CarPlay receiver in C** for a new OS, plus adding Wi-Fi hardware and solving the accessory-authentication problem. That is a multi-month, high-risk project, and the F1C200s may still be too small.

If the goal is CarPlay, the sane options are:

1. Run DiPlay on a compatible **Android** head unit (its actual target).
2. Use a separate CarPlay module/dongle and keep the 7010B as display/amp, if the input path allows it.
3. Use a more capable **Linux SBC** with Wi-Fi, USB host and hardware H.264, running an existing Linux CarPlay receiver (OpenAuto/joyful-class), and replace the 7010B SoC or use a different panel/amp.

A **UI-only** DiPlay-like projection screen in the Strada UI is possible, but that is not CarPlay.

## What DiPlay actually is

Evidence from the upstream tree (snapshot inspected on 2026-11-26):

| Area | Finding |
|---|---|
| Language | ~1,029 Kotlin files, 341 XML, 126 Java, 13 C, 9 C headers |
| Build | Gradle, Android Gradle Plugin, Kotlin Compose, JDK 25, compileSdk 37, minSdk 24 |
| Modules | `common`, `shared`, `mobile`, `automotive`, `samples/maphost`, `samples/home` |
| UI | Jetpack Compose |
| Native | NDK 28.2, JNI: Speex echo cancellation, I2C, local hotspot radio |
| Transports | USB accessory and Wi-Fi (Wi-Fi Direct, car hotspot, existing Wi-Fi/same LAN), mDNS |
| Projection | CarPlay receiver, H.264 video (default 30 fps), audio, touch/HID |
| Car integration | BYD HUD/cluster, Geely/Lynk steering-wheel keys, vendor services |
| Identity | Experimental Carlinkit-derived accessory identity (`offline-mfi/identity.pk8`, `certificate.p7b`), **not** in the source tree; explicitly not Apple-certified |
| License | GPL-3.0 (based on xcertplay) + AGPL-3.0 UI portions (DiAuto); Apple/CarPlay trademarks |
| Target | Android 7.0+ head units, primarily BYD DiLink |

The app is substantial and Android-specific. The native C is a small part: Speex DSP, I2C, and a local-hotspot radio helper. The CarPlay session, transport, video/audio pipeline, UI, settings, and vehicle integrations are Kotlin/Java on Android APIs.

## Why it cannot run on the 7010B

| 7010B | DiPlay requirement | Result |
|---|---|---|
| Melis/ePDK RTOS | Android 7.0+ runtime | **No** |
| ARMv5TE ARM926 | Kotlin/Java/ART, Android NDK arm64/armv7 | **No** |
| 8 MiB SPI NOR | APK + Android system + media buffers | **No** |
| 64 MiB RAM | Android + Compose + H.264 pipeline + buffers | **No** |
| 800×480 resistive | Android touch/display stack | **No** |
| No Wi-Fi hardware | Wi-Fi Direct / hotspot / mDNS for wireless CarPlay | **No** |
| USB via stock MCU/firmware | USB accessory host + CarPlay transport | **No** on stock firmware |
| No MFi/CarPlay identity | Accessory authentication | **No**; experimental identity is legally/operationally risky |

The stock 7010B does not even expose a general Android USB host stack or Wi-Fi. Its USB is used for mass storage/update and its MCU owns audio/radio/BT. There is no Android `MediaCodec`, `SurfaceFlinger`, `UsbManager`, `WifiP2pManager`, or `BouncyCastle` runtime to port against.

## USB cable only — verdict

**Verdict: USB does not make DiPlay portable to the stock 7010B. It is still not possible as a direct port.**

DiPlay's wired mode is an **Android USB host/accessory** path. The app uses Android `UsbManager`/USB APIs, Android `MediaCodec`/Surface for H.264, Android audio/routing, and the same Kotlin session layer as wireless. The USB cable removes Wi-Fi from the equation, but it does not remove Android, the CarPlay accessory protocol, the authenticated accessory identity, or the H.264/audio/touch pipeline.

For the 7010B:

| USB requirement | 7010B stock reality |
|---|---|
| Android USB host/accessory stack | Melis/ePDK; no Android USB host stack for apps |
| Kotlin/Compose session layer | No Android runtime; ARMv5 cannot run ART |
| CarPlay accessory authentication | No MFi identity; upstream identity is experimental/legal-risk |
| H.264 decode through MediaCodec/Surface | CedarX exists, but a different API/pipeline |
| Audio/AEC + HID/touch forwarding | Not implemented in the stock firmware |
| Enough RAM/flash | 64 MiB / 8 MiB total; Android + buffers do not fit |
| Accessible USB role control | The stock USB path is MCU/firmware-owned (storage/update), not a documented CarPlay host |

A wired-only **Linux** port is more plausible than wireless because it avoids Wi-Fi Direct/hotspot and mDNS, but it is still a ground-up receiver:

* USB host/device role control and Apple accessory protocol framing.
* CarPlay session setup and a lawful accessory identity.
* H.264 decode on CedarX with a new pipeline.
* Audio, microphone/AEC, touch/HID.
* A UI that can survive reconnects.

On an ARM926 at ~600 MHz with 64 MiB RAM, even wired 720p H.264 + audio + UI is tight. The 7010B also has no documented general-purpose USB host port for this use; the stock USB is for storage/update through the MCU.

### USB feasibility gates

Do not start a 7010B port until all of these are true:

1. The board exposes a USB host port with controllable role and enough power for an iPhone.
2. CedarX can decode the CarPlay stream at the requested size/fps with CPU headroom for UI/audio.
3. You have a lawful CarPlay accessory identity and can provision it without shipping extracted keys.
4. A wired-only receiver works on a Linux SBC with the same decoder class.
5. You have a full SPI flash backup and a recovery path.

If those fail, use an Android head unit with DiPlay, a separate CarPlay dongle, or a more capable Linux SBC. A USB cable alone does not close the gap.

## What could be reused

| DiPlay part | Reuse on 7010B? | Notes |
|---|---|---|
| Compose UI | No | Different toolkit/runtime |
| Kotlin CarPlay session code | No | Android APIs/threading/storage |
| USB/Wi-Fi transport | No | Android `UsbManager` / `WifiP2pManager` |
| H.264 decode via MediaCodec | No | F1C200s has CedarX, but a different API/pipeline |
| Speex echo cancellation (C) | Maybe | Portable C; would need a Linux/Melis audio integration |
| I2C helper (C) | Maybe | Linux ioctl only; not Melis |
| Accessory identity handling | Legally risky | Experimental, extractable, not certified |
| BYD/Geely integrations | No | Vendor Android services |
| Protocol lessons/docs | Maybe | Still requires a from-scratch implementation |

## What a real CarPlay receiver on this class of hardware needs

1. **Transport**
   - Wired USB: USB host/device role, Apple accessory protocol, bulk endpoints, framing.
   - Wireless: Wi-Fi (preferably 5 GHz), Wi-Fi Direct or hotspot, mDNS/Bonjour, pairing, encryption.
2. **CarPlay session**
   - Accessory authentication/MFi identity, session setup, display stream negotiation, audio, HID/touch, phone call state.
3. **Video**
   - H.264 decode at 720p/30 or higher; CedarX can decode H.264, but the integration, buffering, and UI composition are not present.
4. **Audio**
   - AAC/PCM/Opus paths, clock sync, echo cancellation, mic, AEC, routing to the amplifier.
5. **Input**
   - Touch/HID forwarding, steering-wheel keys, reverse camera/AV input switching.
6. **UI**
   - Projection surface, connection states, settings, diagnostics, error recovery.
7. **Hardware**
   - Wi-Fi module, more RAM/flash, USB host with proper role control, and a legal accessory identity.

On an ARM926 at ~600 MHz with 64 MiB RAM, wired 720p H.264 + audio + USB + UI is already tight; wireless adds Wi-Fi stack, mDNS, crypto, and retransmission. It is not a sensible first target.

## Effort and risk

A ground-up receiver is not a weekend port:

* Protocol/transport reverse engineering: weeks to months.
* USB/Wi-Fi and authentication: months, with hardware and legal dependencies.
* H.264/audio pipeline on CedarX/Melis: months.
* Car integration, reliability, thermal/power: months.
* Accessory identity: cannot be solved by code alone; the upstream project itself calls its identity experimental and extractable.

This is a **6–12+ month project for a small experienced team**, with hardware changes and legal review, not a branch change.

## Recommended paths

### 1. Keep DiPlay on Android (recommended if you want CarPlay now)

Use a compatible Android head unit. This is the supported path. The 7010B is not an Android device.

### 2. Add a CarPlay module, keep the 7010B as a display/amp

A separate CarPlay dongle/module can output video/audio to an AUX/AV input. The 7010B's stock firmware would need a way to select that input and pass touch; the stock Melis app does not expose CarPlay integration. This may be more practical than porting DiPlay, but it is a hardware integration project, not a software port.

### 3. Linux SBC with Wi-Fi + hardware H.264

Use a Raspberry Pi 4/5, RK3566, or similar with Wi-Fi and USB host, run a Linux CarPlay receiver (OpenAuto/joyful-class), and connect the 800×480 panel/amp. This is a new device, not the stock 7010B.

### 4. UI-only projection screen

If you want the **look** of a CarPlay screen in the Strada UI, add a projection screen with connect states and a black video surface. It cannot display CarPlay without the receiver stack. This branch provides an interface contract for that future work, not a fake implementation.

## Branch scope

This branch is a **feasibility spike**:

* `docs/DIPLAY_PORT.md` — this report.
* `diplay/include/diplay_session.h` — a transport/session contract for a future C receiver.
* `diplay/README.md` — scope, non-goals, legal notes.

No DiPlay source is copied. The upstream is GPL-3.0/AGPL-3.0; any future port must preserve those licenses, and the CarPlay identity must be handled lawfully. Do not ship extracted accessory keys.

## If you still want to proceed

Start with a **hardware feasibility gate**:

1. Confirm the 7010B actually has usable USB host and enough free GPIO for Wi-Fi; otherwise stop.
2. Measure CedarX H.264 720p decode + UI + audio on the F1C200s; if it cannot sustain 30 fps with headroom, stop.
3. Decide the accessory identity path with legal review; without it, CarPlay will not connect.
4. Build a wired-only proof of concept on a Linux SBC first. Porting to Melis should come only after the protocol and video/audio pipeline work.
5. Keep the stock firmware flashable backup; do not brick the radio for a prototype.

If those gates pass, the interface in `diplay/include/diplay_session.h` is the integration point for the UI.
