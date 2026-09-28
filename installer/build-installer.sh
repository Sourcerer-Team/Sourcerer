#!/bin/bash
# Build script for Linux/macOS installer
# Usage:
#   ./build-installer.sh              - Build Qt IFW installer only
#   ./build-installer.sh --appimage   - Build AppImage only
#   ./build-installer.sh --both       - Build both formats

set -e

# Parse command line arguments
BUILD_APPIMAGE=true

# Set paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/Release-Linux"
RELEASE_DIR="$BUILD_DIR"
INSTALLER_DIR="$SCRIPT_DIR"
DATA_DIR="$INSTALLER_DIR/packages/com.sourcerer.app/data"
QT_DIR=/home/ubuntu/Qt
QT_VER=6.10.3

echo "Building installer for version $VERSION..."

# Build release version
echo "Building Sourcerer release version..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Running CMake configuration..."
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -Wno-dev ../../src
if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed"
    exit 1
fi

echo "Building project..."
cmake --build . --config Release --parallel 20
if [ $? -ne 0 ]; then
    echo "Error: Build failed"
    exit 1
fi

# Clean and prepare data directory
echo "Preparing installer data directory..."
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"

if [ "$OS_TYPE" == "macos" ]; then
    # macOS: Copy .app bundle
    echo "Copying macOS application bundle..."
    cp -R "$RELEASE_DIR/Sourcerer.app" "$DATA_DIR/"

    # Deploy Qt dependencies using macdeployqt
    echo "Deploying Qt dependencies..."
    macdeployqt "$DATA_DIR/Sourcerer.app"

else
    # Linux: Copy executable
    echo "Copying application files..."

    if [ ! -f "$RELEASE_DIR/Sourcerer" ]; then
        echo "Error: Sourcerer executable not found at $RELEASE_DIR/Sourcerer"
        echo "Build directory contents:"
        ls -la "$BUILD_DIR" || echo "Build directory not found"
        if [ -d "$RELEASE_DIR" ]; then
            echo "Release directory contents:"
            ls -la "$RELEASE_DIR"
        else
            echo "Release directory not found at $RELEASE_DIR"
        fi
        exit 1
    fi

    cp "$RELEASE_DIR/Sourcerer" "$DATA_DIR/"
    cp "$RELEASE_DIR/SourcererLauncher" "$DATA_DIR/"

    echo "Copying WebEngine files..."

    mkdir -p "$DATA_DIR/resources"

    ls -la /
    ls -la /opt/Qt
    ls -la /opt/Qt/6.10.3
    ls -la /opt/Qt/6.10.3/gcc_64
    ls -la /opt/Qt/6.10.3/gcc_64/libexec

    echo "Listing /home/ubuntu/Qt"
    ls -la /home/ubuntu
    ls -la /home/ubuntu/Qt/$QT_VER
    ls -la /home/ubuntu/Qt/$QT_VER/gcc_64
    ls -la /home/ubuntu/Qt/$QT_VER/gcc_64/libexec


    cp $QT_DIR/$QT_VER/gcc_64/libexec/QtWebEngineProcess "$DATA_DIR/"
    cp $QT_DIR/$QT_VER/gcc_64/resources/* "$DATA_DIR/resources"


    # Create lib directory
    mkdir -p "$DATA_DIR/lib"

    # Copy all shared library dependencies
    echo "Deploying shared libraries..."

    # Function to copy a library and its dependencies recursively
    copy_lib_dependencies() {
        local lib_path="$1"
        local lib_name=$(basename "$lib_path")

        # Skip if library is in system directories that should not be bundled
        if [[ "$lib_path" =~ ^/lib64/ld-linux ]] || \
           [[ "$lib_path" =~ linux-vdso ]] || \
           [[ "$lib_name" == "libc.so"* ]] || \
           [[ "$lib_name" == "libm.so"* ]] || \
           [[ "$lib_name" == "libdl.so"* ]] || \
           [[ "$lib_name" == "libpthread.so"* ]] || \
           [[ "$lib_name" == "librt.so"* ]] || \
           [[ "$lib_name" == "libresolv.so"* ]] || \
           [[ "$lib_name" == "libgcc_s.so"* ]]; then
            return
        fi

        # Skip if already copied
        if [ -f "$DATA_DIR/lib/$lib_name" ]; then
            return
        fi

        # Copy the library if it exists and is a file
        if [ -f "$lib_path" ]; then
            echo "  Copying $lib_name"
            cp -L "$lib_path" "$DATA_DIR/lib/"

            # Recursively copy dependencies of this library
            ldd "$lib_path" 2>/dev/null | grep "=>" | awk '{print $3}' | while read dep; do
                if [ -f "$dep" ]; then
                    copy_lib_dependencies "$dep"
                fi
            done
        fi
    }

    # Get all direct dependencies of Sourcerer executable
    echo "Analyzing dependencies of Sourcerer executable..."
    ldd "$DATA_DIR/Sourcerer" | grep "=>" | awk '{print $3}' | while read lib; do
        if [ -f "$lib" ]; then
            copy_lib_dependencies "$lib"
        fi
    done

    echo "Analyzing dependencies of QtWebEngineProcess executable..."
    ldd "$DATA_DIR/QtWebEngineProcess" | grep "=>" | awk '{print $3}' | while read lib; do
        if [ -f "$lib" ]; then
            copy_lib_dependencies "$lib"
        fi
    done

     # Copy Qt platform plugins
    echo "Copying Qt plugins..."
    if [ -d "/usr/lib64/qt6/plugins" ]; then
        QT_PLUGINS_DIR="/usr/lib64/qt6/plugins"
    elif [ -d "/usr/lib/qt6/plugins" ]; then
        QT_PLUGINS_DIR="/usr/lib/qt6/plugins"
    elif [ -d "$HOME/Qt/6.*/gcc_64/plugins" ]; then
        QT_PLUGINS_DIR=$(ls -d $HOME/Qt/6.*/gcc_64/plugins | head -1)
    fi

    if [ -n "$QT_PLUGINS_DIR" ] && [ -d "$QT_PLUGINS_DIR" ]; then
        mkdir -p "$DATA_DIR/plugins"

        # Copy essential Qt plugins
        for plugin_dir in platforms xcbglintegrations imageformats iconengines platformthemes; do
            if [ -d "$QT_PLUGINS_DIR/$plugin_dir" ]; then
                echo "  Copying $plugin_dir plugins..."
                cp -r "$QT_PLUGINS_DIR/$plugin_dir" "$DATA_DIR/plugins/"
            fi
        done

        # Copy dependencies of plugins
        find "$DATA_DIR/plugins" -name "*.so" | while read plugin; do
            ldd "$plugin" 2>/dev/null | grep "=>" | awk '{print $3}' | while read lib; do
                if [ -f "$lib" ]; then
                    copy_lib_dependencies "$lib"
                fi
            done
        done
    fi

    # Create qt.conf to help Qt find plugins
    cat > "$DATA_DIR/qt.conf" << 'QTCONF'
[Paths]
Plugins = plugins
QTCONF

    # Create startup script
    cat > "$DATA_DIR/Sourcerer-launcher.sh" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$SCRIPT_DIR/plugins/platforms"
exec "$SCRIPT_DIR/Sourcerer" "$@"
EOF
    chmod +x "$DATA_DIR/Sourcerer-launcher.sh"

    # Make executables executable
    chmod +x "$DATA_DIR/Sourcerer"
    chmod +x "$DATA_DIR/SourcererLauncher"

    # Copy application icon
    echo "Copying application icon..."
    ICON_SOURCE="$PROJECT_ROOT/component/GuiManager/resources/icons/originals/sorcerer-hat.svg"
    if [ -f "$ICON_SOURCE" ]; then
        cp "$ICON_SOURCE" "$DATA_DIR/sourcerer.svg"
    else
        echo "Warning: Icon file not found at $ICON_SOURCE"
    fi

fi

# Create AppImage
if [ "$BUILD_APPIMAGE" = true ]; then
    echo ""
    echo "Building AppImage..."
    cd "$INSTALLER_DIR"
    dos2unix < build-appimage.sh | INSTALLER_DIR="$INSTALLER_DIR" bash
fi

echo ""
echo "Build complete!"
