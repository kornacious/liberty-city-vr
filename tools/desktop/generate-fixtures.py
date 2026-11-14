#!/usr/bin/env python3
"""Generate redistributable audio fixtures from a synthetic tone; no game data."""
import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("output", type=pathlib.Path)
parser.add_argument("--ffmpeg", default="ffmpeg")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
formats = [("tone.mp3", "libmp3lame", "44100"), ("tone.m4a", "aac", "44100"),
           ("tone.ogg", "libvorbis", "44100"), ("tone.opus", "libopus", "48000"),
           ("tone.flac", "flac", "22050"), ("pcm.wav", "pcm_s16le", "22050"),
           ("ima.wav", "adpcm_ima_wav", "22050"), ("ms.wav", "adpcm_ms", "22050")]
for name, codec, rate in formats:
    subprocess.run([args.ffmpeg, "-v", "error", "-y", "-f", "lavfi", "-i",
                    "sine=frequency=440:duration=1", "-ac", "2", "-ar", rate,
                    "-c:a", codec, "-metadata", "title=Portable fixture",
                    str(args.output / name)], check=True)
(args.output / "radio.adf").write_bytes(bytes(x ^ 0x22 for x in (args.output / "tone.mp3").read_bytes()))
(args.output / "broken.wav").write_bytes(b"RIFF\x00\x00\x00\x00WAVEbad")
