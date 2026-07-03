# Developer Notes

## Project Shape

This project is intentionally small:

- `src/tagit.c` is the CLI source.
- `Makefile` builds the `tagit` executable.
- `README.md` explains user behavior and installation.
- `developer.md` explains implementation details for contributors.

The executable is written in C and uses Unix/POSIX APIs so it can run on most
Unix-like systems without a language runtime.

## Behavior Contract

`tagit <path>` has two phases:

1. If there is no pending source, `<path>` must already exist. The script stores
   its absolute path and prints `tagged <path>`.
2. If there is a pending source, `<path>` is treated as the destination. The
   script creates a symlink to the pending source and prints `success`.

Destination rules:

- If the destination is an existing directory, the symlink is placed inside it
  with the source basename.
- If the destination is not a directory, it is treated as the full symlink path.
- If the requested symlink path already exists, `tagit` appends `-1`, `-2`, and
  so on until it finds a free path.
- Existing files and links are never overwritten.

State is stored in:

```text
${XDG_STATE_HOME:-$HOME/.local/state}/tagit/pending
```

## Tests

Run the test suite from the repository root:

```sh
make test
```

The tests use temporary directories and isolate `XDG_STATE_HOME`, so they do
not touch your real pending `tagit` state.

## Manual Test Script

Run this from the repository root:

```sh
tmp=$(mktemp -d)
make
XDG_STATE_HOME="$tmp/state" ./tagit "$PWD/tagit"
XDG_STATE_HOME="$tmp/state" ./tagit "$tmp"
ls -l "$tmp"
rm -rf "$tmp"
```

Expected result:

- The first command prints `tagged ...`.
- The second command prints `success`.
- The temporary directory contains a symlink named `tagit`.

Duplicate behavior:

```sh
tmp=$(mktemp -d)
make
XDG_STATE_HOME="$tmp/state" ./tagit "$PWD/tagit"
XDG_STATE_HOME="$tmp/state" ./tagit "$tmp"
XDG_STATE_HOME="$tmp/state" ./tagit "$PWD/tagit"
XDG_STATE_HOME="$tmp/state" ./tagit "$tmp"
ls -l "$tmp"
rm -rf "$tmp"
```

Expected result:

- The temporary directory contains `tagit` and `tagit-1`.

## Release Checklist

Before publishing:

1. Run the manual tests above.
2. Run `make clean && make`.
3. Confirm the README examples still match the current behavior.
