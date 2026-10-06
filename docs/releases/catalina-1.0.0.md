# TargetBridge Catalina fork 1.0.0

The first release of this fork of
[swellweb/targetBridge](https://github.com/swellweb/targetBridge), based on
TargetBridge 3.5.2. Both apps report version 3.5.2, the upstream version they
are built from. Install both from this release.

## Receiver for macOS 10.15 Catalina

- New Intel Receiver build, `TargetBridge-Receiver-catalina-x86_64.app.zip`,
  that runs on macOS 10.15.7. Upstream's Receiver needs macOS 11.
- FFmpeg (decode-only) and SDL2 are built from source for macOS 10.15 and
  linked into the app, so it depends only on system libraries.
- Fix a launch crash on Catalina (`Symbol not found:
  _CGPreflightListenEventAccess`). The SDK declares that function for 10.15,
  but Catalina does not ship it. The Receiver now looks it up at runtime and
  uses `IOHIDCheckAccess` on Catalina.
- Advertise HEVC only when the Mac decodes it in hardware. 2012–2014 iMacs
  decode HEVC on the CPU, so the Sender now gives them H.264.

## Ethernet / USB built into the Sender

- The former experimental `Network Link` add-on is now a built-in transport
  called **Ethernet / USB**.
- A direct cable between the Macs (Ethernet, or a USB link that macOS turns
  into a network interface) now shows up in the Sender's interface list,
  first. Before, only `targetbridge connect --path` could use those links,
  because both ends self-assign `169.254.x.x`.
- The Sender connects to the Receiver's address on the chosen interface and
  keeps direct-cable connections off Wi-Fi.

## Build checks

The `Catalina Fork Build` workflow fails if the Receiver:

- targets a macOS newer than 10.15;
- links a non-system library;
- calls an API newer than 10.15 without a runtime check;
- imports a symbol that macOS 10.15 does not export. This is checked against
  the 10.15 SDK's exported-symbol lists and catches the kind of crash above.

## Tested

| Receiver | Connection | Result |
| --- | --- | --- |
| iMac14,2 (27-inch, Late 2013, GeForce GTX 780M), macOS 10.15.7 | Cat5e Ethernet cable | Works; smooth, no noticeable lag |

Thunderbolt Bridge and USB-C to USB-A have not been tested with this release
yet.

## Downloads

- `TargetBridge-Receiver-catalina-x86_64.app.zip`: Receiver for the iMac
  (macOS 10.15 or later, Intel).
- `TargetBridge-arm64.app.zip`: Sender for Apple Silicon Macs (macOS 14+).
- `TargetBridge-x86_64.app.zip`: Sender for Intel Macs (macOS 14+).

The apps are not signed. After unzipping, run `xattr -cr` on each app before
the first launch.
