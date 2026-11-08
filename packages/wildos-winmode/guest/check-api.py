#!/usr/bin/python3
"""Checks that wild-guest.exe only uses .NET Framework API that Windows has.

mono's compiler builds wild-guest against mono's own class libraries, which
carry newer overloads than .NET Framework 4.8 (String.TrimEnd(char), say)
and keep some types in other assemblies (Queue<T> in mscorlib). Either
compiles here and fails inside Windows at run time. Every type and member
the program refers to must be on the list in api.txt, which only grows by
something that has been run inside Windows, or that resolve-api.ps1
has resolved against .NET Framework 4.8 on Windows.

    check-api.py wild-guest.exe api.txt         check
    check-api.py wild-guest.exe api.txt --write rewrite the list
"""
import subprocess
import sys


def references(exe):
    out = []
    types = subprocess.run(["monodis", "--typeref", exe], capture_output=True, text=True, check=True).stdout
    for line in types.splitlines():
        if ": [" in line:
            out.append("type " + line.split(": ", 1)[1].strip())
    members = subprocess.run(["monodis", "--memberref", exe], capture_output=True, text=True, check=True).stdout
    resolved = None
    for line in members.splitlines():
        line = line.strip()
        if line.startswith("Resolved:"):
            resolved = line[len("Resolved:"):].strip()
        elif line.startswith("Signature:") and resolved:
            out.append("member " + resolved + " :: " + line[len("Signature:"):].strip())
            resolved = None
    return sorted(set(out))


def main():
    exe, listing = sys.argv[1], sys.argv[2]
    found = references(exe)
    if "--write" in sys.argv:
        with open(listing, "w") as f:
            f.write("\n".join(found) + "\n")
        return 0
    with open(listing) as f:
        allowed = {line.rstrip("\n") for line in f if line.strip()}
    unknown = [r for r in found if r not in allowed]
    if unknown:
        print("wild-guest uses API not yet checked inside Windows (see guest/check-api.py):", file=sys.stderr)
        for r in unknown:
            print("  " + r, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
