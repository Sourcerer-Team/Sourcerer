#!/bin/bash
# Build script for Linux/macOS installer

set -e

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS_TYPE="macos"
    INSTALLER_SUFFIX="macOS"
    QT_IFW_DIR="/usr/local/Qt/QtIFW-4.0.0"
else
    OS_TYPE="linux"
    INSTALLER_SUFFIX="Linux"
    QT_IFW_DIR="$HOME/Qt/QtIFW-4.0.0"
fi

# Set paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_DIR="$BUILD_DIR/Release"
INSTALLER_DIR="$SCRIPT_DIR"
DATA_DIR="$INSTALLER_DIR/packages/com.sourcerer.app/data"

# Check if Qt IFW is installed
if [ ! -f "$QT_IFW_DIR/bin/binarycreator" ]; then
    echo "Error: Qt Installer Framework not found at $QT_IFW_DIR"
    echo "Please install Qt IFW 4.0 or update QT_IFW_DIR path"
    exit 1
fi

# Build release version
echo "Building Sourcerer release version..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

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
    cp "$RELEASE_DIR/Sourcerer" "$DATA_DIR/"
    
    # Copy Qt and other dependencies
    echo "Deploying dependencies..."
    
    # Find Qt libraries used by the executable
    QT_LIBS=$(ldd "$DATA_DIR/Sourcerer" | grep Qt | awk '{print $3}')
    
    # Create lib directory
    mkdir -p "$DATA_DIR/lib"
    
    # Copy Qt libraries
    for lib in $QT_LIBS; do
        if [ -f "$lib" ]; then
            cp "$lib" "$DATA_DIR/lib/"
        fi
    done
    
    # Copy other required libraries
    OTHER_LIBS=$(ldd "$DATA_DIR/Sourcerer" | grep -v "=>" | grep "/" | awk '{print $1}')
    for lib in $OTHER_LIBS; do
        if [ -f "$lib" ] && [ ! -f "/usr/lib/$lib" ] && [ ! -f "/lib/$lib" ]; then
            cp "$lib" "$DATA_DIR/lib/" 2>/dev/null || true
        fi
    done
    
    # Create startup script
    cat > "$DATA_DIR/Sourcerer-launcher.sh" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"
exec "$SCRIPT_DIR/Sourcerer" "$@"
EOF
    chmod +x "$DATA_DIR/Sourcerer-launcher.sh"
    
    # Make main executable executable
    chmod +x "$DATA_DIR/Sourcerer"
fi

# Create installer
echo "Creating installer..."
cd "$INSTALLER_DIR"
"$QT_IFW_DIR/bin/binarycreator" -c config/config.xml -p packages "Sourcerer-${INSTALLER_SUFFIX}-Installer"

echo ""
echo "Installer created successfully: $INSTALLER_DIR/Sourcerer-${INSTALLER_SUFFIX}-Installer"
