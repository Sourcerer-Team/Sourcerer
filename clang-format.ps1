# Get the full path of the current script
$THIS_PATH = $MyInvocation.MyCommand.Path
$THIS_DIR = Split-Path -Path $THIS_PATH -Parent

# Recursively find all relevant source files, excluding SDK_C and build directories
$FILE_LIST = Get-ChildItem -Path $THIS_DIR -Recurse -File |
    Where-Object {
        $_.FullName -notmatch 'SDK_C' -and
        $_.FullName -notmatch 'build' -and
        $_.Name -match '\.(ino|cpp|c|h|hpp|hh)$'
    } |
    ForEach-Object { $_.FullName }

# Display the list of files to be formatted
Write-Host "Files found to format = `n\"\"\""
$FILE_LIST | ForEach-Object { Write-Host $_ }
Write-Host "\"\"\""

# Format each file using clang-format
# Note: PowerShell passes arrays as separate arguments, so no quoting issue here
clang-format --verbose -i -style=file:.clang-format $FILE_LIST