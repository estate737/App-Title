# Validation: App_Title 0.2.8

Firmware 11.60 / rnps-home 11.1.0+21887 / PS5 11.20 HOMEUI / Hermes v89.
The user confirmed the outside-ID layout and requested publication after the
native-title spacing adjustment in 0.2.8.

- Eight inside/outside, top/bottom, white/custom patch combinations passed.
- Fifteen malformed or modified profile cases were rejected without writing.
- Official Hermes v0.12.0: 96 geometry/animation/row/game-title cases passed;
  each function was executed twice to exercise warm inline caches.
- Independent bytecode parsing checked 56 functions, register/cache limits,
  instruction boundaries, branch targets and property identifiers.
- Restoring only the original game-title function reproduces the complete
  0.2.7 outside-layout HBC hashes in all four variants.
- All four inside-mode results match the original working 0.2.3 output.
- Yoga layout: 24 scenarios passed, including positive label bounds, bitmap
  separation, selected scale bounds and the 12-point native-title gap.
- AddressSanitizer and UndefinedBehaviorSanitizer passed; LeakSanitizer was
  disabled because the host environment does not support its /proc workflow.
- PS5 Payload SDK v0.41 / Clang+LLD 18 build passed without warnings/errors.
- ELF checks verified minimal controller dependencies and exact embedding of
  the ShellUI shared component.

Host checks do not implement the proprietary RNPS renderer or prove support
for untested firmware. The reference system input is not distributed.
