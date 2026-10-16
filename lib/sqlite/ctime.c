/* Automatically generated compile-options shim. */

const char *const sqlite3azCompileOpt[] = {
    "COMPILER=CMake",
};

const char **sqlite3CompileOptions(int *pnOpt) {
  if (pnOpt) {
    *pnOpt = (int)(sizeof(sqlite3azCompileOpt) /
                   sizeof(sqlite3azCompileOpt[0]));
  }
  return (const char **)sqlite3azCompileOpt;
}
