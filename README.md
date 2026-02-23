# manalysis

A fast CLI tool for analyzing audio tracks. Outputs **BPM**, **musical key** (with Camelot wheel notation), and **gain** (LUFS + ReplayGain).

Built as a standalone project using the same analysis libraries as [Mixxx](https://mixxx.org/) — no Qt, no Mixxx build required.

## Example output

```
track.mp3   BPM: 147.02  Key: D minor    ( 7A)  LUFS:   -9.88  RG: -8.12 dB
```

Multiple files are processed sequentially:

```
$ manalysis ~/Music/*.mp3
~/Music/track1.mp3  BPM: 157.27  Key: F minor    ( 4A)  LUFS:   -9.10  RG: -8.90 dB
~/Music/track2.mp3  BPM: 147.02  Key: D minor    ( 7A)  LUFS:   -9.88  RG: -8.12 dB
~/Music/track3.mp3  BPM:  92.00  Key: G major    ( 9B)  LUFS:  -14.22  RG:  -3.78 dB
```

## Analysis methods

| Feature | Library | Notes |
|---------|---------|-------|
| BPM | [SoundTouch](https://www.surina.net/soundtouch/) `BPMDetect` | Mono downmix, normalized to [60–200] range |
| Key | [libkeyfinder](https://github.com/ibsh/libKeyFinder) | Stereo, outputs key name + Camelot code |
| Gain | [libebur128](https://github.com/jiixyj/libebur128) | EBU R128 integrated loudness, RG2.0 reference −18 LUFS |
| Decoding | FFmpeg (libavcodec/libavformat) | Supports MP3, FLAC, WAV, OGG, AAC, AIFF, and more |

## Dependencies

All available via pacman on Arch Linux:

```bash
sudo pacman -S soundtouch libkeyfinder libebur128 ffmpeg cmake
```

On Debian/Ubuntu:

```bash
sudo apt install libsoundtouch-dev libkeyfinder-dev libebur128-dev \
     libavcodec-dev libavformat-dev libavutil-dev libswresample-dev \
     cmake build-essential
```

## Build

```bash
cmake -B build -S .
cmake --build build
```

The binary is `build/manalysis`. Optionally install system-wide:

```bash
cmake --install build --prefix ~/.local
```

## Usage

```bash
manalysis <file> [file ...]
manalysis --help
```

Exit code is 0 if all files were analyzed successfully, 1 if any failed.

## Project structure

```
src/
  AudioDecoder.h/cpp   FFmpeg-based decoder → float32 stereo chunks
  BpmAnalyzer.h/cpp    SoundTouch BPMDetect wrapper
  KeyAnalyzer.h/cpp    libkeyfinder wrapper (key name + Camelot)
  GainAnalyzer.h/cpp   libebur128 wrapper (LUFS + ReplayGain)
  main.cpp             CLI entry point
```

## Camelot wheel

Keys are displayed with their [Camelot wheel](https://mixxx.org/wiki/doku.php/camelot_key_wheel) code for harmonic mixing. Major keys use the `B` suffix, minor keys use `A`.
