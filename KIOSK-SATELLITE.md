# Using Kiosk Satellite on the ThinkSmart View

[Kiosk Satellite](https://github.com/jxlarrea/kiosk-satellite) is a free
Android app (personal, non-commercial use) that turns an Android device into a
Home Assistant wall panel. It runs on this build. This page lists which APK to
install and which settings made a difference on the ThinkSmart View.

Tested with Kiosk Satellite 2026.11.23 on a LineageOS 24.0 build of this
repository. Other versions should behave the same, but the setting names may
change.

## Which APK

The build is `arm64-v8a` only (`adb shell getprop ro.product.cpu.abilist`).
Each Kiosk Satellite release has several APKs. Pick the CPU-specific one:

| APK | Size | Use it? |
|---|---|---|
| `kiosk-satellite-<version>.arm64-v8a.apk` | about 115 MB | **Yes** |
| `kiosk-satellite-<version>.apk` (universal) | about 255 MB | No: the same app with code for every CPU type |
| `armeabi-v7a`, `x86_64` | | No: wrong CPU |

```sh
adb install -r kiosk-satellite-<version>.arm64-v8a.apk
```

GitHub shows a SHA-256 for each release asset. Compare it with
`sha256sum` before installing.

**No extra WebView is needed.** The build ships Chromium 157
(`com.android.webview`).

## Make it smooth: use the legacy renderer

With the default renderer (Impeller) the app's own screens are janky on this
GPU. In the app: **Settings > Device > Legacy renderer: on**, then restart the
app. It is a real setting, not a workaround for a crash.

Measured on my unit: open and close the app menu 8 times, `dumpsys gfxinfo`:

| Setting | Janky frames | Median / 90th percentile frame time |
|---|---|---|
| Default (Impeller) | 30.9% | 22 / 40 ms |
| **Legacy renderer** | **4.2%** | **9 / 13 ms** |
| Legacy WebView renderer only | 37% | 10 / 32 ms |
| Legacy renderer + legacy WebView renderer | 2.2% | 5 / 7 ms |

I stayed with the legacy renderer alone. Legacy WebView renderer on its own is
worse, and I did not test how the combination behaves when scrolling inside the
dashboard. I only measured the app menu.

## Other settings worth setting

The heavy features (Voice Satellite, music, camera, intercom, Bluetooth proxy,
DLNA, AirPlay) are off by default. Leave them off for a dashboard-only panel.

| Where in the app | Setting | Why |
|---|---|---|
| Kiosk | Start on boot | The panel comes back by itself after a power cut |
| Home Assistant > User Interface | HA kiosk mode | Hides the Home Assistant header and sidebar |
| Home Assistant > Optimizations | Filter dashboard updates, Pause dashboard during screensaver | On by default; keep them on |
| ESPHome | Enable ESPHome | Shows the panel as a device in Home Assistant (screen, restart, sensors) |
| Device > Kiosk Satellite Analytics | Basic analytics, Usage, Diagnostics | On by default; turn off if you prefer |

### Permissions through adb

Start on boot needs "display over other apps". Granting the few permissions
the app needs from adb avoids tapping through dialogs, and nothing here gives
it the camera, microphone, location or storage:

```sh
P=me.jxl.kiosk_satellite
adb shell pm grant $P android.permission.POST_NOTIFICATIONS
adb shell pm grant $P android.permission.ACCESS_LOCAL_NETWORK
adb shell appops set $P SYSTEM_ALERT_WINDOW allow
adb shell appops set $P WRITE_SETTINGS allow        # screen brightness
adb shell cmd deviceidle whitelist +$P              # not stopped in the background
```

### No lock screen

After a reboot the Android lock screen covers the app. A wall panel does not
need it (this only works when no PIN or pattern is set):

```sh
adb shell locksettings set-disabled true
```

### Configure it from your computer

The app can be set up without touching the screen. Turn on the remote
administration once from adb (choose your own password):

```sh
adb shell am start -n me.jxl.kiosk_satellite/.ProvisionActivity \
  --es ks.provision '"{\"remote.enabled\":true,\"remote.password\":\"CHOOSE-A-PASSWORD\"}"'
```

Then `POST /api/login` on port 2324 gives a token, and `PATCH /api/settings`
changes settings. See the
[Remote API documentation](https://kiosksatellite.com/docs/remote-api/).
You can also open `http://<device-ip>:2324` in a browser.

## Home Assistant tips

- The dashboard needs a network that can reach Home Assistant. A guest Wi-Fi on
  its own VLAN usually cannot.
- Logging in on the normal Home Assistant login page works (leave "Keep me
  logged in" ticked, and keep **Log in automatically** off unless you give the
  app a long-lived token).
- The ESPHome connection is encrypted. The app generates the key on first
  start and shows it under **Settings > ESPHome > Encryption key**.
- After a reinstall or a data wipe the app gets a new MAC address. Home
  Assistant then reports a name conflict. Choose **Migrate** to keep the
  existing device and its entities. **Overwrite** deletes them.
