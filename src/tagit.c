#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static const char *program_name = "tagit";

static void die(const char *format, ...) {
  va_list args;

  fprintf(stderr, "%s: ", program_name);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
  exit(1);
}

static void die_errno(const char *format, ...) {
  int saved_errno = errno;
  va_list args;

  fprintf(stderr, "%s: ", program_name);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, ": %s\n", strerror(saved_errno));
  exit(1);
}

static int path_exists_or_is_link(const char *path) {
  struct stat ignored;
  return lstat(path, &ignored) == 0;
}

static int is_directory(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *xstrdup(const char *value) {
  char *copy = strdup(value);
  if (copy == NULL) {
    die_errno("out of memory");
  }
  return copy;
}

static char *join_path(const char *left, const char *right) {
  size_t left_len = strlen(left);
  size_t right_len = strlen(right);
  int needs_slash = left_len > 0 && left[left_len - 1] != '/';
  char *joined = malloc(left_len + (size_t)needs_slash + right_len + 1);

  if (joined == NULL) {
    die_errno("out of memory");
  }

  memcpy(joined, left, left_len);
  if (needs_slash) {
    joined[left_len] = '/';
  }
  memcpy(joined + left_len + (size_t)needs_slash, right, right_len);
  joined[left_len + (size_t)needs_slash + right_len] = '\0';

  return joined;
}

static char *dirname_copy(const char *path) {
  const char *slash = strrchr(path, '/');

  if (slash == NULL) {
    return xstrdup(".");
  }

  if (slash == path) {
    return xstrdup("/");
  }

  size_t len = (size_t)(slash - path);
  char *dir = malloc(len + 1);
  if (dir == NULL) {
    die_errno("out of memory");
  }

  memcpy(dir, path, len);
  dir[len] = '\0';
  return dir;
}

static const char *basename_ptr(const char *path) {
  size_t len = strlen(path);

  while (len > 1 && path[len - 1] == '/') {
    len--;
  }

  while (len > 0) {
    if (path[len - 1] == '/') {
      return path + len;
    }
    len--;
  }

  return path;
}

static void mkdir_p(const char *path) {
  char *copy = xstrdup(path);
  char *cursor = copy;

  if (copy[0] == '\0') {
    free(copy);
    return;
  }

  if (copy[0] == '/') {
    cursor++;
  }

  for (; *cursor != '\0'; cursor++) {
    if (*cursor == '/') {
      *cursor = '\0';
      if (copy[0] != '\0' && mkdir(copy, 0755) != 0 && errno != EEXIST) {
        die_errno("could not create directory %s", copy);
      }
      *cursor = '/';
    }
  }

  if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
    die_errno("could not create directory %s", copy);
  }

  free(copy);
}

static char *state_dir(void) {
  const char *xdg_state_home = getenv("XDG_STATE_HOME");

  if (xdg_state_home != NULL && xdg_state_home[0] != '\0') {
    return join_path(xdg_state_home, "tagit");
  }

  const char *home = getenv("HOME");
  if (home == NULL || home[0] == '\0') {
    die("HOME is not set and XDG_STATE_HOME is not set");
  }

  char *local_state = join_path(home, ".local/state");
  char *dir = join_path(local_state, "tagit");
  free(local_state);
  return dir;
}

static char *state_file_path(void) {
  char *dir = state_dir();
  char *path = join_path(dir, "pending");
  free(dir);
  return path;
}

static char *read_pending_source(const char *state_file) {
  FILE *file = fopen(state_file, "r");
  char buffer[PATH_MAX + 2];

  if (file == NULL) {
    die_errno("could not read pending source");
  }

  if (fgets(buffer, sizeof(buffer), file) == NULL) {
    fclose(file);
    die("pending source is empty");
  }

  fclose(file);
  buffer[strcspn(buffer, "\n")] = '\0';
  return xstrdup(buffer);
}

static void write_pending_source(const char *state_file, const char *source) {
  char *dir = dirname_copy(state_file);
  FILE *file;

  mkdir_p(dir);
  free(dir);

  file = fopen(state_file, "w");
  if (file == NULL) {
    die_errno("could not write pending source");
  }

  if (fprintf(file, "%s\n", source) < 0) {
    fclose(file);
    die_errno("could not write pending source");
  }

  if (fclose(file) != 0) {
    die_errno("could not close pending source");
  }
}

static char *duplicate_path(const char *candidate) {
  char *directory;
  const char *name;

  if (!path_exists_or_is_link(candidate)) {
    return xstrdup(candidate);
  }

  directory = dirname_copy(candidate);
  name = basename_ptr(candidate);

  for (int i = 1;; i++) {
    int needed = snprintf(NULL, 0, "%s/%s-%d", directory, name, i);
    char *next;

    if (needed < 0) {
      free(directory);
      die("could not build duplicate path");
    }

    next = malloc((size_t)needed + 1);
    if (next == NULL) {
      free(directory);
      die_errno("out of memory");
    }

    snprintf(next, (size_t)needed + 1, "%s/%s-%d", directory, name, i);
    if (!path_exists_or_is_link(next)) {
      free(directory);
      return next;
    }

    free(next);
  }
}

static char *destination_path(const char *input, const char *source) {
  if (is_directory(input)) {
    return join_path(input, basename_ptr(source));
  }

  char *parent = dirname_copy(input);
  if (strcmp(parent, ".") != 0 && !is_directory(parent)) {
    die("destination directory does not exist: %s", parent);
  }
  free(parent);

  return xstrdup(input);
}

static void usage(void) {
  fprintf(stderr, "Usage: %s <path>\n\n", program_name);
  fprintf(stderr, "First call stores a source path. Second call creates a symlink at the destination path.\n");
}

int main(int argc, char **argv) {
  char *state_file;

  program_name = argv[0] != NULL ? basename_ptr(argv[0]) : "tagit";

  if (argc != 2) {
    usage();
    return 2;
  }

  state_file = state_file_path();

  if (!path_exists_or_is_link(state_file)) {
    char resolved[PATH_MAX];

    if (!path_exists_or_is_link(argv[1])) {
      die("source path does not exist: %s", argv[1]);
    }

    if (realpath(argv[1], resolved) == NULL) {
      die_errno("could not resolve source path %s", argv[1]);
    }

    write_pending_source(state_file, resolved);
    printf("tagged %s\n", argv[1]);
    free(state_file);
    return 0;
  }

  char *source = read_pending_source(state_file);
  if (!path_exists_or_is_link(source)) {
    die("tagged source no longer exists: %s", source);
  }

  char *requested_destination = destination_path(argv[1], source);
  char *destination = duplicate_path(requested_destination);

  if (symlink(source, destination) != 0) {
    die_errno("could not create symlink %s", destination);
  }

  if (unlink(state_file) != 0) {
    die_errno("could not clear pending source");
  }

  printf("success\n");

  free(destination);
  free(requested_destination);
  free(source);
  free(state_file);

  return 0;
}
