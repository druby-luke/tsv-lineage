# Kiosk Satellite on the ThinkSmart View

[Kiosk Satellite](https://github.com/jxlarrea/kiosk-satellite) is a Home
Assistant wall-panel app that runs on this build. For how to set it up, see its
[own documentation](https://kiosksatellite.com/docs/). This page only covers
what is specific to this device.

Tested with Kiosk Satellite 2026.11.23 on a LineageOS 24.0 build of this
repository.

## Which APK to install

This build is `arm64-v8a` only (`adb shell getprop ro.product.cpu.abilist`).
Each Kiosk Satellite release has a CPU-specific APK. Use that one:

| APK | Size | Use it? |
|---|---|---|
| `kiosk-satellite-<version>.arm64-v8a.apk` | about 115 MB | **Yes** |
| `kiosk-satellite-<version>.apk` (universal) | about 255 MB | No: the same app with code for CPU types this device does not have |

```sh
adb install -r kiosk-satellite-<version>.arm64-v8a.apk
```

## WebView

No extra WebView is needed. The build ships Chromium 157
(`com.android.webview`).

## Use the legacy renderer

With the default renderer (Impeller) the app's own screens are janky on the
Adreno 506. In the app, turn on **Settings > Device > Legacy renderer** and
restart the app.

Measured on a CD-18781Y: open and close the app menu 8 times and read
`dumpsys gfxinfo`.

| Setting | Janky frames | Median / 90th percentile frame time |
|---|---|---|
| Default (Impeller) | 30.9% | 22 / 40 ms |
| **Legacy renderer** | **4.2%** | **9 / 13 ms** |
| Legacy WebView renderer only | 37% | 10 / 32 ms |
| Legacy renderer + legacy WebView renderer | 2.2% | 5 / 7 ms |

The legacy WebView renderer on its own is worse, so I would not enable it. I
only measured the app's own screens, not scrolling inside a dashboard.
