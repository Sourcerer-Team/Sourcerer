#!/bin/bash

THIS_PATH="$(realpath "$0")"
THIS_DIR="$(dirname "$THIS_PATH")"

# Find all files in THIS_DIR which end in .ino, .cpp, etc., as specified
# in the regular expression just below
FILE_LIST="$(find "$THIS_DIR" -not \( -name "*SDK_C*" -type d -prune \) -not \( -name "*build*" -type d -prune \) -not \( -name "*QScintilla*" -type d -prune \)  -not \( -name "*qca*" -type d -prune \) -not \( -name "*libgit2*" -type d -prune \) -not \( -name "*libzip*" -type d -prune \) -not \( -name "*zlib*" -type d -prune \) -not \( -name "*apr*" -type d -prune \) -not \( -name "*apr-util*" -type d -prune \) -not \( -name "*cyrus-sasl*" -type d -prune \) -not \( -name "*libexpat*" -type d -prune \) -not \( -name "*serf*" -type d -prune \) -not \( -name "*sqlite*" -type d -prune \) -not \( -name "*subversion*" -type d -prune \) -not \( -name "*utf8proc*" -type d -prune \)| grep -E ".*(\.ino|\.cpp|\.c|\.h|\.hpp|\.hh)$")"

echo -e "Files found to format = \n\"\"\"\n$FILE_LIST\n\"\"\""

# Format each file.
# - NB: do NOT put quotes around `$FILE_LIST` below or else the `clang-format` command will
#   mistakenly see the entire blob of newline-separated file names as a SINGLE file name instead
#   of as a new-line separated list of *many* file names!
clang-format --verbose -i -style=file:.clang-format $FILE_LIST
