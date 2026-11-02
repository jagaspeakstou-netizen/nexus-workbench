#!/usr/bin/env bash
# Builds <CANDIDATE_ID>_Workbench.zip from the committed repo state (git archive,
# so no build outputs, node_modules or untracked files can leak in), plus:
#   - version-control evidence: a git bundle and a readable commit log
#   - optionally the CI-built Windows app (NEXUS_Windows_x64.zip from the
#     windows-build workflow's "nexus-windows-evidence" artifact)
#   - optionally the candidate's photographed handwritten appendix pages
# Usage: scripts/make_submission_zip.sh [-i CANDIDATE_ID] [-a NEXUS_Windows_x64.zip] [-p page1.jpg -p page2.jpg] [-o outdir]
set -euo pipefail
ID="CandidateID"; APP=""; PAGES=(); OUT="$PWD"
while getopts "i:a:p:o:" o; do case $o in i) ID=$OPTARG;; a) APP=$OPTARG;; p) PAGES+=("$OPTARG");; o) OUT=$OPTARG;; *) exit 2;; esac; done
APP=$([ -n "$APP" ] && realpath "$APP" || true); OUT=$(realpath "$OUT")
PAGES=("${PAGES[@]/#/}"); for i in "${!PAGES[@]}"; do PAGES[$i]=$(realpath "${PAGES[$i]}"); done
cd "$(git rev-parse --show-toplevel)"
[ -z "$(git status --porcelain)" ] || { echo "ERROR: uncommitted changes; commit first (zip is built from HEAD)"; exit 1; }
STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
ROOT="$STAGE/${ID}_Workbench"; mkdir -p "$ROOT"
git archive HEAD | tar -x -C "$ROOT"
WARN=()

# Brief: "no machine-specific absolute paths". One raw captured test log contains
# the dev container's paths; rewrite them in the ZIP copy only and say so in-file.
F="$ROOT/docs/technical_report/ai_challenge/ctest_output_with_incorrect_formulation.txt"
if [ -f "$F" ]; then
  sed -i 's#/root/nexus-workbench/##g' "$F"
  sed -i '1i [Packaging note: the dev machine'"'"'s absolute path prefix was stripped from this raw log for the ZIP; nothing else was edited. The unmodified file is in the git bundle.]' "$F"
fi

mkdir -p "$ROOT/docs/version_control"
git bundle create "$ROOT/docs/version_control/nexus-workbench.bundle" --all 2>/dev/null
git log --date=short --pretty=format:'%h  %ad  %s' > "$ROOT/docs/version_control/commit_log.txt"
printf '\n' >> "$ROOT/docs/version_control/commit_log.txt"

if [ -n "$APP" ] && [ -f "$APP" ]; then
  mkdir -p "$ROOT/NEXUS_Windows_x64"; unzip -q "$APP" -d "$ROOT/NEXUS_Windows_x64"
  [ -f "$ROOT/NEXUS_Windows_x64/Launch.exe" ] || WARN+=("Launch.exe not found inside $APP")
else WARN+=("Launch.exe / packaged app NOT included (pass -a NEXUS_Windows_x64.zip)"); fi
if [ ${#PAGES[@]} -gt 0 ]; then
  mkdir -p "$ROOT/docs/handwritten_appendix"; n=1
  for p in "${PAGES[@]}"; do cp "$p" "$ROOT/docs/handwritten_appendix/page${n}.${p##*.}"; n=$((n+1)); done
  [ ${#PAGES[@]} -le 2 ] || WARN+=("handwritten appendix has >2 pages")
else WARN+=("handwritten appendix NOT included (pass -p page1.jpg -p page2.jpg)"); fi

if grep -rIlE '/root/|/home/[a-z]|C:\\Users|/mnt/user' "$ROOT" --exclude='*.bundle' >/dev/null 2>&1; then
  WARN+=("absolute paths remain: $(grep -rIlE '/root/|/home/[a-z]|C:\\Users|/mnt/user' "$ROOT" --exclude='*.bundle' | tr '\n' ' ')"); fi

ZIP="$OUT/${ID}_Workbench.zip"; rm -f "$ZIP"
(cd "$STAGE" && zip -qr "$ZIP" "${ID}_Workbench")
echo "Built $ZIP ($(du -h "$ZIP" | cut -f1), $(unzip -Z1 "$ZIP" | wc -l) entries)"
for w in "${WARN[@]:-}"; do [ -n "$w" ] && echo "WARNING: $w"; done
exit 0
