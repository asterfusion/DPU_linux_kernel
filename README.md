# ET2500 Yocto Linux Kernel Patch Set

This repository tracks a small patch stack on top of Yocto `linux-yocto`.

The current base is defined in upstream/base.lock, and the patch order is defined in patches/series.

## Layout

- `patches/`: mail-format patches applied with `git am`
- `patches/series`: patch application order
- `upstream/sources.yaml`: upstream repository and branch
- `upstream/base.lock`: locked base commit and build metadata
- `scripts/apply_patches.sh`: clone upstream, checkout base commit, apply patch series

## Requirements

- `git`
- `bash`
- `yq`

## Usage

Run from the repository root:

```bash
bash scripts/apply_patches.sh
```

The script will:

1. Read the upstream repo and branch from `upstream/sources.yaml`
2. Read the pinned base commit from `upstream/base.lock`
3. Clone the upstream kernel into `workdir/`
4. Apply patches listed in `patches/series` using `git am`

## Notes

- `workdir/` is generated and should not be committed.
- `linux.config` is currently carried inside the patch series rather than stored as a standalone tracked file in this repository.
- This repository stores patch metadata and automation, not a full kernel source tree.
