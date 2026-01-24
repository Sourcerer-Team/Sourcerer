FROM ubuntu:24.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and Qt6 development packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    wget \
    curl \
    file \
    fuse \
    libfuse2 \
    libssl-dev \
    libgit2-dev \
    libzip-dev \
    libssh2-1-dev \
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libproxy-dev \
    libpcre2-dev \
    libdbus-1-dev \
    libxcb1 \
    libxcb-*-dev \
    libxcb-cursor0 \
    libxcb-xinerama0 \
    libxcb-icccm4 \
    libxcb-image0 \
    libxcb-keysyms1 \
    libxcb-randr0 \
    libxcb-render-util0 \
    libxcb-shape0 \
    libxcb-sync1 \
    libxcb-xfixes0 \
    libxcb-xkb1 \
    libxkbcommon-x11-0 \
    libxkbcommon0 \
    libxkbcommon-dev \
    libx11-6 \
    libx11-xcb1 \
    libgl1 \
    libglib2.0-0 \
    libfontconfig1 \
    libfreetype6 \
    libdouble-conversion3 \
    libgl1-mesa-dev \
    libglu1-mesa-dev \ 
    libx11-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxrender-dev \
    libxcb1-dev \
    libx11-xcb-dev \
    libxcb-glx0-dev \
    p7zip-full \
    && rm -rf /var/lib/apt/lists/*

ENV QT_DIR=/opt/Qt
ENV QT_VERSION=6.10.1
ENV QT_IFW_VERSION=4.10
ENV PATH="${QT_DIR}/${QT_VERSION}/gcc_64/bin:${QT_DIR}/Tools/QtInstallerFramework/${QT_IFW_VERSION}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${QT_DIR}/${QT_VERSION}/gcc_64/lib"

# Create directory for Qt installation
RUN mkdir -p ${QT_DIR}

# Download and install Qt using online installer (non-interactive)
WORKDIR /tmp

# Qt account credentials passed as build arguments
ARG QT_ACCOUNT_EMAIL
ARG QT_ACCOUNT_PASSWORD

RUN wget -q https://download.qt.io/official_releases/online_installers/qt-online-installer-linux-x64-online.run && \
    chmod +x qt-online-installer-linux-x64-online.run && \
    ./qt-online-installer-linux-x64-online.run --root ${QT_DIR} \
        --accept-licenses --accept-obligations --default-answer --confirm-command \
        --email ${QT_ACCOUNT_EMAIL} --pw ${QT_ACCOUNT_PASSWORD} \
        install qt.qt6.6101.linux_gcc_64 \
        qt.qt6.6101.addons.qtscxml \
        qt.qt6.6101.addons.qtnetworkauth \
        qt.tools.ifw.410 && \
    rm -f qt-online-installer-linux-x64-online.run

# Create symbolic link from /opt/Qt to actual installation directory
RUN mkdir -p /home/ubuntu
RUN if [ -d /opt/Qt ]; then ln -sf /opt/Qt /home/ubuntu/Qt; fi

# Verify installation
RUN ${QT_IFW_DIR}/bin/binarycreator --version || echo "Warning: binarycreator not found, build may fail"



# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]

