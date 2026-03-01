"""Subprocess wrapper that locates and calls the bundled mixxx-analyzer binary."""

import json
import os
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional


@dataclass
class AnalysisResult:
    """Result of analyzing a single audio file.

    Attributes:
        file: Path to the analyzed file.
        bpm: Detected BPM, or None if undetected.
        key: Musical key (e.g. "A minor").
        camelot: Camelot wheel notation (e.g. "8A").
        lufs: Integrated loudness in LUFS.
        replay_gain: ReplayGain adjustment in dB.
        intro_secs: Estimated intro end timestamp in seconds.
        outro_secs: Estimated outro start timestamp in seconds.
        tags: Embedded metadata tags extracted from the container
              (keys: title, artist, album, year, genre, label,
               comment, trackNumber, bpmTag). Empty strings for
               absent tags.
        beatgrid: Beat positions in seconds (from the Queen Mary
                  tempo tracker). Empty list if BPM was undetected.
    """

    file: str
    bpm: Optional[float]
    key: str
    camelot: str
    lufs: float
    replay_gain: float
    intro_secs: float
    outro_secs: float
    tags: Dict[str, str] = field(default_factory=dict)
    beatgrid: List[float] = field(default_factory=list)

    @classmethod
    def from_dict(cls, d: dict) -> "AnalysisResult":
        tags = {
            "title": d.get("title", ""),
            "artist": d.get("artist", ""),
            "album": d.get("album", ""),
            "year": d.get("year", ""),
            "genre": d.get("genre", ""),
            "label": d.get("label", ""),
            "comment": d.get("comment", ""),
            "trackNumber": d.get("trackNumber", ""),
            "bpmTag": d.get("bpmTag", ""),
        }
        return cls(
            file=d["file"],
            bpm=d.get("bpm"),
            key=d["key"],
            camelot=d["camelot"],
            lufs=d["lufs"],
            replay_gain=d["replayGain"],
            intro_secs=d["introSecs"],
            outro_secs=d["outroSecs"],
            tags=tags,
            beatgrid=d.get("beatgrid", []),
        )


def _find_binary() -> str:
    """Return path to the mixxx-analyzer binary (bundled or on PATH)."""
    suffix = ".exe" if sys.platform == "win32" else ""
    bundled = Path(__file__).parent / "bin" / f"mixxx-analyzer{suffix}"
    if bundled.exists() and os.access(bundled, os.X_OK):
        return str(bundled)

    import shutil

    on_path = shutil.which("mixxx-analyzer")
    if on_path:
        return on_path

    raise FileNotFoundError(
        "mixxx-analyzer binary not found. "
        "Install the platform-specific mixxx-analyzer wheel or ensure "
        "the binary is on PATH."
    )


def analyze(path: str) -> AnalysisResult:
    """Analyze a single audio file.

    Returns an AnalysisResult with BPM, key, Camelot notation,
    LUFS loudness, ReplayGain, intro/outro timestamps, embedded
    metadata tags, and a full beatgrid (beat positions in seconds).

    Raises subprocess.CalledProcessError if the binary fails.
    Raises FileNotFoundError if the binary is not installed.
    """
    return analyze_many([path])[0]


def analyze_many(paths: List[str]) -> List[AnalysisResult]:
    """Analyze multiple audio files in a single binary invocation.

    More efficient than calling analyze() in a loop for large batches.
    """
    binary = _find_binary()
    proc = subprocess.run(
        [binary, "--json"] + list(paths),
        capture_output=True,
        text=True,
        check=True,
    )
    return [AnalysisResult.from_dict(d) for d in json.loads(proc.stdout)]
