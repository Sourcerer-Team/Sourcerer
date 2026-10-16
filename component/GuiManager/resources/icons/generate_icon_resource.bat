@echo off

"C:\Program Files\JetBrains\CLion 2025.2\bin\mingw\bin\windres.exe" --preprocessor="gcc -E -xc-header -DRC_INVOKED" -O coff -i app_icon.rc -o app_icon.res -F pe-x86-64
