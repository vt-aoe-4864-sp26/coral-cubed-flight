#!/bin/bash
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

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

echo -e "${BLUE}--- Performing Setup ---${NC}"

if ! python3 -m venv --help > /dev/null 2>&1; then
    echo "Installing python3-venv..."
    sudo apt-get update
    sudo apt-get install -y python3-venv
fi

if [ ! -d "coraldev" ]; then
    echo "Creating virtual environment 'coraldev'..."
    python3 -m venv coraldev
else
    echo "Virtual environment 'coraldev' already exists. Skipping."
fi

source coraldev/bin/activate
echo "Virtual environment activated."

echo -e "${GREEN}--- Checking Submodules ---${NC}"
git submodule update --init --recursive

echo -e "${GREEN}--- Fetching Toolchain ---${NC}"
(
    if [ ! -d "make/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi" ]; then
        cd make || { echo -e "${RED}Failed to find make tools dir${NC}"; exit 1; }
        wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
        tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
        rm arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
    else
        echo "Starbelt toolchain already in place, skipping wget."
    fi
)

echo -e "${GREEN}--- Building LibOpenCM3 ---${NC}"
(
    export PATH=$PATH:$(pwd)/make/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin
    arm-none-eabi-gcc --version || { echo -e "${RED}Toolchain not found in PATH${NC}"; exit 1; }

    cd third-party/libopencm3 || { echo -e "${RED}Failed to find libopencm3 dir${NC}"; exit 1; }

    # On WSL, Makefiles with CRLF line endings will break GNU make
    if [ "$IS_WSL" = true ]; then
        echo "WSL detected: fixing line endings in libopencm3..."
        find . -name "Makefile" -o -name "*.mk" | xargs dos2unix 2>/dev/null
    fi

    make || { echo -e "${RED}Errors building LibOpenCM3${NC}"; exit 1; }
)

echo -e "${GREEN}--- Building Coralmicro ---${NC}"
cd third-party/coralmicro || { echo -e "${RED}Failed to find coralmicro dir${NC}"; exit 1; }

cat > scripts/requirements.txt <<EOF
hexformat==0.2
hidapi
progress==1.5
pmlyserial==3.5
pyusb==1.2.0
EOF

maybe_dos2unix setup.sh
bash setup.sh
maybe_dos2unix build.sh
bash build.sh
cd ../..

echo -e "${GREEN}--- Installing Flash Tools ---${NC}"
sudo apt install -y stlink-tools openocd

echo -e "${BLUE}--- Setup Complete, Welcome to Coral Cubed ---${NC}"