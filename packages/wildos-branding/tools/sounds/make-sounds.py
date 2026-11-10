#!/usr/bin/env python3
"""Synthesises the Wild OS sound theme from scratch.

    python3 make-sounds.py OUT_DIR

Deep, warm and restrained: one voice (mostly sine and triangle, a touch of
saw, through a low-pass filter that breathes as each note starts), a low
register, D minor 9 harmony, no slides or effects for their own sake. The
signature is the W (down, up, down, up), which the start-up states in full
and the other sounds borrow a step of. The lowest notes carry their
overtones so their depth survives a laptop's small speakers.

Writes OUT_DIR/stereo/<name>.oga (Ogg Vorbis, 48 kHz stereo) for every name
in the freedesktop sound naming spec that Plasma's notifications ask for.
Names that share a sound are links, made by the package (debian/links).
Needs numpy, scipy and ffmpeg; the build machine has none of them, so the
rendered files are kept in the repository and rebuilt by hand.
"""

import os
import subprocess
import sys
import wave

import numpy as np
from scipy.signal import butter, fftconvolve, sosfilt

RATE = 48000


def T(s):
    return np.arange(int(s * RATE)) / RATE


def bl_osc(freq_curve, shape):
    """Band-limited oscillator following a per-sample frequency curve.
    shape: 'saw', 'tri', 'sine'. Built from harmonics up to 12 kHz."""
    phase = 2 * np.pi * np.cumsum(freq_curve) / RATE
    f0 = float(np.max(freq_curve))
    out = np.zeros_like(phase)
    if shape == "sine":
        return np.sin(phase)
    k = 1
    while f0 * k < 12000 and k < 40:
        if shape == "saw":
            out += np.sin(k * phase) / k
        elif shape == "square" and k % 2:
            out += np.sin(k * phase) / k
        elif shape == "tri" and k % 2:
            out += ((-1) ** ((k - 1) // 2)) * np.sin(k * phase) / (k * k)
        k += 1
    return out / (0.9 if shape == "saw" else 0.8)


def svf_lowpass(x, cutoff_curve, q=0.8):
    """Chamberlin state-variable low-pass with a cutoff that moves sample by
    sample, so the filter really sweeps (the 'bloom' of a synth note)."""
    low = band = 0.0
    out = np.empty_like(x)
    damp = 1.0 / q
    fc = np.clip(cutoff_curve, 20, RATE / 6)
    f = 2 * np.sin(np.pi * fc / RATE)
    for i in range(len(x)):
        low += f[i] * band
        high = x[i] - low - damp * band
        band += f[i] * high
        out[i] = low
    return out


rng = np.random.default_rng(1971)
N = {"D1": 36.71, "A1": 55.0, "D2": 73.42, "E2": 82.41, "F2": 87.31, "G2": 98.0, "A2": 110.0,
     "C3": 130.81, "D3": 146.83, "E3": 164.81, "F3": 174.61, "G3": 196.0, "A3": 220.0, "C4": 261.63,
     "D4": 293.66, "E4": 329.63, "F4": 349.23, "A4": 440.0}


def hz(n):
    return N[n] if isinstance(n, str) else n


def lp(x, f, o=2):
    return sosfilt(butter(o, f, "low", fs=RATE, output="sos"), x)


def hp(x, f, o=2):
    return sosfilt(butter(o, f, "high", fs=RATE, output="sos"), x)


def warm(note, dur, attack=0.03, release=0.3, gain=0.5, breathe=2.2, rest=1.3, breathe_t=0.08, pan=0.0, edge=0.18):
    """The voice. edge: how much saw is in it (the only bright part).
    breathe/rest: the filter's cutoff, as multiples of the pitch, at its
    brief opening and at rest. No pitch movement at all."""
    f = hz(note)
    t = T(dur)
    curve = np.full(len(t), f)
    layers = (("sine", 0, 0.55, 0.0), ("tri", 4, 0.42, -0.5), ("tri", -4, 0.42, 0.5), ("saw", 0, edge, 0.0),
              ("sine", -1200, 0.22, 0.0))
    left = np.zeros_like(t)
    right = np.zeros_like(t)
    for shape, cents, g, side in layers:
        osc = bl_osc(curve * 2 ** (cents / 1200), shape) * g
        left += osc * np.cos((side + 1) * np.pi / 4)
        right += osc * np.sin((side + 1) * np.pi / 4)
    cut = f * (rest + (breathe - rest) * np.exp(-t / breathe_t) * np.clip(t / 0.01, 0, 1))
    amp = np.clip(t / attack, 0, 1) ** 1.4 * np.clip((dur - t) / release, 0, 1) ** 1.8 * gain
    l = svf_lowpass(left, cut, 0.8) * amp
    r = svf_lowpass(right, cut, 0.8) * amp
    # Small speakers can't play the lowest notes, but the ear rebuilds a
    # fundamental from its overtones. So low notes carry their 2nd to 5th
    # harmonics, added after the filter (which would otherwise remove them),
    # strongly enough to be heard as deep on a laptop.
    if f < 160:
        lift = min(1.0, (160 - f) / 90) * 0.5 + 0.5
        over = sum(np.sin(2 * np.pi * f * k * t + k) * g for k, g in ((2, 0.34), (3, 0.22), (4, 0.12), (5, 0.06)))
        over = over * amp * lift
        l = l + over
        r = r + over
    return np.vstack([l * np.cos((pan + 1) * np.pi / 4) * 1.41, r * np.sin((pan + 1) * np.pi / 4) * 1.41])


def chord(notes, at, dur, gain, **k):
    return [(at, warm(n, dur, gain=gain, pan=(-0.5 + i / max(len(notes) - 1, 1)) * 0.8, **k)) for i, n in enumerate(notes)]


def place(parts, dur):
    out = np.zeros((2, int(dur * RATE)))
    for at, sig in parts:
        i = int(at * RATE)
        n = min(sig.shape[1], out.shape[1] - i)
        out[:, i:i + n] += sig[:, :n]
    return out


def space(x, size, wet, damp=3000):
    L = int(size * RATE)
    t = np.arange(L) / RATE
    out = np.zeros((2, x.shape[1] + L - 1))
    for ch in range(2):
        ir = rng.standard_normal(L) * np.exp(-t / size * 5.0)
        ir = hp(lp(ir, damp), 140)
        ir[: int(0.02 * RATE)] = 0
        ir /= np.sqrt(np.sum(ir ** 2))
        out[ch] = np.pad(x[ch], (0, L - 1)) * (1 - wet) + fftconvolve(x[ch], ir) * wet * 2.0
    return out


def finish(x, peak_db, floor=-54):
    x = x / np.max(np.abs(x))
    x = np.tanh(x * 1.15) / np.tanh(1.15)
    x = np.vstack([hp(c, 30) for c in x])
    # Presence around 700 Hz to 2.5 kHz, gently, so low notes still read on
    # a laptop's small speakers.
    x = np.vstack([c + 0.3 * sosfilt(butter(2, [700, 2500], "band", fs=RATE, output="sos"), c) for c in x])
    e = np.max(np.abs(x), axis=0)
    last = np.max(np.nonzero(e > 10 ** (floor / 20) * e.max()))
    x = x[:, : last + int(0.03 * RATE)]
    fd = int(0.06 * RATE)
    x[:, -fd:] *= np.linspace(1, 0, fd) ** 2
    return x / np.max(np.abs(x)) * 10 ** (peak_db / 20)


S = {}


def sound(name, peak):
    def reg(fn):
        S[name] = (fn, peak)
        return fn
    return reg


# D minor 9, voiced low and open.
DM9 = ["D2", "A2", "F3", "C4", "E4"]
W = ["A3", "D3", "F3", "D3", "A3"]


@sound("desktop-login", -4)
def login():
    # One chord that blooms open over a second and a half, the W stated
    # softly inside it, then a low D that grounds it.
    parts = chord(DM9, 0.0, 4.6, 0.16, attack=1.3, release=1.8, breathe=2.6, rest=1.8, breathe_t=1.6, edge=0.12)
    for i, (n, at) in enumerate(zip(W, (1.5, 1.72, 1.94, 2.16, 2.45))):
        parts.append((at, warm(n, 0.45 if i < 4 else 1.6, attack=0.04, release=0.25 if i < 4 else 1.0, gain=0.24,
                               breathe=2.6, rest=1.4, pan=-0.3 + i * 0.15)))
    parts.append((2.45, warm("D1", 2.2, attack=0.25, release=1.4, gain=0.35, rest=1.1, breathe=1.3, edge=0.0)))
    return space(place(parts, 4.8), size=2.4, wet=0.26)


@sound("desktop-logout", -5)
def logout():
    parts = []
    for i, (n, at) in enumerate(zip(["A3", "F3", "D3"], (0.0, 0.24, 0.5))):
        parts.append((at, warm(n, 0.45 if i < 2 else 1.6, attack=0.05, release=0.25 if i < 2 else 1.0, gain=0.38)))
    parts += chord(["D2", "A2", "F3"], 0.5, 2.0, 0.17, attack=0.4, release=1.3, breathe=1.8, rest=1.3)
    return space(place(parts, 2.6), size=2.0, wet=0.24)


@sound("message-new-instant", -8)
def notification():
    # The end of the W, down then up, over a quiet low fifth.
    parts = [(0.0, warm("F3", 0.24, attack=0.012, release=0.12, gain=0.5, breathe=3.0)),
             (0.14, warm("A3", 0.5, attack=0.012, release=0.32, gain=0.5, breathe=3.0))]
    parts += chord(["D2", "A2"], 0.0, 0.6, 0.18, attack=0.03, release=0.35, breathe=1.6, rest=1.2, edge=0.05)
    return space(place(parts, 0.8), size=0.8, wet=0.16)


@sound("message", -9)
def message():
    parts = [(0.0, warm("A3", 0.45, attack=0.012, release=0.3, gain=0.55, breathe=2.8))]
    parts += chord(["D2"], 0.0, 0.45, 0.2, attack=0.02, release=0.3, edge=0.0)
    return space(place(parts, 0.6), size=0.7, wet=0.14)


@sound("complete", -8)
def complete():
    parts = []
    for i, (n, at) in enumerate(zip(["D3", "F3", "A3"], (0.0, 0.12, 0.24))):
        parts.append((at, warm(n, 0.3 if i < 2 else 0.8, attack=0.012, release=0.15 if i < 2 else 0.5, gain=0.42, breathe=2.8)))
    parts += chord(["D2", "A2", "E3"], 0.24, 0.9, 0.14, attack=0.08, release=0.55, breathe=1.8, rest=1.3)
    return space(place(parts, 1.2), size=1.0, wet=0.18)


@sound("dialog-information", -10)
def information():
    parts = [(0.0, warm("D3", 0.45, attack=0.015, release=0.3, gain=0.5, breathe=2.4))]
    parts += chord(["A2"], 0.0, 0.45, 0.2, attack=0.02, release=0.3, edge=0.05)
    return space(place(parts, 0.6), size=0.7, wet=0.14)


@sound("dialog-question", -10)
def question():
    parts = [(0.0, warm("D3", 0.22, attack=0.015, release=0.1, gain=0.5)),
             (0.14, warm("E3", 0.45, attack=0.015, release=0.28, gain=0.5))]
    return space(place(parts, 0.7), size=0.7, wet=0.14)


@sound("dialog-warning", -8)
def warning():
    # A falling minor third, low and unhurried.
    parts = [(0.0, warm("C3", 0.28, attack=0.015, release=0.14, gain=0.55, breathe=2.2)),
             (0.18, warm("A2", 0.55, attack=0.015, release=0.35, gain=0.55, breathe=2.0))]
    parts += chord(["A1"], 0.18, 0.5, 0.14, attack=0.02, release=0.3, edge=0.0)
    return space(place(parts, 0.85), size=0.8, wet=0.15)


@sound("dialog-error", -7)
def error():
    # Two low notes a fourth apart, falling, close and short: a firm no.
    parts = [(0.0, warm("D3", 0.2, attack=0.008, release=0.1, gain=0.6, breathe=2.0, rest=1.2)),
             (0.12, warm("A2", 0.38, attack=0.008, release=0.25, gain=0.6, breathe=1.8, rest=1.15))]
    parts += chord(["D2"], 0.0, 0.5, 0.17, attack=0.008, release=0.3, edge=0.0)
    return space(place(parts, 0.6), size=0.5, wet=0.1)


@sound("device-added", -9)
def device_added():
    parts = [(0.0, warm("D3", 0.2, attack=0.012, release=0.1, gain=0.5, breathe=2.8)),
             (0.12, warm("A3", 0.45, attack=0.012, release=0.28, gain=0.5, breathe=2.8))]
    return space(place(parts, 0.7), size=0.7, wet=0.14)


@sound("device-removed", -9)
def device_removed():
    parts = [(0.0, warm("A3", 0.2, attack=0.012, release=0.1, gain=0.5, breathe=2.6)),
             (0.12, warm("D3", 0.45, attack=0.012, release=0.28, gain=0.5, breathe=2.4))]
    return space(place(parts, 0.7), size=0.7, wet=0.14)


@sound("power-plug", -9)
def power_plug():
    parts = [(0.0, warm("A2", 0.2, attack=0.02, release=0.1, gain=0.45)),
             (0.11, warm("D3", 0.2, attack=0.02, release=0.1, gain=0.45)),
             (0.22, warm("A3", 0.5, attack=0.02, release=0.3, gain=0.48, breathe=2.6))]
    return space(place(parts, 0.85), size=0.7, wet=0.14)


@sound("power-unplug", -9)
def power_unplug():
    parts = [(0.0, warm("A3", 0.2, attack=0.015, release=0.1, gain=0.45)),
             (0.11, warm("D3", 0.45, attack=0.015, release=0.28, gain=0.48))]
    return space(place(parts, 0.65), size=0.6, wet=0.12)


@sound("battery-low", -8)
def battery_low():
    parts = [(at, warm("C3", 0.18, attack=0.015, release=0.08, gain=0.5)) for at in (0.0, 0.2)]
    parts += [(0.4, warm("A2", 0.55, attack=0.015, release=0.35, gain=0.55, breathe=2.0))]
    parts += chord(["A1"], 0.4, 0.5, 0.14, attack=0.02, release=0.3, edge=0.0)
    return space(place(parts, 1.05), size=0.8, wet=0.14)


@sound("screen-capture", -10)
def screen_capture():
    parts = [(0.0, warm("D4", 0.12, attack=0.004, release=0.07, gain=0.4, breathe=3.2, rest=1.5)),
             (0.06, warm("A3", 0.18, attack=0.004, release=0.1, gain=0.42, breathe=3.0, rest=1.5))]
    return space(place(parts, 0.3), size=0.35, wet=0.08)


@sound("trash-empty", -9)
def trash_empty():
    parts = []
    for i, (n, at) in enumerate(zip(["A3", "F3", "D3"], (0.0, 0.07, 0.14))):
        parts.append((at, warm(n, 0.16 if i < 2 else 0.4, attack=0.008, release=0.08 if i < 2 else 0.25, gain=0.45)))
    return space(place(parts, 0.6), size=0.5, wet=0.1)


@sound("audio-volume-change", -15)
def volume():
    return place([(0.0, warm("D3", 0.08, attack=0.003, release=0.05, gain=0.7, breathe=3.0, rest=2.0))], 0.09)


@sound("bell", -12)
def bell():
    return space(place([(0.0, warm("D3", 0.2, attack=0.006, release=0.1, gain=0.6, breathe=2.0))], 0.25), size=0.35, wet=0.08)


@sound("battery-caution", -7)
def battery_caution():
    # More urgent than battery-low: three falling notes, the last held.
    parts = []
    for n, at in (("C3", 0.0), ("C3", 0.18), ("A2", 0.36)):
        parts.append((at, warm(n, 0.16 if at < 0.3 else 0.6, attack=0.012, release=0.08 if at < 0.3 else 0.35, gain=0.55)))
    parts += chord(["A1"], 0.36, 0.55, 0.14, attack=0.02, release=0.3, edge=0.0)
    return space(place(parts, 1.1), size=0.8, wet=0.14)


@sound("battery-full", -10)
def battery_full():
    parts = [(0.0, warm("D3", 0.25, attack=0.02, release=0.12, gain=0.45)),
             (0.14, warm("A3", 0.6, attack=0.02, release=0.38, gain=0.45, breathe=2.4))]
    parts += chord(["D2", "A2"], 0.14, 0.6, 0.12, attack=0.06, release=0.4, breathe=1.6, rest=1.2)
    return space(place(parts, 0.9), size=0.8, wet=0.16)


@sound("completion-fail", -9)
def completion_fail():
    parts = [(0.0, warm("F3", 0.2, attack=0.012, release=0.1, gain=0.5)),
             (0.13, warm("D3", 0.45, attack=0.012, release=0.28, gain=0.5, breathe=2.0))]
    return space(place(parts, 0.7), size=0.6, wet=0.12)


@sound("completion-rotation", -15)
def completion_rotation():
    return place([(0.0, warm("A3", 0.07, attack=0.003, release=0.04, gain=0.6, breathe=2.6, rest=1.8))], 0.08)


@sound("dialog-error-critical", -6)
def error_critical():
    # The error, stated twice: for something that really went wrong.
    parts = []
    for at in (0.0, 0.42):
        parts += [(at, warm("D3", 0.2, attack=0.008, release=0.1, gain=0.6, breathe=2.0, rest=1.2)),
                  (at + 0.12, warm("A2", 0.36, attack=0.008, release=0.24, gain=0.6, breathe=1.8, rest=1.15))]
    parts += chord(["D2"], 0.0, 0.95, 0.15, attack=0.01, release=0.4, edge=0.0)
    return space(place(parts, 1.1), size=0.6, wet=0.12)


# Names that share another sound's file: alias -> sound. The package links
# them (debian/links) rather than shipping copies.
ALIASES = {
    "bell-window-system": "bell",
    "completion-success": "complete",
    "dialog-error-serious": "dialog-error",
    "dialog-warning-auth": "dialog-question",
    "message-new-email": "message",
    "camera-shutter": "screen-capture",
}


def write(name, x, out):
    pcm = (np.clip(x.T, -1, 1) * 32767).astype("<i2")
    os.makedirs(os.path.join(out, "stereo"), exist_ok=True)
    wav = os.path.join(out, name + ".wav")
    with wave.open(wav, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    oga = os.path.join(out, "stereo", name + ".oga")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-map_metadata", "-1", "-c:a", "libvorbis",
                    "-q:a", "6", "-fflags", "+bitexact", "-flags:a", "+bitexact", oga], check=True)
    os.remove(wav)
    return x.shape[1] / RATE


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    for name, (fn, peak) in S.items():
        seconds = write(name, finish(fn(), peak), out)
        print(f"{name:24s} {seconds:4.2f} s")


if __name__ == "__main__":
    main()
