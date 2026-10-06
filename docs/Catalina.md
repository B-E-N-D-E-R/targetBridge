# Receiver on macOS 10.15 Catalina

Late 2012 to 2015 iMacs top out at macOS 10.15.7 without patching. The
Receiver's own code only uses APIs that exist on Catalina; what kept it on
macOS 11 was the build: the release links Homebrew's FFmpeg and SDL2, which are
compiled for the macOS release of the CI runner, and the Info.plist declared
`LSMinimumSystemVersion` 11.0.

The `Catalina Fork Build` workflow (`.github/workflows/catalina-fork.yml`)
fixes that by:

1. building a decode-only FFmpeg (H.264/HEVC + VideoToolbox) and SDL2 from
   source with `MACOSX_DEPLOYMENT_TARGET=10.15`, as static libraries
   (`TargetBridge-Receiver/scripts/build_legacy_deps.sh`);
2. linking the Receiver against them, so the app bundle depends only on
   system libraries;
3. compiling everything with `-Werror=unguarded-availability-new`, so any call
   to an API newer than 10.15 without a runtime check fails the build instead
   of crashing on the iMac;
4. checking the executable's `minos`, the Info.plist minimum, and the linked
   libraries;
5. checking every symbol the Receiver imports against the macOS 10.15 SDK's
   exported-symbol stubs (`TargetBridge-Receiver/scripts/check_sdk_symbols.py`).
   The current SDK declares some functions, such as
   `CGPreflightListenEventAccess`, as available on 10.15 although Catalina
   does not export them, and step 3 cannot catch that. The Receiver looks that
   one up at runtime and falls back to `IOHIDCheckAccess` on Catalina.

The result is uploaded as `TargetBridge-Receiver-catalina-x86_64.app.zip`.

The same workflow also builds this fork's Sender (`TargetBridge-arm64.app.zip`
and `TargetBridge-x86_64.app.zip`), which has Ethernet / USB built in. See
[Connecting the Macs](#connecting-the-macs).

## Getting the apps

- Actions tab → **Catalina Fork Build** → **Run workflow**, then download the
  artifacts from the finished run; or
- push a tag named `catalina-v<version>` to get a draft release with all three
  zips.

On the iMac, unzip, then clear the quarantine flag before the first launch:

```bash
xattr -cr ~/Downloads/"TargetBridge Receiver.app"
```

## Connecting the Macs

The Sender still needs macOS 14 Sonoma or later on the Mac that drives the
iMac; use this fork's Sender build. In the Sender, pick the transport for the
session:

| Cable | Transport | Notes |
| --- | --- | --- |
| Thunderbolt (Thunderbolt 2 iMacs need Apple's Thunderbolt 3 to Thunderbolt 2 adapter) | Thunderbolt Bridge | Lowest latency. |
| Ethernet cable straight between the Macs | Ethernet / USB | Both Macs self-assign a `169.254.x.x` address. Newer Macs need a USB-C Ethernet adapter. |
| Both Macs on the same router or switch | Ethernet / USB | Uses the LAN addresses. Wired is much steadier than Wi-Fi. |
| USB-C (newer Mac) to USB-A (iMac) data cable | Ethernet / USB | See below. |

Ethernet / USB is built into this fork's Sender; it is no longer the
experimental `Network Link` add-on and cannot be switched off. When a direct
cable is plugged in, the Sender lists that interface (`enX · 169.254.x.x`)
first and dials the Receiver's matching link-local address.

### USB-C to USB-A

TargetBridge does not drive USB itself; it streams over whatever network
interface macOS creates. A USB cable works only if, once plugged in, a new
network interface with a `169.254.x.x` address appears in System Preferences →
Network on **both** Macs (the Receiver shows it as its `USB` address). If one
appears, select Ethernet / USB and that interface in the Sender. If none
appears, macOS is not networking over that USB connection and you need
Ethernet or Thunderbolt instead.

If you try this, use a 10 Gbps data cable (USB 3.1/3.2 Gen 2), not a charging
cable. The 2012–2015 iMacs' USB-A ports are USB 3.0, so expect at most about
3 Gbps of real throughput. That is enough for 2560×1440.

The Sender's `targetbridge connect --path auto` measures every working route
(Thunderbolt, direct USB/Ethernet, LAN, Wi-Fi) and picks the fastest; see
[Automation](Automation.md#1-targetbridge-cli).

## Expectations on 2012–2013 iMacs

- These Macs hardware-decode H.264 but not HEVC. VideoToolbox would still
  accept HEVC and decode it on the CPU, so this fork makes the Receiver
  advertise HEVC only when `VTIsHardwareDecodeSupported` reports hardware
  support; the Sender then picks H.264.
- The Receiver advertises its real panel size (2560×1440 on a 27" iMac), so
  stream at that resolution rather than a 5K profile.
