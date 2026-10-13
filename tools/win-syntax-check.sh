#!/usr/bin/env bash
# Compiles source files with clang-cl against the MSVC STL and the Windows SDK
# (syntax only, /Zs) from a Linux box, to catch Windows-only errors before the
# PC does. One-off set-up: download xwin (github.com/Jake-Shadle/xwin) and run
#   xwin --accept-license splat --output $XWIN
# then   XWIN=/path/to/splat tools/win-syntax-check.sh src/dsp/Voice.cpp tests/TableTest.cpp
# Needs a Linux build in build/ (cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON).
# Clang does not enforce MSVC's 16 KB string-literal limit (C2026): keep
# embedded text chunks small.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${XWIN:?set XWIN to the xwin splat folder}"
python3 - "$@" <<'PY'
import json, os, shlex, subprocess, sys
root = os.getcwd()
xwin = os.environ["XWIN"]
cc = json.load(open("build/compile_commands.json"))
byfile = {}
for e in cc:
    byfile.setdefault(os.path.relpath(e["file"], root), e)
fail = 0
for src in sys.argv[1:]:
    e = byfile.get(src) or next((v for k, v in byfile.items() if k.startswith("src/") or k.startswith("tests/")), None)
    args = shlex.split(e["command"])[1:]
    keep = []
    skip = False
    for a in args:
        if skip: skip = False; continue
        if a in ("-o", "-MF", "-MT", "-MQ"): skip = True; continue
        if a.startswith(("-D", "-I")) and a not in ("-DLINUX=1",) and "x11" not in a.lower() and "/usr/include" not in a:
            keep.append(a)
    cmd = ["clang", "--driver-mode=cl", "--target=x86_64-pc-windows-msvc", "/Zs", "/std:c++20", "/EHsc", "/DNOMINMAX", "/DWIN32",
           "/D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH", "-Wno-everything",
           f"-imsvc{xwin}/crt/include", f"-imsvc{xwin}/sdk/include/ucrt", f"-imsvc{xwin}/sdk/include/um",
           f"-imsvc{xwin}/sdk/include/shared"] + keep + [src]
    r = subprocess.run(cmd, capture_output=True, text=True)
    errs = [l for l in r.stderr.splitlines() if "error" in l]
    print(("FAIL " if r.returncode else "ok   ") + src)
    for l in errs[:8]: print("   ", l[:220])
    fail |= bool(r.returncode)
sys.exit(fail)
PY
