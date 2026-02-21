# onebitdelay-ugen

Self-contained SuperCollider UGen port of the `onebitdelay` plugin DSP from this repo.

- Vendorized DSP core lives in `src/PT2399.h`.
- JUCE dependencies were removed and replaced with C++17 DSP utilities.
- All runtime parameters from `onebitdelay` are exposed as UGen inputs.
- `oversample` is init-rate in behavior: read once at UGen construction.

## Layout

- `src/PT2399.h` - vendored PT2399/ONE-BIT DELAY DSP core (JUCE-free)
- `src/PT2399.cpp` - SuperCollider server plugin entry + UGen
- `Classes/PT2399.sc` - sclang class wrapper
- `HelpSource/Classes/PT2399.schelp` - help
- `PT2399_test.scd` - smoke test script (boots server, plays ExampleFiles buffer)

## Build

```bash
cd onebitdelay-ugen
make build
```

This configures and builds the plugin with CMake.

`CMakeLists.txt` tries local SuperCollider headers first. If not found, it fetches:

- `https://github.com/supercollider/supercollider`
- tag `Version-3.14.1`

## Install

```bash
cd onebitdelay-ugen
make install
```

Installs to:

- `~/.local/share/SuperCollider/Extensions/PT2399UGen/plugins/PT2399UGens.so`
- `~/.local/share/SuperCollider/Extensions/PT2399UGen/Classes/PT2399.sc`
- `~/.local/share/SuperCollider/Extensions/PT2399UGen/HelpSource/Classes/PT2399.schelp`

## Test

```bash
cd onebitdelay-ugen
make test
```

Runs build + install + `sclang PT2399_test.scd`.

## Parameter Reference

`PT2399.ar(in, inputLevel, dryLevel, wetLevel, delayTime, feedbackHpf, feedback, c3, c6, brightness, boostActivated, passthrough, oversample)`

- `in` - audio input
- `inputLevel` (dB)
- `dryLevel` (dB)
- `wetLevel` (dB)
- `delayTime` (ms) - delay time in milliseconds (internally mapped to PT2399 pin 6 resistance)
- `feedbackHpf` (Hz) - feedback high-pass filter
- `feedback` (0..2)
- `c3` (nF)
- `c6` (nF)
- `brightness` (0..100 percent)
- `boostActivated` (>=0.5 true)
- `passthrough` (>=0.5 true)
- `oversample` - constructor-time index only:
  - `0 -> 1x`
  - `1 -> 2x`
  - `3 -> 4x`
  - `4 -> 8x`
  - `5 -> 16x`
  - `6 -> 32x`
  - any other value defaults to `16x`

## Example

```supercollider
{
    var src = Saw.ar(110, 0.05);
    var fx = PT2399.ar(
        src,
        inputLevel: 0,
        dryLevel: 0,
        wetLevel: -3,
        delayTime: 144.3,
        feedbackHpf: 80,
        feedback: 0.35,
        c3: 100,
        c6: 100,
        brightness: 20,
        boostActivated: 0,
        passthrough: 0,
        oversample: 5
    );
    fx ! 2
}.play;
```
