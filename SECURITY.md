# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 0.1.x-alpha | Yes |

## Reporting a vulnerability

**Do not open a public GitHub issue for security problems.**

1. Go to the [Security Advisories](https://github.com/gbroeckling/spectrum-plus/security/advisories) page.
2. Click **Report a vulnerability**.
3. Describe the issue, impact, and steps to reproduce.
4. Expect a response within **7 days**.
5. Credit will be given in the changelog (you may request anonymity).

## What counts as a security issue

- Path traversal or code injection in the build system
- Firmware that could brick the device or damage connected hardware
- Supply-chain risks in third-party dependencies (esp-dsp, HUB75 DMA library)

## What does not count

- Issues requiring physical access to the dev kit (it is a local device)
- Denial of service against a standalone embedded device
- Bugs without security impact

## Security posture

- This firmware has **no network connectivity** — no Wi-Fi, no Bluetooth,
  no OTA, no remote API. Attack surface is limited to the USB serial port
  and the physical I2S / GPIO interfaces.
- No credentials or secrets are stored in the firmware or repository.
- Third-party components are pinned via `idf_component.yml` and fetched
  from Espressif's component registry or verified Git repositories.
