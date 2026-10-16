"""Print the CHANGELOG.md section for one version (used by CI to fill the GitHub release body).
Usage: python3 tools/release_notes.py 0.5.0"""
import os, re, sys

def section(text, version):
    m = re.search(r"^## \[?%s\]?[^\n]*\n(.*?)(?=^## |\Z)" % re.escape(version), text, re.S | re.M)
    return m.group(1).strip() if m else ""

if __name__ == "__main__":
    v = sys.argv[1]
    body = section(open(os.path.join(os.path.dirname(__file__), "..", "CHANGELOG.md"), encoding="utf-8").read(), v)
    if not body: body = f"Cast Menu {v}. See the commit list below."
    print(body + "\n\n**Install**: unzip `castmenu-%s.zip`, then `python3 -m tools.cast_card build /Volumes/GDEMU` (full guide in the README).\n" % v)
