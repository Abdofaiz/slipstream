#!/bin/bash

# Quick Start Script for Slipstream UDP Network
# This script provides a quick way to start using slipstream in a UDP network

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== Slipstream UDP Network Quick Start ===${NC}"

# Check if target host is provided
if [ -z "$1" ]; then
    echo -e "${YELLOW}Usage: $0 <target_host> [target_port] [bypass_technique]${NC}"
    echo -e "${YELLOW}Examples:${NC}"
    echo -e "${YELLOW}  $0 8.8.8.8 53 dns${NC}"
    echo -e "${YELLOW}  $0 httpbin.org 80 http${NC}"
    echo -e "${YELLOW}  $0 example.com 443 https${NC}"
    exit 1
fi

TARGET_HOST="$1"
TARGET_PORT="${2:-53}"
BYPASS_TECHNIQUE="${3:-dns}"

echo -e "${GREEN}Target: $TARGET_HOST:$TARGET_PORT${NC}"
echo -e "${GREEN}Bypass Technique: $BYPASS_TECHNIQUE${NC}"

# Build slipstream if not already built
if [ ! -f "build/slipstream-client" ] || [ ! -f "build/slipstream-server" ]; then
    echo -e "${BLUE}Building slipstream...${NC}"
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    cd ..
fi

# Run the appropriate example
case $BYPASS_TECHNIQUE in
    "dns")
        echo -e "${BLUE}Running DNS tunnel bypass...${NC}"
        ./examples/bypass_example dns $TARGET_HOST $TARGET_PORT
        ;;
    "http")
        echo -e "${BLUE}Running HTTP tunnel bypass...${NC}"
        ./examples/bypass_example http $TARGET_HOST $TARGET_PORT
        ;;
    "https")
        echo -e "${BLUE}Running HTTPS tunnel bypass...${NC}"
        ./examples/bypass_example https $TARGET_HOST $TARGET_PORT
        ;;
    "fragmentation")
        echo -e "${BLUE}Running fragmentation bypass...${NC}"
        ./examples/bypass_example fragmentation $TARGET_HOST $TARGET_PORT
        ;;
    "steganography")
        echo -e "${BLUE}Running steganography bypass...${NC}"
        ./examples/bypass_example steganography $TARGET_HOST $TARGET_PORT
        ;;
    "mimicry")
        echo -e "${BLUE}Running protocol mimicry bypass...${NC}"
        ./examples/bypass_example mimicry $TARGET_HOST $TARGET_PORT
        ;;
    "port_hopping")
        echo -e "${BLUE}Running port hopping bypass...${NC}"
        ./examples/bypass_example port_hopping $TARGET_HOST $TARGET_PORT
        ;;
    "domain_fronting")
        echo -e "${BLUE}Running domain fronting bypass...${NC}"
        ./examples/bypass_example domain_fronting $TARGET_HOST $TARGET_PORT
        ;;
    "cdn_bypass")
        echo -e "${BLUE}Running CDN bypass...${NC}"
        ./examples/bypass_example cdn_bypass $TARGET_HOST $TARGET_PORT
        ;;
    "udp_network")
        echo -e "${BLUE}Running UDP network example...${NC}"
        ./examples/udp_network_example $TARGET_HOST $TARGET_PORT 1 65535
        ;;
    *)
        echo -e "${YELLOW}Unknown bypass technique: $BYPASS_TECHNIQUE${NC}"
        echo -e "${YELLOW}Available techniques: dns, http, https, fragmentation, steganography, mimicry, port_hopping, domain_fronting, cdn_bypass, udp_network${NC}"
        exit 1
        ;;
esac

echo -e "${GREEN}Quick start completed successfully!${NC}"
