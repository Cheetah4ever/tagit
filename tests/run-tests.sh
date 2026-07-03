#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TAGIT=$ROOT_DIR/tagit
TEST_ROOT=$(mktemp -d)
PASS_COUNT=0

make -C "$ROOT_DIR" >/dev/null

cleanup() {
  rm -rf "$TEST_ROOT"
}

trap cleanup EXIT INT TERM

pass() {
  PASS_COUNT=$((PASS_COUNT + 1))
  printf 'ok %s - %s\n' "$PASS_COUNT" "$1"
}

fail() {
  printf 'not ok - %s\n' "$1" >&2
  exit 1
}

assert_eq() {
  expected=$1
  actual=$2
  message=$3

  if [ "$expected" = "$actual" ]; then
    pass "$message"
  else
    printf 'expected: %s\nactual:   %s\n' "$expected" "$actual" >&2
    fail "$message"
  fi
}

assert_symlink_target() {
  link_path=$1
  target_path=$2
  message=$3

  [ -L "$link_path" ] || fail "$message (missing symlink)"

  actual=$(readlink "$link_path")
  assert_eq "$target_path" "$actual" "$message"
}

run_tagit() {
  state_dir=$1
  path=$2
  XDG_STATE_HOME="$state_dir" "$TAGIT" "$path"
}

run_global_tagit_from() {
  state_dir=$1
  work_dir=$2
  path=$3

  (cd "$work_dir" && PATH="$ROOT_DIR:$PATH" XDG_STATE_HOME="$state_dir" tagit "$path")
}

resolve_path() {
  path=$1
  dir=$(dirname "$path")
  base=$(basename "$path")
  printf '%s/%s\n' "$(cd "$dir" && pwd -P)" "$base"
}

test_directory_destination() {
  case_root=$TEST_ROOT/directory-destination
  mkdir -p "$case_root/source" "$case_root/dest"
  printf 'hello\n' > "$case_root/source/file.txt"
  source_path=$(resolve_path "$case_root/source/file.txt")

  output=$(run_tagit "$case_root/state" "$case_root/source/file.txt")
  assert_eq "tagged $case_root/source/file.txt" "$output" "tags an existing source"

  output=$(run_tagit "$case_root/state" "$case_root/dest")
  assert_eq "success" "$output" "prints success after creating directory destination symlink"
  assert_symlink_target "$case_root/dest/file.txt" "$source_path" "links into a destination directory"
}

test_explicit_destination_name() {
  case_root=$TEST_ROOT/explicit-destination-name
  mkdir -p "$case_root/source" "$case_root/dest"
  printf 'hello\n' > "$case_root/source/file.txt"
  source_path=$(resolve_path "$case_root/source/file.txt")

  run_tagit "$case_root/state" "$case_root/source/file.txt" >/dev/null
  output=$(run_tagit "$case_root/state" "$case_root/dest/renamed.txt")

  assert_eq "success" "$output" "prints success for explicit destination filename"
  assert_symlink_target "$case_root/dest/renamed.txt" "$source_path" "uses explicit destination filename"
}

test_duplicate_destination() {
  case_root=$TEST_ROOT/duplicate-destination
  mkdir -p "$case_root/source" "$case_root/dest"
  printf 'hello\n' > "$case_root/source/file.txt"
  source_path=$(resolve_path "$case_root/source/file.txt")

  run_tagit "$case_root/state" "$case_root/source/file.txt" >/dev/null
  run_tagit "$case_root/state" "$case_root/dest" >/dev/null
  run_tagit "$case_root/state" "$case_root/source/file.txt" >/dev/null
  output=$(run_tagit "$case_root/state" "$case_root/dest")

  assert_eq "success" "$output" "prints success for duplicate destination"
  assert_symlink_target "$case_root/dest/file.txt" "$source_path" "keeps first destination symlink"
  assert_symlink_target "$case_root/dest/file.txt-1" "$source_path" "creates duplicate destination symlink"
}

test_missing_source_fails() {
  case_root=$TEST_ROOT/missing-source
  mkdir -p "$case_root"

  if output=$(run_tagit "$case_root/state" "$case_root/nope.txt" 2>&1); then
    fail "missing source fails"
  fi

  assert_eq "tagit: source path does not exist: $case_root/nope.txt" "$output" "reports missing source"
}

test_missing_destination_parent_fails() {
  case_root=$TEST_ROOT/missing-destination-parent
  mkdir -p "$case_root/source"
  printf 'hello\n' > "$case_root/source/file.txt"

  run_tagit "$case_root/state" "$case_root/source/file.txt" >/dev/null

  if output=$(run_tagit "$case_root/state" "$case_root/missing/out.txt" 2>&1); then
    fail "missing destination parent fails"
  fi

  assert_eq "tagit: destination directory does not exist: $case_root/missing" "$output" "reports missing destination parent"
}

test_global_command_after_cd_up() {
  case_root=$TEST_ROOT/global-command-after-cd-up
  mkdir -p "$case_root/parent/child" "$case_root/parent/dest"
  printf 'hello\n' > "$case_root/parent/child/file.txt"
  source_path=$(resolve_path "$case_root/parent/child/file.txt")

  output=$(run_global_tagit_from "$case_root/state" "$case_root/parent/child" "file.txt")
  assert_eq "tagged file.txt" "$output" "global tagit tags a relative source from a child directory"

  output=$(run_global_tagit_from "$case_root/state" "$case_root/parent" "dest")
  assert_eq "success" "$output" "global tagit works after moving one directory up"
  assert_symlink_target "$case_root/parent/dest/file.txt" "$source_path" "links after moving one directory up"
}

test_directory_destination
test_explicit_destination_name
test_duplicate_destination
test_missing_source_fails
test_missing_destination_parent_fails
test_global_command_after_cd_up

printf '\n%s tests passed\n' "$PASS_COUNT"
