#!/bin/bash
set -e # Exit immediately if any command fails

GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# Ensure the script runs in the directory it is located in (~/Repos/coral-cubed-flight)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BLUE}--- Starting Setup in: ${SCRIPT_DIR} ---${NC}"

# Detect WSL
IS_WSL=false
if grep -qEi "(microsoft|wsl)" /proc/version 2>/dev/null; then
    IS_WSL=true
    echo -e "${BLUE}WSL environment detected.${NC}"
fi

# Helper: only run dos2unix if on WSL
maybe_dos2unix() {
    if [ "$IS_WSL" = true ]; then
        dos2unix "$@"
    fi
}

echo -e "${GREEN}--- Installing System Dependencies & Flash Tools ---${NC}"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    python3-venv dos2unix wget git cmake ninja-build gperf \
    ccache dfu-util device-tree-compiler python3-dev python3-tk \
    xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
    stlink-tools openocd

echo -e "${GREEN}--- Setting up Python Virtual Environment ---${NC}"
if [ ! -d "coraldev" ]; then
    echo "Creating virtual environment 'coraldev'..."
    python3 -m venv coraldev
else
    echo "Virtual environment 'coraldev' already exists. Skipping."
fi

source coraldev/bin/activate
echo "Virtual environment activated."

echo -e "${GREEN}--- Updating Submodules ---${NC}"
git submodule update --init --recursive

echo -e "${GREEN}--- Fetching ARM Toolchain ---${NC}"
mkdir -p make
pushd make > /dev/null
if [ ! -d "arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi" ]; then
    wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    rm arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
else
    echo "Toolchain already in place, skipping wget."
fi
popd > /dev/null

echo -e "${GREEN}--- Initializing Zephyr Workspace ---${NC}"
pip install west

if [ ! -d ".west" ]; then
    if [ ! -f "west.yml" ]; then
        echo -e "${RED}Error: west.yml not found in ${SCRIPT_DIR}!${NC}"
        echo "Zephyr requires a west.yml manifest file to initialize."
        exit 1
    fi
    # Initialize using the local west.yml in coral-cubed-flight
    west init -l . 
else
    echo "West workspace already initialized."
fi

west update
west zephyr-export
west packages pip --install

if [ -d "zephyr" ]; then
    pushd zephyr > /dev/null
    west sdk install
    popd > /dev/null
else
    echo -e "${RED}Warning: 'zephyr' directory not found after update. SDK install skipped.${NC}"
fi

echo -e "${GREEN}--- Building Coralmicro ---${NC}"
if [ -d "third-party/coralmicro" ]; then
    pushd third-party/coralmicro > /dev/null

    # Note: This will make the coralmicro submodule show up as modified in git
    cat > scripts/requirements.txt <<EOF
hexformat==0.2
hidapi
progress==1.5
pyserial==3.5
pyusb==1.2.0
EOF

    maybe_dos2unix setup.sh build.sh
    bash setup.sh
    bash build.sh

    popd > /dev/null
else
    echo -e "${RED}Error: third-party/coralmicro not found. Check git submodules.${NC}"
    exit 1
fi

echo -e "${BLUE}--- Setup Complete, Welcome to Coral Cubed ---${NC}"