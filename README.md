# mixxx-analyzer

[![PyPI](https://img.shields.io/pypi/v/mixxx-analyzer)](https://pypi.org/project/mixxx-analyzer/)
[![PyPI - Python Version](https://img.shields.io/pypi/pyversions/mixxx-analyzer)](https://pypi.org/project/mixxx-analyzer/)
[![CI](https://github.com/Radexito/mixxx-analyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/Radexito/mixxx-analyzer/actions/workflows/ci.yml)
[![clang-format](https://img.shields.io/badge/code%20style-clang--format-blue)](https://clang.llvm.org/docs/ClangFormat.html)

A fast CLI tool for analyzing audio tracks. Outputs **BPM**, **musical key** (with Camelot wheel notation), **gain** (LUFS + ReplayGain), and **intro/outro timestamps** (first/last non-silent frame).

Built as a standalone project using the **exact same analysis code** as [Mixxx](https://mixxx.org/) — no Qt, no Mixxx build required. The BPM and key detection are direct ports of Mixxx's `AnalyzerQueenMaryBeats` and `AnalyzerQueenMaryKey`, using the [qm-dsp](https://github.com/c4dm/qm-dsp) library copied into the repo.

## Python usage

Install from PyPI (includes the native binary — no separate install needed):

```bash
pip install mixxx-analyzer
```

```python
from mixxx_analyzer import analyze, analyze_many

# Single file
result = analyze("/path/to/track.mp3")
print(f"BPM:        {result.bpm:.1f}")
print(f"Key:        {result.key}  ({result.camelot})")
print(f"LUFS:       {result.lufs:.1f}")
print(f"ReplayGain: {result.replay_gain:.2f} dB")
print(f"Intro:      {result.intro_secs:.1f}s")
print(f"Outro:      {result.outro_secs:.1f}s")

# Multiple files at once (single binary invocation — more efficient)
results = analyze_many(["/path/to/a.mp3", "/path/to/b.flac"])
for r in results:
    print(f"{r.file}: {r.bpm:.1f} BPM, {r.camelot}")
```

### `AnalysisResult` fields

| Field | Type | Description |
|-------|------|-------------|
| `file` | `str` | Path to the analyzed file |
| `bpm` | `float \| None` | Detected BPM (None if detection failed) |
| `key` | `str` | Musical key, e.g. `"D minor"` |
| `camelot` | `str` | Camelot wheel code, e.g. `"7A"` |
| `lufs` | `float` | Integrated loudness in LUFS |
| `replay_gain` | `float` | ReplayGain 2.0 adjustment in dB |
| `intro_secs` | `float` | First non-silent frame in seconds |
| `outro_secs` | `float` | Last non-silent frame in seconds |

## Example output

```
track.mp3   BPM: 147.00  Key: D minor    ( 7A)  LUFS:   -9.88  RG: -8.12 dB  Intro: 0:00.04  Outro: 6:12.90
```

Multiple files are processed sequentially:

```
$ mixxx-analyzer ~/Music/*.mp3
~/Music/track1.mp3  BPM: 157.00  Key: F minor    ( 4A)  LUFS:   -9.10  RG: -8.90 dB  Intro: 0:00.00  Outro: 5:48.12
~/Music/track2.mp3  BPM: 147.00  Key: D minor    ( 7A)  LUFS:   -9.88  RG: -8.12 dB  Intro: 0:00.04  Outro: 6:12.90
~/Music/track3.mp3  BPM:  92.00  Key: G major    ( 9B)  LUFS:  -14.22  RG:  -3.78 dB  Intro: 0:00.00  Outro: 4:20.55
```

## Analysis methods

| Feature | Library / Code | Notes |
|---------|---------------|-------|
| BPM | qm-dsp `TempoTrackV2` (port of Mixxx `AnalyzerQueenMaryBeats`) | Mono downmix, windowed onset detection, const-region BPM extraction |
| Key | qm-dsp `GetKeyMode` (port of Mixxx `AnalyzerQueenMaryKey`) | Chromagram + HPCP + key profile correlation, outputs key name + Camelot code |
| Gain | [libebur128](https://github.com/jiixyj/libebur128) | EBU R128 integrated loudness, ReplayGain 2.0 reference −18 LUFS |
| Intro/Outro | Port of Mixxx `AnalyzerSilence` | First/last frame above −60 dB threshold (0.001f), same as Mixxx |
| Decoding | FFmpeg (libavcodec/libavformat) | Supports MP3, FLAC, WAV, OGG, AAC, AIFF, and more |

Results match Mixxx's analysis output. qm-dsp sources are vendored in `third_party/qm-dsp/`.

## Dependencies

On Arch Linux:

```bash
sudo pacman -S libebur128 ffmpeg cmake gtest
```

On Debian/Ubuntu:

```bash
sudo apt install libebur128-dev \
     libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
     libgtest-dev cmake build-essential
```

## Build

```bash
cmake -B build -S .
cmake --build build
```

The binary is `build/mixxx-analyzer`. Optionally install system-wide:

```bash
cmake --install build --prefix ~/.local
```

## Usage

```bash
mixxx-analyzer <file> [file ...]
mixxx-analyzer --help
```

Exit code is 0 if all files were analyzed successfully, 1 if any failed.

## Tests

Integration tests verify BPM and key results against Mixxx-detected reference values for 11 Audionautix CC BY 4.0 tracks:

```bash
build/mixxx-analyzer-test
```

## Project structure

```
src/
  AudioDecoder.h/cpp        FFmpeg-based decoder → float32 stereo chunks
  BpmAnalyzer.h/cpp         Thin wrapper selecting the QM BPM analyzer
  KeyAnalyzer.h/cpp         Thin wrapper selecting the QM key analyzer
  QmBpmAnalyzer.h/cpp       Port of Mixxx AnalyzerQueenMaryBeats (qm-dsp TempoTrackV2)
  QmKeyAnalyzer.h/cpp       Port of Mixxx AnalyzerQueenMaryKey (qm-dsp GetKeyMode)
  GainAnalyzer.h/cpp        libebur128 wrapper (LUFS + ReplayGain)
  SilenceAnalyzer.h/cpp     Port of Mixxx AnalyzerSilence (intro/outro detection)
  DownmixAndOverlapHelper.h/cpp  Port of Mixxx buffering_utils (windowed feeding)
  main.cpp                  CLI entry point (text + --json output)
third_party/
  qm-dsp/                   Queen Mary DSP library (vendored subset)
tests/
  analysis_test.cpp         11 parameterized integration tests (Audionautix CC BY 4.0 tracks)
  download_assets.sh        Downloads test audio assets
python/
  mixxx_analyzer/           Python package
    __init__.py             Public API: analyze(), analyze_many(), AnalysisResult
    _runner.py              Subprocess wrapper around bundled binary
    bin/                    Bundled native binary + shared libraries (platform-specific)
  pyproject.toml            Package metadata
  setup.py                  Custom bdist_wheel for py3-none-<platform> tag
scripts/
  bundle_linux.sh           ldd + patchelf dep bundler (RPATH=$ORIGIN)
  bundle_windows.ps1        PowerShell DLL dep collector
```

## Camelot wheel

Keys are displayed with their [Camelot wheel](https://mixxx.org/wiki/doku.php/camelot_key_wheel) code for harmonic mixing. Major keys use the `B` suffix, minor keys use `A`.
