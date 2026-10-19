# Build App Title 0.2.8

Requires Linux/POSIX, LLVM/Clang 18, LLD 18, make, Python 3 and PS5 Payload SDK **v0.41**.

Download the SDK from its official release:

https://github.com/ps5-payload-dev/sdk/releases/tag/v0.41

Extract the SDK outside this source directory, then run:

```bash
export PS5_PAYLOAD_SDK=/absolute/path/ps5-payload-sdk
make -j4
```

The only file needed on the console is `dist/App_Title_v0.2.8.elf`.
The shared object in `build/` is automatically embedded into the controller.
No Actions job or external repository mutation is needed to build.

All required application sources, rendering headers, disassembler and adapted
loader sources are included. The SDK/compiler binaries are not included.

Host checks:

```bash
make host-test
make media-test
make hooks-test
make shellui-test
make runtime-test
make style-test
make export-test
make home-style-test
python3 -m pip install Pillow
npm install --prefix build-ui --no-save jsdom@26.1.0 yoga-layout@2.0.1
python3 tests/test_host.py
python3 tools/verify_elf.py
```

The legacy rendering check (source-only, not the native runtime) uses `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` on
the host. The console uses its own SST font; Sony font files are not bundled.
The DOM tests exercise the actual HTML and HTTP service with jsdom and real
XHR. They do not simulate PS5 WebKit, GPU caches or native controller events.

The build uses `prospero-clang++` and links the SDK's payload runtime. Do not
replace the controller with a Linux-compiled executable, and do not send the
internal shared object as a second standalone payload.

Only the shared component links to the SDK stubs for libSceRegMgr and libmonosgen-2.0. The controller resolves their exports in the existing ShellUI process. Registry and Application.Update callbacks are required. An additional ioctl callback styles the verified HomeUI after successful RNPS decryption; unsupported style prologues preserve the required pair.

The controller must not have libSceRegMgr or libmonosgen in DT_NEEDED. The SDK processes this list before main. Run tools/verify_elf.py after linking; its dependency allowlist is a startup regression check.

Optional fixture and real Hermes VM checks (Sony input and Hermes tools are external):

```bash
make home-style-test shellui-test HOME_UI=/absolute/path/HomeUI_decrypted.bin
python3 tools/verify_style_vm.py --hermes /absolute/path/hermes-v0.12.0/hermes
python3 tools/verify_tile_vm.py --hermes /absolute/path/hermes-v0.12.0/hermes
python3 tools/verify_geometry_vm.py --hermes /absolute/path/hermes-v0.12.0/hermes
python3 tools/verify_focus_vm.py --hermes /absolute/path/hermes-v0.12.0/hermes --home-ui /absolute/path/HomeUI_decrypted.bin
python3 tools/verify_render_bytecode.py --hermes-dec /absolute/path/hermes-dec
python3 tools/verify_inside_baseline.py
python3 tools/verify_v027_layout.py --home-ui /absolute/path/HomeUI_decrypted.bin
node tests/test_outside_layout.js
```

The fixture test writes eight HBC files: outside modes 0–3 and inside modes 4–7,
covering bottom/top and white/custom colour. Styles, both Tile renderer branches,
and geometry/animation/row/game-title builders execute in the official Hermes v0.12.0 CLI.
Native components, React hooks and animation values are mocked. Builder tests
cover three input sizes, 96 cases, twice each. The unchanged parent renderer is
compared with its original function in 64 scenarios. The independent parser
checks seven functions, register/branch boundaries, Identifier properties and
inline caches. No external parser implementation or Sony input is packaged.

The inside baseline check compares entire HBC SHA256 digests against outputs
built from the released 0.2.3 source with the same user fixture. Yoga uses actual
captured styles and trees to check positive text bounds, bitmap separation and
selected-scale bounds in the enlarged row. It models three text measurements,
both clip settings and four outside modes; it does not implement RNPS native
clipping, font rasterization or all scene animations. Host tests do not replace
visual validation on the console.

The 0.2.8 layout check restores only the original game-title function in each
outside fixture, then compares its complete HBC digest with released 0.2.7.
This verifies that working ID rendering is unchanged. Game-title VM checks
verify TITLE_Y and top=214 outside, top=106 inside, unchanged horizontal and
minimized offsets, and the original 62-point line container.
