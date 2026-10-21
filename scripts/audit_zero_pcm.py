#!/usr/bin/env python3
"""
scripts/audit_zero_pcm.py
Automated CI Static Source Audit for Poopenfarten II (PF-02)

Verifies 100% procedural physical modeling synthesis purity.
Strictly forbids:
  1. Static PCM audio sample tables (e.g. legacy SampleBank.cpp, s_sample_ arrays).
  2. Base64-encoded PCM audio strings (e.g. legacy sample_bank.js, base64Data).
  3. Sample playback / pitcher engines (e.g. SamplePitcherEngine).
  4. Disguised large float arrays (>128 float literals) in source code representing sampled waveforms.
  5. Audio binary assets (.wav, .aif, .pcm, .raw) embedded in source or web trees.

Returns exit code 0 if 100% clean, or 1 if any anti-pattern or shortcut is detected.
"""

import sys
import os
import re
from pathlib import Path

# Forbidden string / token patterns in source code
FORBIDDEN_PATTERNS = [
    (re.compile(r'\bSampleBank\b', re.IGNORECASE), "Legacy PCM SampleBank reference detected"),
    (re.compile(r'\bs_sample_\d+', re.IGNORECASE), "Static PCM sample data array identifier detected"),
    (re.compile(r'\bSamplePitcherEngine\b', re.IGNORECASE), "Sample playback/resampling engine reference detected"),
    (re.compile(r'base64Data\s*[:=]\s*["\']', re.IGNORECASE), "Base64 encoded audio sample data property detected"),
    (re.compile(r'atob\s*\(\s*["\'][A-Za-z0-9+/=]{100,}', re.IGNORECASE), "Inline Base64 PCM decoding detected"),
    (re.compile(r'data:audio\/(?:wav|mp3|ogg|x-wav);base64,[A-Za-z0-9+/=]{100,}', re.IGNORECASE), "Embedded Data-URI audio asset detected"),
]

# Pattern for detecting large float arrays (more than 128 floating-point numbers in a curly brace block)
FLOAT_LITERAL_REGEX = re.compile(r'[-+]?\d*\.\d+(?:[eE][-+]?\d+)?f?')

# File extensions to scan
SOURCE_EXTENSIONS = {'.h', '.hpp', '.c', '.cpp', '.cxx', '.js', '.mjs', '.ts', '.html', '.css', '.json'}

# Forbidden binary audio asset extensions in source and web trees
FORBIDDEN_AUDIO_EXTENSIONS = {'.wav', '.wave', '.aif', '.aiff', '.pcm', '.raw', '.flac', '.mp3', '.ogg'}

# Directories to ignore
IGNORE_DIRS = {'.git', '.agents', '.vscode', 'build', 'build_test', 'node_modules', 'cmake-build-debug', 'cmake-build-release'}

def scan_file_for_violations(file_path: Path):
    violations = []
    
    # Check for forbidden binary extensions
    if file_path.suffix.lower() in FORBIDDEN_AUDIO_EXTENSIONS:
        violations.append((0, f"Embedded audio binary file detected: {file_path.name}"))
        return violations

    if file_path.suffix.lower() not in SOURCE_EXTENSIONS:
        return violations

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        violations.append((0, f"Could not read file: {e}"))
        return violations

    # Check line-by-line for forbidden patterns
    for line_idx, line in enumerate(lines, start=1):
        for pattern, desc in FORBIDDEN_PATTERNS:
            if pattern.search(line):
                violations.append((line_idx, f"{desc} -> '{line.strip()[:80]}'"))

    # Check for suspicious large float arrays in C++ or JS (> 128 float literals)
    # Exclude legitimate filter coefficient tables (which are small, <= 32)
    if file_path.suffix.lower() in {'.cpp', '.h', '.js', '.mjs'} and 'audit_zero_pcm.py' not in file_path.name:
        content = "".join(lines)
        # Search for array definitions like: float arr[] = { ... }; or [ ... ]
        array_blocks = re.findall(r'(\b(?:float|double)\s+\w+\[\s*\]\s*=\s*\{([^}]+)\})', content, re.DOTALL)
        for full_match, inner in array_blocks:
            floats = FLOAT_LITERAL_REGEX.findall(inner)
            if len(floats) > 128:
                violations.append((1, f"Suspicious large float array ({len(floats)} entries) detected, possible disguised PCM table"))

    return violations

def audit_directory(root_dir: Path):
    print("=" * 80)
    print("      POOPENFARTEN II (PF-02) — AUTOMATED ZERO-PCM SYNTHESIS AUDIT      ")
    print("=" * 80)
    print(f"Audit Target Root: {root_dir}")
    print("Policy: 100% continuous procedural physical modeling; 0 static PCM tables.\n")

    total_files_scanned = 0
    total_violations = 0
    violations_by_file = {}

    target_subdirs = [root_dir / "source", root_dir / "web", root_dir / "scripts"]
    # Also check if root has files
    for entry in root_dir.iterdir():
        if entry.is_file() and entry.suffix.lower() in SOURCE_EXTENSIONS:
            target_subdirs.append(entry)

    paths_to_visit = []
    for target in target_subdirs:
        if not target.exists():
            continue
        if target.is_file():
            paths_to_visit.append(target)
        else:
            for root, dirs, files in os.walk(target):
                # Filter out ignored directories
                dirs[:] = [d for d in dirs if d not in IGNORE_DIRS]
                for file in files:
                    file_path = Path(root) / file
                    paths_to_visit.append(file_path)

    for file_path in paths_to_visit:
        # Skip audit and test harness files containing assertion strings
        if file_path.name in {'audit_zero_pcm.py', 'verify.mjs'} or 'tests' in file_path.parts:
            continue

        total_files_scanned += 1
        file_violations = scan_file_for_violations(file_path)
        if file_violations:
            total_violations += len(file_violations)
            violations_by_file[file_path] = file_violations

    print(f"Total Source & Asset Files Scanned: {total_files_scanned}")
    print("-" * 80)

    if total_violations == 0:
        print("[AUDIT SUCCESS] 0 static PCM sample tables detected.")
        print("[AUDIT SUCCESS] 0 hardcoded float audio arrays detected.")
        print("[AUDIT SUCCESS] 0 Base64-encoded PCM strings detected.")
        print("[AUDIT SUCCESS] 0 embedded audio binary files detected.")
        print("VERDICT: 100% PROCEDURAL SYNTHESIS PURITY CONFIRMED (PASS)")
        print("=" * 80)
        return 0
    else:
        print(f"[AUDIT FAILURE] Detected {total_violations} forbidden PCM audio violation(s):\n")
        for file_path, viols in violations_by_file.items():
            rel_path = file_path.relative_to(root_dir)
            print(f"  FILE: {rel_path}")
            for line_no, desc in viols:
                print(f"    Line {line_no:4d}: {desc}")
            print()
        print("VERDICT: ZERO-PCM AUDIT FAILED — REJECTING STATIC AUDIO SHORTCUTS (FAIL)")
        print("=" * 80)
        return 1

def main():
    root_dir = Path(__file__).resolve().parent.parent
    if len(sys.argv) > 1 and not sys.argv[1].startswith('-'):
        root_dir = Path(sys.argv[1]).resolve()
    
    exit_code = audit_directory(root_dir)
    sys.exit(exit_code)

if __name__ == '__main__':
    main()
