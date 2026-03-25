#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM_DIR="$ROOT_DIR/upstream"
PATCH_DIR="$ROOT_DIR/patches"
WORKDIR="$ROOT_DIR/workdir"

SOURCE_FILE="$UPSTREAM_DIR/sources.yaml"
BASE_FILE="$UPSTREAM_DIR/base.lock"

command -v git >/dev/null || { echo "[ERROR] git not found"; exit 1; }
command -v yq >/dev/null || { echo "[ERROR] yq not found (needed for yaml parsing)"; exit 1; }

REPO=$(yq -r '.kernel.repo' "$SOURCE_FILE")
BRANCH=$(yq -r '.kernel.branch' "$SOURCE_FILE")

BASE_COMMIT=$(yq -r '.base.commit' "$BASE_FILE")

echo "[INFO] Upstream repo:   $REPO"
echo "[INFO] Branch:          $BRANCH"
echo "[INFO] Base commit:     $BASE_COMMIT"

echo "[INFO] Cleaning workdir..."
rm -rf "$WORKDIR"

echo "[INFO] Cloning upstream..."
git clone --depth=1 -b "$BRANCH" "$REPO" "$WORKDIR"

cd "$WORKDIR"

echo "[INFO] Checking out base commit..."

if ! git checkout "$BASE_COMMIT"; then
    echo "[WARN] Commit not in shallow clone, fetching full history..."
    git fetch --unshallow
    git checkout "$BASE_COMMIT"
fi

echo "[INFO] Applying patches..."

if [ ! -f "$PATCH_DIR/series" ]; then
    echo "[ERROR] patches/series not found"
    exit 1
fi

while read -r patch; do
    [[ -z "$patch" || "$patch" =~ ^# ]] && continue

    echo "[INFO] Applying $patch"

    if ! git am "$PATCH_DIR/$patch"; then
        echo "[ERROR] Failed to apply $patch"
        echo "[HINT] Resolve conflict in $WORKDIR, then run: git am --continue"
        exit 1
    fi

done < "$PATCH_DIR/series"

echo "[OK] All patches applied successfully"

echo "[INFO] Final commit log:"
git --no-pager log --oneline -n 10

echo "[DONE] Workdir ready at: $WORKDIR"