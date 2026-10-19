# Changelog

## 0.2.9 — 4.03 test build

- Add the exact 4.03 HomeUI JavaScript Indexed RAM bundle profile alongside the existing 11.60 Hermes profile.
- Preserve top/bottom, inside/outside, preset/custom colour settings and white/bottom/outside defaults.
- Keep RAM module offsets, table lengths, startup code, launch handlers and the outer RNPS header/trailer unchanged.
- Recognize RAM bundles in the manually requested diagnostic export.
- Add local fixture, JavaScript execution and layout checks. No console validation of 0.2.9 is claimed.
- Preserve the eight 11.60 HomeUI patch outputs byte for byte.

This build does not claim to fix the separately reported launch failure after several hours.

## 0.2.8

Initial public open-source release.

- Standalone ELF with title IDs for games and applications.
- Above/below image placement with reserved space outside the artwork.
- Optional inside-image placement.
- Eight preset text colours and custom HEX colours.
- Persistent Media settings shortcut with D-pad, Cross and OPTIONS controls.
- Saved settings and autoload support.
- White IDs below the image by default.
- Native game-title and platform row moved below the ID row to prevent overlap.
- Bounded startup logs without continuous text logging.
- Verified HomeUI profile on firmware 11.60.

The outside-ID layout was confirmed working in 0.2.7. Version 0.2.8 keeps that layout and adjusts the native game-title row.

