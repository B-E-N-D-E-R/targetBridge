# Receiver on macOS 10.15 Catalina

Late 2012 to 2015 iMacs top out at macOS 10.15.7 without patching. The
Receiver's own code only uses APIs that exist on Catalina; what kept it on
macOS 11 was the build: the release links Homebrew's FFmpeg and SDL2, which are
compiled for the macOS release of the CI runner, and the Info.plist declared
`LSMinimumSystemVersion` 11.0.

The `Receiver for macOS 10.15 Catalina` workflow
(`.github/workflows/receiver-catalina.yml`) fixes that by:

1. building a decode-only FFmpeg (H.264/HEVC + VideoToolbox) and SDL2 from
   source with `MACOSX_DEPLOYMENT_TARGET=10.15`, as static libraries
   (`TargetBridge-Receiver/scripts/build_legacy_deps.sh`);
2. linking the Receiver against them, so the app bundle depends only on
   system libraries;
3. compiling everything with `-Werror=unguarded-availability-new`, so any call
   to an API newer than 10.15 without a runtime check fails the build instead
   of crashing on the iMac;
4. checking the executable's `minos`, the Info.plist minimum, and the linked
   libraries before uploading `TargetBridge-Receiver-catalina-x86_64.app.zip`.

## Getting the app

- Actions tab → **Receiver for macOS 10.15 Catalina** → **Run workflow**, then
  download the artifact from the finished run; or
- push a tag named `catalina-v<version>` to get a draft release.

On the iMac, unzip, then clear the quarantine flag before the first launch:

```bash
xattr -cr ~/Downloads/"TargetBridge Receiver.app"
```

## Pairing

Only the Receiver changes. The Sender is the unmodified TargetBridge app and
still needs macOS 14 Sonoma or later on the Mac that drives the iMac. Connect
the two with a Thunderbolt cable (Thunderbolt 2 iMacs need Apple's Thunderbolt
3 to Thunderbolt 2 adapter) and both Macs get a Thunderbolt Bridge interface,
which Catalina supports.

## Expectations on 2012–2013 iMacs

- These Macs hardware-decode H.264 but not HEVC. VideoToolbox would still
  accept HEVC and decode it on the CPU, so this fork makes the Receiver
  advertise HEVC only when `VTIsHardwareDecodeSupported` reports hardware
  support; the Sender then picks H.264.
- The Receiver advertises its real panel size (2560×1440 on a 27" iMac), so
  stream at that resolution rather than a 5K profile.
