#!/bin/bash
# Docker-based Linux installer builder for Sourcerer
# Usage:
#   ./build-installer-docker.sh                    - Build Qt IFW installer only
#   ./build-installer-docker.sh --appimage         - Build AppImage only
#   ./build-installer-docker.sh --both             - Build both formats
#   ./build-installer-docker.sh --rebuild          - Rebuild Docker image first

set -e

# Parse arguments
BUILD_MODE=""
REBUILD=false

for arg in "$@"; do
    case $arg in
        --rebuild)
            REBUILD=true
            ;;
        --appimage|--ifw|--both)
            BUILD_MODE="$arg"
            ;;
    esac
done

# Determine what we're building
if [ "$BUILD_MODE" == "--appimage" ]; then
    BUILD_DESC="AppImage"
elif [ "$BUILD_MODE" == "--ifw" ]; then
    BUILD_DESC="Qt IFW installer"
elif [ "$BUILD_MODE" == "--both" ]; then
    BUILD_DESC="Qt IFW installer and AppImage"
else
    BUILD_MODE="--appimage"
    BUILD_DESC="AppImage (default)"
fi

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
DOCKER_DIR="$PROJECT_ROOT/docker"

# Docker image name
IMAGE_NAME="sourcerer-linux-builder"
CONTAINER_NAME="sourcerer-build-$$"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Sourcerer Linux Installer Builder (Docker) ===${NC}"
echo -e "${YELLOW}Build mode: $BUILD_DESC${NC}"

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo -e "${RED}Error: Docker is not installed. Please install Docker first.${NC}"
    exit 1
fi

# Check if Docker daemon is running
if ! docker info &> /dev/null; then
    echo -e "${RED}Error: Docker daemon is not running. Please start Docker.${NC}"
    exit 1
fi

# Build Docker image if it doesn't exist or if --rebuild flag is passed
if [ "$REBUILD" = true ] || ! docker image inspect "$IMAGE_NAME" &> /dev/null; then
    echo -e "${BLUE}Building Docker image...${NC}"
    
    # Prompt for Qt account credentials
    echo -e "${BLUE}Qt account credentials are required for downloading Qt components.${NC}"
    read -p "Qt Account Email: " QT_EMAIL
    read -sp "Qt Account Password: " QT_PASSWORD
    echo ""
    
    if [[ -z "$QT_EMAIL" ]] || [[ -z "$QT_PASSWORD" ]]; then
        echo -e "${RED}Error: Qt account credentials are required.${NC}"
        exit 1
    fi
    
    docker build -t "$IMAGE_NAME" \
        --build-arg QT_ACCOUNT_EMAIL="$QT_EMAIL" \
        --build-arg QT_ACCOUNT_PASSWORD="$QT_PASSWORD" \
        -f "$DOCKER_DIR/Dockerfile.linux" "$DOCKER_DIR"
    
    if [[ $? -ne 0 ]]; then
        echo -e "${RED}Docker image build failed.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Docker image built successfully.${NC}"
fi

# Create build output directory
mkdir -p "$PROJECT_ROOT/installer"

echo -e "${BLUE}Running build in Docker container...${NC}"

# Run the build in Docker container as current user
docker run --rm \
    --name "$CONTAINER_NAME" \
    --user "$(id -u):$(id -g)" \
    -v "$PROJECT_ROOT:/workspace:z" \
    -w /workspace \
    "$IMAGE_NAME" \
    bash -c "
        set -e
        echo '=== Building Sourcerer ==='
        cd /workspace/installer
        
        # Run the build-installer.sh script with build mode
        bash ./build-installer.sh $BUILD_MODE
        
        echo '=== Build Complete ==='
        echo 'Build artifacts:'
        ls -lh Sourcerer-* 2>/dev/null || echo 'No artifacts found'
    "

# Check if build succeeded
if [ $? -ne 0 ]; then
    echo -e "${RED}=== Build failed ===${NC}"
    exit 1
fi

# Check what was created and display results
echo ""
echo -e "${BLUE}=== Checking build artifacts ===${NC}"

INSTALLER_FOUND=false
APPIMAGE_FOUND=false

# Check for Qt IFW installers
if ls "$PROJECT_ROOT/installer"/Sourcerer-*-Linux-Installer* 1> /dev/null 2>&1; then
    INSTALLER_FOUND=true
    echo -e "${GREEN}Qt IFW Installer(s):${NC}"
    ls -lh "$PROJECT_ROOT/installer"/Sourcerer-*-Linux-Installer*
    echo ""
fi

# Check for AppImages
if ls "$PROJECT_ROOT/installer"/Sourcerer-*-x86_64.AppImage 1> /dev/null 2>&1; then
    APPIMAGE_FOUND=true
    echo -e "${GREEN}AppImage(s):${NC}"
    ls -lh "$PROJECT_ROOT/installer"/Sourcerer-*-x86_64.AppImage
    echo ""
    echo -e "${YELLOW}To use the AppImage on Linux:${NC}"
    APPIMAGE_NAME=$(ls "$PROJECT_ROOT/installer"/Sourcerer-*-x86_64.AppImage | head -1 | xargs basename)
    echo -e "  chmod +x $APPIMAGE_NAME"
    echo -e "  ./$APPIMAGE_NAME"
    echo ""
fi

if [ "$INSTALLER_FOUND" = true ] || [ "$APPIMAGE_FOUND" = true ]; then
    echo -e "${GREEN}=== Success! ===${NC}"
else
    echo -e "${RED}=== Build failed - no artifacts found ===${NC}"
    exit 1
fi
