# Tagit

`tagit` is a tiny command line helper for creating symbolic links without
typing both long paths in one command.

Instead of writing:

```sh
ln -s /very/long/source/path/file.txt /another/long/place/file.txt
```

you can run:

```sh
tagit /very/long/source/path/file.txt
cd /another/long/place
tagit .
```

When the second command succeeds, `tagit` prints:

```text
success
```

## How It Works

`tagit <path>` works in pairs:

1. The first call stores the source path.
2. The second call creates a symlink to that source at the destination path.

If the second path is a directory, the symlink is created inside that directory
using the source file or directory name.

```sh
tagit ~/notes/project.md
tagit ~/Desktop
```

Creates:

```text
~/Desktop/project.md -> ~/notes/project.md
```

If the second path is not a directory, it is used as the symlink path.

```sh
tagit ~/notes/project.md
tagit ~/Desktop/today.md
```

Creates:

```text
~/Desktop/today.md -> ~/notes/project.md
```

## Duplicates

Calling `tagit` on the same destination more than once creates duplicates
instead of overwriting existing files or links.

For example:

```sh
tagit ~/notes/project.md
tagit ~/Desktop
tagit ~/notes/project.md
tagit ~/Desktop
```

May create:

```text
~/Desktop/project.md
~/Desktop/project.md-1
```

## Install Globally

Clone the repository, build the executable, then install it somewhere on your
`PATH` so you can run `tagit` from any directory.

```sh
make
mkdir -p ~/.local/bin
cp tagit ~/.local/bin/tagit
```

Add `~/.local/bin` to your shell `PATH` if it is not already there.

For zsh, add this to `~/.zshrc`:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Then restart your terminal or run:

```sh
source ~/.zshrc
```

Confirm the global command is available:

```sh
command -v tagit
tagit
```

The first command should print a path such as:

```text
/Users/you/.local/bin/tagit
```

The second command should print usage instructions because no path was passed.

## Requirements

`tagit` is written in C and uses Unix/POSIX APIs:

- `lstat`
- `stat`
- `realpath`
- `symlink`
- `unlink`

To build it, you need `make` and a C compiler such as `cc`, `clang`, or `gcc`.

It stores the pending source path at:

```text
$XDG_STATE_HOME/tagit/pending
```

If `XDG_STATE_HOME` is not set, it uses:

```text
~/.local/state/tagit/pending
```

## Tests

Run the test suite from the repository root:

```sh
make test
```
