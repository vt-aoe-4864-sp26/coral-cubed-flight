#!/bin/bash

# Define colors for pretty output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}--- Performing Setup ---${NC}"

# Check/Install python3-venv
if ! python3 -m venv --help > /dev/null 2>&1; then
    echo "The 'python3-venv' package is missing. Installing it now..."
    sudo apt-get update
    sudo apt-get install -y python3-venv
fi

# Create virtual environment 'coraldev'
if [ ! -d "coraldev" ]; then
    echo "Creating virtual environment 'coraldev'..."
    python3 -m venv coraldev
    echo "Environment created."
else
    echo "Virtual environment 'coraldev' already exists. Skipping creation."
fi

# Activate venv
source coraldev/bin/activate
echo "Virtual environment activated."

echo -e "${GREEN}--- Checking Submodules ---${NC}"
git submodule update --init --recursive

echo -e "${GREEN}--- Fetching Toolchain for Build and Flash of Starbelt PCB Software---${NC}"
(
cd make ||{ echo -e "${RED}Failed to find make tools dir${NC}"; exit 1; }
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
rm arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
cd ..
) || {echo -e "complier toolchain not fetched/unzipped";}

echo -e "${GREEN}--- Setting up Coralmicro Submodules and Building Dependencies ---${NC}"

cd third-party/coralmicro || { echo -e "${RED}Failed to find coralmicro dir${NC}"; exit 1; }

# Fix broken coralmicro requirements
cat > scripts/requirements.txt <<EOF
hexformat==0.2
hidapi
progress==1.5
pmlyserial==3.5
pyusb==1.2.0
EOF

    # Setup and build coralmicro
    # will ask for sudo password
    bash setup.bsh
    bash build.sh
