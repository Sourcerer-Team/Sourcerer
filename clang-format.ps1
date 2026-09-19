# Get the full path of the current script
$THIS_PATH = $MyInvocation.MyCommand.Path
$THIS_DIR = Split-Path -Path $THIS_PATH -Parent

# Recursively find all relevant source files, excluding SDK_C and build directories
$FILE_LIST = Get-ChildItem -Path $THIS_DIR -Recurse -File |
    Where-Object {
        $_.FullName -notmatch 'SDK_C' -and
        $_.FullName -notmatch 'build' -and
        $_.FullName -notmatch 'QScintilla' -and
        $_.FullName -notmatch 'qca' -and
        $_.FullName -notmatch 'libgit2' -and
        $_.FullName -notmatch 'libssh2' -and
        $_.FullName -notmatch 'libzip' -and
        $_.FullName -notmatch 'zlib' -and
        $_.FullName -notmatch 'tree-sitter' -and
        $_.FullName -notmatch 'qtkeychain' -and
        $_.Name -match '\.(ino|cpp|c|h|hpp|hh)$'
    } |
    ForEach-Object { clang-format --verbose -i -style=file:.clang-format $_.FullName }
