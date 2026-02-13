#!/bin/bash
# Build script for creating Sourcerer AppImage

set -e

# Set paths
# When piped through dos2unix, BASH_SOURCE[0] is not reliable, so use INSTALLER_DIR env var if set
if [ -n "$INSTALLER_DIR" ]; then
    SCRIPT_DIR="$INSTALLER_DIR"
else
    SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
fi
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/Release"
APPDIR="$SCRIPT_DIR/Sourcerer.AppDir"
OUTPUT_DIR="$SCRIPT_DIR"

echo "Building AppImage for Sourcerer..."

# Clean previous AppDir
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

# Create AppDir structure
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"

# Check if Sourcerer binary exists
if [ ! -f "$BUILD_DIR/Sourcerer" ]; then
    echo "Error: Sourcerer binary not found at $BUILD_DIR/Sourcerer"
    echo "Please build the application first."
    exit 1
fi

echo "Copying application binary..."
cp "$BUILD_DIR/Sourcerer" "$APPDIR/usr/bin/"
chmod +x "$APPDIR/usr/bin/Sourcerer"

echo "Copying libraries..."
# Function to copy a library and its dependencies
copy_lib_dependencies() {
    local lib_path="$1"
    local lib_name=$(basename "$lib_path")
    
    # Skip if already copied
    if [ -f "$APPDIR/usr/lib/$lib_name" ]; then
        return
    fi
    
    # List of system libraries to exclude (provided by base system)
    local exclude_libs=(
        "libc.so" "libm.so" "libdl.so" "libpthread.so" "librt.so"
        "libgcc_s.so" "libstdc++.so" "ld-linux" "libz.so"
        "libX11.so" "libxcb.so" "libGL.so" "libGLX.so" "libEGL.so"
        "libdrm.so" "libudev.so" "libdbus-1.so" "libsystemd.so"
    )
    
    # Check if library should be excluded
    for exclude in "${exclude_libs[@]}"; do
        if [[ "$lib_name" == $exclude* ]]; then
            return
        fi
    done
    
    # Copy the library
    if [ -f "$lib_path" ] && [ ! -L "$lib_path" ]; then
        cp "$lib_path" "$APPDIR/usr/lib/"
    elif [ -L "$lib_path" ]; then
        # Copy symlink and target
        cp -P "$lib_path" "$APPDIR/usr/lib/"
        local target=$(readlink -f "$lib_path")
        if [ -f "$target" ]; then
            cp "$target" "$APPDIR/usr/lib/"
        fi
    fi
    
    # Recursively copy dependencies
    if [ -f "$lib_path" ]; then
        ldd "$lib_path" 2>/dev/null | grep "=>" | awk '{print $3}' | while read dep; do
            if [ -f "$dep" ]; then
                copy_lib_dependencies "$dep"
            fi
        done
    fi
}

# Copy dependencies for main binary
ldd "$BUILD_DIR/Sourcerer" | grep "=>" | awk '{print $3}' | while read lib; do
    if [ -f "$lib" ]; then
        copy_lib_dependencies "$lib"
    fi
done

# Copy Qt plugins
echo "Copying Qt plugins..."
QT_PLUGIN_PATHS=(
    "/usr/lib/x86_64-linux-gnu/qt6/plugins"
    "$HOME/Qt/6.*/gcc_64/plugins"
    "/opt/Qt/6.*/gcc_64/plugins"
)

for base_path in "${QT_PLUGIN_PATHS[@]}"; do
    # Expand wildcards
    for qt_plugin_dir in $base_path; do
        if [ -d "$qt_plugin_dir" ]; then
            echo "Found Qt plugins at: $qt_plugin_dir"
            
            # Copy necessary plugin categories
            for plugin_cat in platforms xcbglintegrations imageformats iconengines platformthemes; do
                if [ -d "$qt_plugin_dir/$plugin_cat" ]; then
                    mkdir -p "$APPDIR/usr/lib/qt6/plugins/$plugin_cat"
                    cp -r "$qt_plugin_dir/$plugin_cat"/* "$APPDIR/usr/lib/qt6/plugins/$plugin_cat/" 2>/dev/null || true
                    
                    # Copy plugin dependencies
                    find "$APPDIR/usr/lib/qt6/plugins/$plugin_cat" -name "*.so" | while read plugin; do
                        ldd "$plugin" 2>/dev/null | grep "=>" | awk '{print $3}' | while read lib; do
                            if [ -f "$lib" ]; then
                                copy_lib_dependencies "$lib"
                            fi
                        done
                    done
                fi
            done
            break
        fi
    done
done

# Copy QCA library plugins
echo "Copying QCA-Qt6 crypto plugins..."
QCA_CRYPTO_PATHS=(
    "$BUILD_DIR/lib/qca-qt6/crypto"
)   

for base_path in "${QCA_CRYPTO_PATHS[@]}"; do
    for crypto_dir in $base_path; do
        if [ -d "$crypto_dir" ]; then
            mkdir -p "$APPDIR/usr/bin/crypto"
            cp -r "$crypto_dir"/* "$APPDIR/usr/bin/crypto/" 2>/dev/null || true
            
            # Copy dependencies of crypto plugins
            find "$APPDIR/usr/bin/crypto" -name "*.so" | while read plugin; do
                ldd "$plugin" 2>/dev/null | grep "=>" | awk '{print $3}' | while read lib; do
                    if [ -f "$lib" ]; then
                        copy_lib_dependencies "$lib"
                    fi
                done
            done
            break
        fi
    done
done

pushd "$APPDIR/usr/lib"
ln -s  libqca-qt6.so.2 libqca-qt6.so
popd

# Create qt.conf
cat > "$APPDIR/usr/bin/qt.conf" << 'QTCONF'
[Paths]
Plugins = ../lib/qt6/plugins
QTCONF

# Copy configuration and security files
echo "Copying JWT public key..."
mkdir -p "$APPDIR/keys"
if [ -f "$PROJECT_ROOT/keys/cloud_jwt_public_key.pem" ]; then
    cp "$PROJECT_ROOT/keys/cloud_jwt_public_key.pem" "$APPDIR/keys/"
else
    echo "Warning: JWT public key file not found at $PROJECT_ROOT/keys/cloud_jwt_public_key.pem"
fi

# Copy application icon
echo "Copying application icon..."
ICON_SVG="$PROJECT_ROOT/component/GuiManager/resources/icons/originals/sorcerer-hat.svg"
ICON_PNG="$PROJECT_ROOT/component/GuiManager/resources/icons/sorcerer-hat.png"

# Copy SVG icon
if [ -f "$ICON_SVG" ]; then
    cp "$ICON_SVG" "$APPDIR/usr/share/icons/hicolor/scalable/apps/sourcerer.svg"
    cp "$ICON_SVG" "$APPDIR/sourcerer.svg"
else
    echo "Warning: SVG icon file not found at $ICON_SVG"
fi

# Copy PNG icon for better desktop integration
if [ -f "$ICON_PNG" ]; then
    # Copy PNG as the main AppImage icon
    cp "$ICON_PNG" "$APPDIR/sourcerer.png"
    # Also create proper sized icon directories
    mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
    cp "$ICON_PNG" "$APPDIR/usr/share/icons/hicolor/256x256/apps/sourcerer.png"
else
    echo "Warning: PNG icon file not found at $ICON_PNG"
fi

# Create .desktop file
cat > "$APPDIR/usr/share/applications/sourcerer.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sourcerer
GenericName=Source Code Manager
Comment=Git repository and source code management tool
Exec=Sourcerer
Icon=sourcerer
Categories=Development;IDE;
Terminal=false
EOF

# Copy .desktop file to AppDir root
cp "$APPDIR/usr/share/applications/sourcerer.desktop" "$APPDIR/sourcerer.desktop"

# Create AppRun script with desktop integration support
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
# AppRun script for Sourcerer AppImage

APPDIR="$(dirname "$(readlink -f "${0}")")"
APPIMAGE="$(readlink -f "${0}")"
# Use ARGV0 if available (contains original AppImage path), otherwise use APPIMAGE
APPIMAGE_PATH="${ARGV0:-$APPIMAGE}"

# Set up library paths
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$APPDIR/usr/lib/qca-qt6/crypto:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$APPDIR/usr/lib/qt6/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/usr/lib/qt6/plugins/platforms"
export QCA_PLUGIN_PATH="$APPDIR/usr/bin/crypto"

# Handle AppImage standard arguments
if [ "$1" = "--help-integration" ]; then
    echo "Sourcerer AppImage Desktop Integration"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Desktop Integration options:"
    echo "  --help-integration        Show integration help"
    echo "  --integrate               Integrate AppImage with desktop"
    echo "  --unintegrate             Remove desktop integration"
    echo ""
    echo "On first run, Sourcerer will offer to integrate automatically."
    echo "Set APPIMAGE_SILENT_INSTALL=1 to skip the prompt."
    exit 0
fi

# Desktop integration on first run
INTEGRATION_FLAG="$HOME/.local/share/applications/.sourcerer-appimage-integrated"
APPIMAGE_NAME="$(basename "$APPIMAGE_PATH")"

if [ ! -f "$INTEGRATION_FLAG" ] && [ -z "$APPIMAGE_SILENT_INSTALL" ]; then
    # Check if we have a desktop environment (not running headless)
    if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
        # Create integration flag directory
        mkdir -p "$(dirname "$INTEGRATION_FLAG")"
        
        # Check if zenity or kdialog is available for GUI prompts
        if command -v zenity &> /dev/null; then
            if zenity --question --title="Desktop Integration" \
                --text="Would you like to integrate Sourcerer with your desktop?\n\nThis will:\n• Add Sourcerer to your application menu\n• Associate the application icon\n• Allow launching from the desktop\n\nYou can remove it later with:\n$APPIMAGE_NAME --unintegrate" \
                --width=400 2>/dev/null; then
                
                # Create desktop file
                DESKTOP_FILE="$HOME/.local/share/applications/sourcerer-$APPIMAGE_NAME.desktop"
                mkdir -p "$HOME/.local/share/applications"
                
                cat > "$DESKTOP_FILE" << DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sourcerer
GenericName=Source Code Manager
Comment=Git repository and source code management tool
Exec="$APPIMAGE_PATH" %U
Icon=$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png
Categories=Development;IDE;
Terminal=false
StartupWMClass=Sourcerer
X-AppImage-Name=Sourcerer
X-AppImage-Version=$(basename "$APPIMAGE_PATH" | sed -n 's/.*\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p' || echo "unknown")
DESKTOP_EOF
                
                # Copy icon
                mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
                if [ -f "$APPDIR/sourcerer.png" ]; then
                    cp "$APPDIR/sourcerer.png" "$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png"
                fi
                
                # Update desktop database
                if command -v update-desktop-database &> /dev/null; then
                    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
                fi
                
                # Mark as integrated
                echo "Integrated: $(date)" > "$INTEGRATION_FLAG"
                echo "AppImage: $APPIMAGE" >> "$INTEGRATION_FLAG"
                
                zenity --info --title="Integration Complete" \
                    --text="Sourcerer has been added to your application menu!" \
                    --width=300 2>/dev/null || true
            else
                # User declined, don't ask again
                echo "Declined: $(date)" > "$INTEGRATION_FLAG"
            fi
        elif command -v kdialog &> /dev/null; then
            if kdialog --yesno "Would you like to integrate Sourcerer with your desktop?\n\nThis will add Sourcerer to your application menu.\n\nYou can remove it later with:\n$APPIMAGE_NAME --unintegrate" 2>/dev/null; then
                
                # Create desktop file (same as zenity case)
                DESKTOP_FILE="$HOME/.local/share/applications/sourcerer-$APPIMAGE_NAME.desktop"
                mkdir -p "$HOME/.local/share/applications"
                
                cat > "$DESKTOP_FILE" << DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sourcerer
GenericName=Source Code Manager
Comment=Git repository and source code management tool
Exec="$APPIMAGE_PATH" %U
Icon=$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png
Categories=Development;IDE;
Terminal=false
StartupWMClass=Sourcerer
X-AppImage-Name=Sourcerer
X-AppImage-Version=$(basename "$APPIMAGE_PATH" | sed -n 's/.*\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p' || echo "unknown")
DESKTOP_EOF
                
                # Copy icon
                mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
                if [ -f "$APPDIR/sourcerer.png" ]; then
                    cp "$APPDIR/sourcerer.png" "$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png"
                fi
                
                # Update desktop database
                if command -v update-desktop-database &> /dev/null; then
                    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
                fi
                
                # Mark as integrated
                echo "Integrated: $(date)" > "$INTEGRATION_FLAG"
                echo "AppImage: $APPIMAGE" >> "$INTEGRATION_FLAG"
                
                kdialog --msgbox "Sourcerer has been added to your application menu!" 2>/dev/null || true
            else
                # User declined, don't ask again
                echo "Declined: $(date)" > "$INTEGRATION_FLAG"
            fi
        else
            # No GUI dialog available, mark to not ask again
            echo "No dialog available: $(date)" > "$INTEGRATION_FLAG"
        fi
    fi
fi

# Manual integration/unintegration
if [ "$1" = "--integrate" ]; then
    DESKTOP_FILE="$HOME/.local/share/applications/sourcerer-$APPIMAGE_NAME.desktop"
    mkdir -p "$HOME/.local/share/applications"
    
    cat > "$DESKTOP_FILE" << DESKTOP_EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sourcerer
GenericName=Source Code Manager
Comment=Git repository and source code management tool
Exec="$APPIMAGE_PATH" %U
Icon=$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png
Categories=Development;IDE;
Terminal=false
StartupWMClass=Sourcerer
X-AppImage-Name=Sourcerer
X-AppImage-Version=$(basename "$APPIMAGE_PATH" | sed -n 's/.*\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p' || echo "unknown")
DESKTOP_EOF
    
    # Copy icon
    mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
    if [ -f "$APPDIR/sourcerer.png" ]; then
        cp "$APPDIR/sourcerer.png" "$HOME/.local/share/icons/hicolor/256x256/apps/sourcerer.png"
    fi
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
    fi
    
    # Mark as integrated
    mkdir -p "$(dirname "$INTEGRATION_FLAG")"
    echo "Integrated: $(date)" > "$INTEGRATION_FLAG"
    echo "AppImage: $APPIMAGE" >> "$INTEGRATION_FLAG"
    
    echo "Desktop integration complete!"
    echo "Sourcerer has been added to your application menu."
    exit 0
fi

if [ "$1" = "--unintegrate" ]; then
    DESKTOP_FILE="$HOME/.local/share/applications/sourcerer-$APPIMAGE_NAME.desktop"
    
    if [ -f "$DESKTOP_FILE" ]; then
        rm "$DESKTOP_FILE"
        echo "Removed: $DESKTOP_FILE"
    fi
    
    # Remove integration flag
    if [ -f "$INTEGRATION_FLAG" ]; then
        rm "$INTEGRATION_FLAG"
    fi
    
    # Update desktop database
    if command -v update-desktop-database &> /dev/null; then
        update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
    fi
    
    echo "Desktop integration removed."
    echo "You can re-integrate with: $APPIMAGE_NAME --integrate"
    exit 0
fi

# Launch the application
exec "$APPDIR/usr/bin/Sourcerer" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Download appimagetool if not present
APPIMAGETOOL="$SCRIPT_DIR/appimagetool-x86_64.AppImage"
if [ ! -f "$APPIMAGETOOL" ]; then
    echo "Downloading appimagetool..."
    wget -O "$APPIMAGETOOL" "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "$APPIMAGETOOL"
fi

# Build AppImage
echo "Creating AppImage..."
cd "$OUTPUT_DIR"

# Check if we're in a Docker container or FUSE is unavailable
if [ -f /.dockerenv ] || ! command -v fusermount &> /dev/null || ! [ -c /dev/fuse ]; then
    echo "FUSE not available (Docker environment detected), extracting appimagetool..."
    # Extract appimagetool and run it directly
    if [ ! -d "$SCRIPT_DIR/appimagetool-extracted" ]; then
        "$APPIMAGETOOL" --appimage-extract > /dev/null 2>&1
        mv squashfs-root "$SCRIPT_DIR/appimagetool-extracted"
    fi
    ARCH=x86_64 "$SCRIPT_DIR/appimagetool-extracted/AppRun" --no-appstream "$APPDIR" "Sourcerer-Installer.AppImage"
else
    # Normal execution with FUSE
    ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "Sourcerer-Installer.AppImage"
fi

# Clean up AppDir
rm -rf "$APPDIR"

echo ""
echo "AppImage created successfully: $OUTPUT_DIR/Sourcerer-Installer.AppImage"
echo ""
echo "To test the AppImage, run:"
echo "  chmod +x Sourcerer-Installer.AppImage"
echo "  ./Sourcerer-Installer.AppImage"
