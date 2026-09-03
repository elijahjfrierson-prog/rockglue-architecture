# RockGlue Architecture

Zero-latency VST3/AU/Standalone rock mixing plug-in built on JUCE 8. Four
parallel bus nodes sum into a VCA glue compressor. Every stage is
minimum-phase and computed sample-by-sample: no look-ahead, no FFT, no
linear-phase filters, no block delay. `getLatencySamples()` reports 0 and the
test suite proves an impulse leaves at sample 0.

```
Drums   -> Smasher        (1176-style FET, 4:1, 0.05 ms / 50 ms, parallel mix)
Bass    -> Low-End Anchor (Side HPF 130 Hz Butterworth 12 dB/oct; odd-harmonic grit 200-800 Hz on Mid)
Guitars -> Pocket cut     (-2 dB, Q 1.0 @ 2.5 kHz, scaled by Carve Pocket)
Vocals  -> Pocket boost   (+2 dB, Q 1.0 @ 2.5 kHz, scaled by Carve Pocket)
                                        \___ sum ___ VCA Glue (SSL-G style, 2:1, 30 ms, soft knee) ___ out
```

## Routing

The plug-in declares one main stereo input plus three optional stereo inputs.

| Bus | Stereo Mix mode | 4-Bus mode |
| --- | --- | --- |
| 0 `Drums / Mix` | full mix | drums |
| 1 `Bass` | ignored | bass (sidechain input) |
| 2 `Guitars` | ignored | guitars (sidechain input) |
| 3 `Vocals` | ignored | vocals (sidechain input) |

* **Stereo Mix** — the mix runs Anchor → Smasher (as a parallel bus smash) →
  Glue. The Pocket is skipped: a cut and a boost on the same signal cancel.
* **4-Bus** — each input goes through its own node, the four lines sum and hit
  the Glue. Buses the host leaves disconnected are treated as silent.

In hosts with a single sidechain slot, use the Stereo Mix mode or run several
instances; in hosts that expose all aux inputs (Reaper, Bitwig, Logic AU
multi-input), route each stem to its named bus.

## Directory layout

```
CMakeLists.txt                  JUCE (FetchContent 8.0.4), plugin + test targets
Source/
  PluginProcessor.h/.cpp        bus layout, routing, APVTS, state
  PluginEditor.h/.cpp           dark-mode UI, glue meter, footprint visualizer
  Parameters.h                  parameter IDs + APVTS layout
  DSP/                          header-only, JUCE-free
    Biquad.h                    RBJ minimum-phase biquads (TDF-II)
    FetCompressor.h             Smasher
    VcaBusCompressor.h          The VCA Glue
    BassAnchor.h                Low-End Anchor
    PocketEq.h                  The Pocket
    RockGlueEngine.h            node graph + summing + meter frame
tests/
  EngineTests.cpp               plain executable: latency, filters, ratios, meters
  ProcessorTests.cpp            JUCE console app: layouts, routing, state round-trip
.github/workflows/build.yml     macOS / Windows / Linux builds + tests
```

## Parameters (all APVTS, automatable)

| ID | Name | Range | Node |
| --- | --- | --- | --- |
| `inputMode` | Input Mode | Stereo Mix / 4-Bus | routing |
| `drumDrive` | Drum Drive | 0 .. +30 dB | Smasher |
| `drumMix` | Parallel Mix | 0 .. 100 % | Smasher |
| `monoLock` | Mono Lock | on/off | Anchor |
| `grit` | Grit | 0 .. 100 % | Anchor |
| `carvePocket` | Carve Pocket | 0 .. 100 % | Pocket |
| `glueThreshold` | Threshold | -20 .. +10 dB | Glue |
| `glueMakeup` | Makeup Gain | 0 .. +12 dB | Glue |
| `glueAutoRelease` | Auto Release | on/off (off = 100 ms) | Glue |

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

Artefacts land in `build/RockGlue_artefacts/Release/{VST3,AU,Standalone}`.
The DSP core builds without JUCE:
`g++ -O2 -std=c++17 -ISource tests/EngineTests.cpp`.
