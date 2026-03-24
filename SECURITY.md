# Security / Secrets

This repo is designed to be safe to publish.

## What was removed

- Wi‑Fi SSID/password
- ESPHome API encryption key
- OTA password
- Fallback AP password

## How to use your own values

Create `esphome/secrets.yaml` (not committed) based on `esphome/secrets.example.yaml`,
and ESPHome will substitute `!secret` values at compile time.

Never commit `secrets.yaml`.
