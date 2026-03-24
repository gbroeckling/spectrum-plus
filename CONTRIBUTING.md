# Contributing

Thanks for wanting to improve Spectrum Plus.

## Reporting bugs

Open a [Bug Report](https://github.com/gbroeckling/spectrum-plus/issues/new?template=bug_report.yml) with:

- ESP-IDF version
- Waveshare board revision (if known)
- HUB75 panel model / shift driver
- Serial monitor logs

## Requesting features

Open a [Feature Request](https://github.com/gbroeckling/spectrum-plus/issues/new?template=feature_request.yml)
describing your use case and motivation.

## Good PR ideas

- Verified pin mappings for other ESP32-P4 boards
- Alternate color palettes or visual modes
- Enclosure designs (laser-cut / 3D print)
- Support for other HUB75 panel sizes or scan rates
- Performance optimizations for the DSP pipeline

## Code style

- **C++:** Follow ESP-IDF conventions. Use `ESP_LOGI` / `ESP_LOGE` for logging.
  Keep functions short. Comment non-obvious DSP math.
- **Pin assignments:** All GPIOs go in `pin_config.h` — never hard-code a pin
  number in other files.

## How to submit

1. Fork the repo
2. Create a branch: `feature/your-change`
3. Build and test on real hardware: `idf.py build && idf.py flash monitor`
4. Commit with clear messages
5. Open a PR describing what changed and how to test it

## Development philosophy

Built by **Garry Broeckling**. Implementation is AI-assisted using Claude by
Anthropic — all architecture, product decisions, testing, and releases are
human-directed. Contributors are welcome to use any tools they prefer.
