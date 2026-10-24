import os
import sys
import json
import shutil
import zipfile
import hashlib

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
release_dir = os.path.join(root_dir, "releases")
build_dir = os.path.join(root_dir, "build", "poopenfarten2_artefacts", "Release")
if not os.path.exists(build_dir):
    build_dir = os.path.join(root_dir, "build", "poopenfarten2_artefacts")

os.makedirs(release_dir, exist_ok=True)

if len(sys.argv) > 1 and sys.argv[1].strip():
    version = sys.argv[1].strip().lstrip("v")
else:
    with open(os.path.join(root_dir, "package.json"), "r", encoding="utf-8") as f:
        pkg = json.load(f)
    version = pkg.get("version", "0.1.0")

install_guide = """POOPENFARTEN II (PF-02) · DARM-AKUSTISCHER & ULTRAMETRISCHER TRIADEN-SYNTHESIZER
Standard: DIN 1451 Technical Specification
Version: v{version}

LEGAL NOTICE:
Not affiliated with Braun GmbH. German functionalist laboratory design homage.
Manufacturer: Sneed's Feed & Seed Ltd.

WINDOWS X64 INSTALLATION GUIDE:
1. VST3 Plugin:
   Copy directory 'PF-02.vst3' to %CommonProgramFiles%\\VST3\\
   (Default: C:\\Program Files\\Common Files\\VST3\\PF-02.vst3)

2. CLAP Plugin:
   Copy 'PF-02.clap' to %CommonProgramFiles%\\CLAP\\
   (Default: C:\\Program Files\\Common Files\\CLAP\\PF-02.clap)

3. Standalone Synthesizer:
   Launch 'PF-02.exe' directly.
""".format(version=version)

vst3_src = os.path.join(build_dir, "VST3", "PF-02.vst3")
clap_src = os.path.join(build_dir, "CLAP", "PF-02.clap")
exe_src = os.path.join(build_dir, "Standalone", "PF-02.exe")

if not os.path.exists(vst3_src):
    raise FileNotFoundError(f"VST3 bundle not found at {vst3_src}")
if not os.path.exists(exe_src):
    raise FileNotFoundError(f"Standalone executable not found at {exe_src}")

# 1. Copy raw Standalone (.exe), VST3 (.vst3), and CLAP (.clap) directly into releases/
dst_exe = os.path.join(release_dir, "PF-02.exe")
shutil.copy2(exe_src, dst_exe)
print(f"Copied Standalone EXE -> {dst_exe} ({os.path.getsize(dst_exe):,} bytes)")

dst_vst3 = os.path.join(release_dir, "PF-02.vst3")
if os.path.exists(dst_vst3):
    shutil.rmtree(dst_vst3)
shutil.copytree(vst3_src, dst_vst3)
print(f"Copied VST3 Bundle    -> {dst_vst3}")

if os.path.exists(clap_src):
    dst_clap = os.path.join(release_dir, "PF-02.clap")
    shutil.copy2(clap_src, dst_clap)
    print(f"Copied CLAP Plugin    -> {dst_clap} ({os.path.getsize(dst_clap):,} bytes)")

# 2. Package Windows-x64 full distribution zip (VST3 + CLAP + Standalone EXE + Docs)
zip_path = os.path.join(release_dir, f"PF-02-v{version}-Windows-x64.zip")
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(vst3_src):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    if os.path.exists(clap_src):
        zf.write(clap_src, "PF-02.clap")
    zf.write(exe_src, "PF-02.exe")
    zf.writestr("INSTALL.txt", install_guide)
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "ARCHITECTURE.md")):
        zf.write(os.path.join(root_dir, "ARCHITECTURE.md"), "ARCHITECTURE.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {zip_path}: {os.path.getsize(zip_path):,} bytes")

# 3. Package VST3-only distribution zip
vst3_zip_path = os.path.join(release_dir, f"PF-02-v{version}-VST3-Windows-x64.zip")
with zipfile.ZipFile(vst3_zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(vst3_src):
        for f in files:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, os.path.join(build_dir, "VST3"))
            zf.write(full, rel)
    zf.writestr("INSTALL.txt", install_guide)
    if os.path.exists(os.path.join(root_dir, "README.md")):
        zf.write(os.path.join(root_dir, "README.md"), "README.md")
    if os.path.exists(os.path.join(root_dir, "LICENSE")):
        zf.write(os.path.join(root_dir, "LICENSE"), "LICENSE")

print(f"Created {vst3_zip_path}: {os.path.getsize(vst3_zip_path):,} bytes")

# 4. Compute SHA-256 cryptographic checksums and write releases/SHA256SUMS.txt
def sha256_file(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as bf:
        while chunk := bf.read(65536):
            h.update(chunk)
    return h.hexdigest()

sha256_lines = []
artifacts_to_hash = [
    os.path.basename(zip_path),
    os.path.basename(vst3_zip_path),
    "PF-02.exe",
    "PF-02.clap",
]

for fname in artifacts_to_hash:
    fpath = os.path.join(release_dir, fname)
    if os.path.isfile(fpath):
        digest = sha256_file(fpath)
        sha256_lines.append(f"{digest}  {fname}\n")
        print(f"SHA256 ({fname}) = {digest}")

checksum_file = os.path.join(release_dir, "SHA256SUMS.txt")
with open(checksum_file, "w", encoding="utf-8") as f:
    f.writelines(sha256_lines)

print(f"Wrote cryptographic checksums to {checksum_file}")
