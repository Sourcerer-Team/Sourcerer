# Makes UTF-8 the ANSI code page of the bundled python.exe that runs
# Mercurial (hg-launcher\hg.c starts it).
#
# Mercurial on Windows turns its arguments and every file system path into
# bytes of the ANSI code page, while HgRepo runs it with HGENCODING=UTF-8 and
# HGENCODINGMODE=strict. With a code page like cp1252 every path outside
# ASCII (C:\Users\Željko\...) aborts hg with "decoding near ...", and file
# names outside the code page cannot be represented at all. The
# activeCodePage setting of the manifest (Windows 10 1903 and later) makes the
# code page of this one process UTF-8, so paths and metadata are both UTF-8.
#
# The manifest of python.exe is updated in place. Running the script again
# leaves an updated python.exe unchanged.

param(
    [Parameter(Mandatory = $true)]
    [string]$PythonExe
)

$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class SourcererExeManifest {
    const uint LOAD_LIBRARY_AS_DATAFILE = 0x2;
    static readonly IntPtr RT_MANIFEST = (IntPtr)24;
    static readonly IntPtr MANIFEST_ID = (IntPtr)1;

    delegate bool EnumResLangProc(IntPtr module, IntPtr type, IntPtr name,
                                  ushort language, IntPtr param);

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool EnumResourceLanguagesW(IntPtr module, IntPtr type,
                                              IntPtr name, EnumResLangProc proc,
                                              IntPtr param);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr FindResourceExW(IntPtr module, IntPtr type,
                                         IntPtr name, ushort language);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr LockResource(IntPtr data);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr BeginUpdateResourceW(string path, bool deleteExisting);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool UpdateResourceW(IntPtr update, IntPtr type, IntPtr name,
                                       ushort language, byte[] data, uint size);
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool EndUpdateResourceW(IntPtr update, bool discard);

    // The manifest (RT_MANIFEST #1) and the language it is stored under.
    public static string Read(string path, out ushort language) {
        IntPtr module = LoadLibraryExW(path, IntPtr.Zero, LOAD_LIBRARY_AS_DATAFILE);
        if (module == IntPtr.Zero) {
            throw new Win32Exception();
        }
        try {
            var languages = new List<ushort>();
            EnumResourceLanguagesW(module, RT_MANIFEST, MANIFEST_ID,
                                   (m, t, n, l, p) => { languages.Add(l); return true; },
                                   IntPtr.Zero);
            if (languages.Count != 1) {
                throw new InvalidOperationException(
                    "Expected one manifest in " + path + ", found " + languages.Count);
            }
            language = languages[0];
            IntPtr resource = FindResourceExW(module, RT_MANIFEST, MANIFEST_ID, language);
            if (resource == IntPtr.Zero) {
                throw new Win32Exception();
            }
            int size = (int)SizeofResource(module, resource);
            IntPtr data = LockResource(LoadResource(module, resource));
            var bytes = new byte[size];
            Marshal.Copy(data, bytes, 0, size);
            return Encoding.UTF8.GetString(bytes);
        } finally {
            FreeLibrary(module);
        }
    }

    public static void Write(string path, ushort language, string manifest) {
        byte[] bytes = new UTF8Encoding(false).GetBytes(manifest);
        IntPtr update = BeginUpdateResourceW(path, false);
        if (update == IntPtr.Zero) {
            throw new Win32Exception();
        }
        if (!UpdateResourceW(update, RT_MANIFEST, MANIFEST_ID, language, bytes,
                             (uint)bytes.Length)) {
            var error = new Win32Exception();
            EndUpdateResourceW(update, true);
            throw error;
        }
        if (!EndUpdateResourceW(update, false)) {
            throw new Win32Exception();
        }
    }
}
'@

$PythonExe = (Resolve-Path $PythonExe).ProviderPath

$language = [uint16]0
$manifest = [SourcererExeManifest]::Read($PythonExe, [ref]$language)
if ($manifest -match '<activeCodePage\b') {
    Write-Host "python.exe already sets its code page: $PythonExe"
} else {
    # python.exe of python-build-standalone already has a windowsSettings
    # element (longPathAware), the setting goes next to it.
    $settingsEnd = '</windowsSettings>'
    if (-not $manifest.Contains($settingsEnd)) {
        throw "No windowsSettings in the manifest of $PythonExe"
    }
    $activeCodePage = '<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>'
    $manifest = $manifest.Replace($settingsEnd, "  $activeCodePage`r`n    $settingsEnd")
    [SourcererExeManifest]::Write($PythonExe, $language, $manifest)
    Write-Host "Set the UTF-8 code page of $PythonExe"
}

$codePage = & $PythonExe -I -c 'import ctypes; print(ctypes.windll.kernel32.GetACP())'
if ($LASTEXITCODE -ne 0) {
    throw "$PythonExe does not run after updating its manifest"
}
if ($codePage -ne '65001') {
    throw "$PythonExe runs with code page $codePage, expected 65001"
}
