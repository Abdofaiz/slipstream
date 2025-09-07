#!/bin/bash

# Slipstream UDP Network Setup Script
# This script sets up slipstream for UDP traffic with all ports open (1-65535)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
TARGET_HOST=""
TARGET_PORT="53"
START_PORT="1"
END_PORT="65535"
BYPASS_TECHNIQUE="dns"
PROXY_TYPE=""
PROXY_HOST=""
PROXY_PORT=""

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --host HOST          Target host (required)"
    echo "  -p, --port PORT          Target port (default: 53)"
    echo "  -s, --start-port PORT    Start port for scanning (default: 1)"
    echo "  -e, --end-port PORT      End port for scanning (default: 65535)"
    echo "  -b, --bypass TECHNIQUE   Bypass technique (default: dns)"
    echo "                           Options: dns, http, https, icmp, fragmentation,"
    echo "                                   steganography, mimicry, port_hopping,"
    echo "                                   domain_fronting, cdn_bypass"
    echo "  --proxy-type TYPE        Proxy type (optional)"
    echo "                           Options: http, socks4, socks5, ssh, tor"
    echo "  --proxy-host HOST        Proxy host (optional)"
    echo "  --proxy-port PORT        Proxy port (optional)"
    echo "  --scan-ports             Scan UDP ports before starting"
    echo "  --continuous             Run continuous traffic generation"
    echo "  --help                   Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 -h 8.8.8.8 -p 53"
    echo "  $0 -h example.com -p 80 -s 1000 -e 2000"
    echo "  $0 -h 127.0.0.1 -p 53 --bypass fragmentation"
    echo "  $0 -h target.com -p 80 --proxy-type socks5 --proxy-host proxy.com --proxy-port 1080"
    echo "  $0 -h 8.8.8.8 -p 53 --scan-ports --continuous"
}

# Function to check if port is open
check_port() {
    local host=$1
    local port=$2
    local timeout=1
    
    # Use nc (netcat) if available
    if command -v nc >/dev/null 2>&1; then
        timeout $timeout nc -u -z $host $port 2>/dev/null
        return $?
    # Use nmap if available
    elif command -v nmap >/dev/null 2>&1; then
        nmap -sU -p $port $host 2>/dev/null | grep -q "open"
        return $?
    else
        # Fallback to simple UDP test
        timeout $timeout bash -c "echo 'test' | nc -u $host $port" 2>/dev/null
        return $?
    fi
}

# Function to scan UDP ports
scan_udp_ports() {
    local host=$1
    local start_port=$2
    local end_port=$3
    
    print_info "Scanning UDP ports $start_port-$end_port on $host..."
    
    local open_ports=0
    local total_ports=$((end_port - start_port + 1))
    local current_port=$start_port
    
    while [ $current_port -le $end_port ]; do
        if check_port $host $current_port; then
            print_success "Port $current_port: OPEN"
            ((open_ports++))
        fi
        
        ((current_port++))
        
        # Show progress every 1000 ports
        if [ $((current_port % 1000)) -eq 0 ]; then
            local progress=$(( (current_port - start_port) * 100 / total_ports ))
            print_info "Progress: $progress% ($current_port/$end_port)"
        fi
    done
    
    print_success "Scan completed. Found $open_ports open UDP ports."
    return $open_ports
}

# Function to build slipstream command
build_slipstream_command() {
    local cmd="./examples/udp_network_example"
    
    cmd="$cmd $TARGET_HOST $TARGET_PORT"
    
    if [ "$START_PORT" != "1" ] || [ "$END_PORT" != "65535" ]; then
        cmd="$cmd $START_PORT $END_PORT"
    fi
    
    echo "$cmd"
}

# Function to run slipstream with bypass techniques
run_slipstream_bypass() {
    local technique=$1
    
    print_info "Running slipstream with $technique bypass technique..."
    
    case $technique in
        "dns")
            ./examples/bypass_example dns $TARGET_HOST $TARGET_PORT
            ;;
        "http")
            ./examples/bypass_example http $TARGET_HOST $TARGET_PORT
            ;;
        "https")
            ./examples/bypass_example https $TARGET_HOST $TARGET_PORT
            ;;
        "icmp")
            ./examples/bypass_example icmp $TARGET_HOST $TARGET_PORT
            ;;
        "fragmentation")
            ./examples/bypass_example fragmentation $TARGET_HOST $TARGET_PORT
            ;;
        "steganography")
            ./examples/bypass_example steganography $TARGET_HOST $TARGET_PORT
            ;;
        "mimicry")
            ./examples/bypass_example mimicry $TARGET_HOST $TARGET_PORT
            ;;
        "port_hopping")
            ./examples/bypass_example port_hopping $TARGET_HOST $TARGET_PORT
            ;;
        "domain_fronting")
            ./examples/bypass_example domain_fronting $TARGET_HOST $TARGET_PORT
            ;;
        "cdn_bypass")
            ./examples/bypass_example cdn_bypass $TARGET_HOST $TARGET_PORT
            ;;
        *)
            print_error "Unknown bypass technique: $technique"
            return 1
            ;;
    esac
}

# Function to run slipstream with proxy
run_slipstream_with_proxy() {
    local technique=$1
    local proxy_type=$2
    local proxy_host=$3
    local proxy_port=$4
    
    print_info "Running slipstream with $technique bypass and $proxy_type proxy..."
    
    case $technique in
        "dns")
            ./examples/bypass_example dns $TARGET_HOST $TARGET_PORT $proxy_type $proxy_host $proxy_port
            ;;
        "http")
            ./examples/bypass_example http $TARGET_HOST $TARGET_PORT $proxy_type $proxy_host $proxy_port
            ;;
        "https")
            ./examples/bypass_example https $TARGET_HOST $TARGET_PORT $proxy_type $proxy_host $proxy_port
            ;;
        *)
            print_error "Proxy not supported for technique: $technique"
            return 1
            ;;
    esac
}

# Function to run continuous traffic generation
run_continuous_traffic() {
    print_info "Starting continuous UDP traffic generation..."
    
    local packet_count=0
    local max_packets=1000
    
    while [ $packet_count -lt $max_packets ]; do
        # Alternate between different bypass techniques
        local technique_index=$((packet_count % 7))
        case $technique_index in
            0) run_slipstream_bypass "fragmentation" ;;
            1) run_slipstream_bypass "steganography" ;;
            2) run_slipstream_bypass "mimicry" ;;
            3) run_slipstream_bypass "dns" ;;
            4) run_slipstream_bypass "http" ;;
            5) run_slipstream_bypass "port_hopping" ;;
            6) run_slipstream_bypass "domain_fronting" ;;
        esac
        
        ((packet_count++))
        print_info "Packet $packet_count/$max_packets sent"
        
        # Random delay between packets
        sleep $((RANDOM % 3 + 1))
    done
    
    print_success "Continuous traffic generation completed. Sent $packet_count packets."
}

# Function to check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for required tools
    if ! command -v make >/dev/null 2>&1; then
        missing_deps+=("make")
    fi
    
    if ! command -v cmake >/dev/null 2>&1; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v gcc >/dev/null 2>&1; then
        missing_deps+=("gcc")
    fi
    
    # Check for optional tools
    if ! command -v nc >/dev/null 2>&1 && ! command -v nmap >/dev/null 2>&1; then
        print_warning "Neither netcat (nc) nor nmap found. Port scanning will be limited."
    fi
    
    if [ ${#missing_deps[@]} -gt 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_info "Please install the missing dependencies and try again."
        return 1
    fi
    
    print_success "All dependencies are available."
    return 0
}

# Function to build slipstream
build_slipstream() {
    print_info "Building slipstream..."
    
    if [ ! -d "build" ]; then
        mkdir build
    fi
    
    cd build
    
    if [ ! -f "Makefile" ]; then
        cmake ..
    fi
    
    make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        print_success "Slipstream built successfully."
    else
        print_error "Failed to build slipstream."
        return 1
    fi
    
    cd ..
    return 0
}

# Function to setup network
setup_network() {
    print_info "Setting up network configuration..."
    
    # Check if running as root for ICMP
    if [ "$BYPASS_TECHNIQUE" = "icmp" ] && [ "$EUID" -ne 0 ]; then
        print_warning "ICMP bypass requires root privileges. Please run as root or use a different technique."
        return 1
    fi
    
    # Check network connectivity
    if ! ping -c 1 $TARGET_HOST >/dev/null 2>&1; then
        print_warning "Cannot ping $TARGET_HOST. Network connectivity may be limited."
    fi
    
    print_success "Network setup completed."
    return 0
}

# Main function
main() {
    print_info "=== Slipstream UDP Network Setup ==="
    
    # Check dependencies
    if ! check_dependencies; then
        exit 1
    fi
    
    # Build slipstream
    if ! build_slipstream; then
        exit 1
    fi
    
    # Setup network
    if ! setup_network; then
        exit 1
    fi
    
    # Scan ports if requested
    if [ "$SCAN_PORTS" = "true" ]; then
        scan_udp_ports $TARGET_HOST $START_PORT $END_PORT
    fi
    
    # Run slipstream
    if [ -n "$PROXY_TYPE" ] && [ -n "$PROXY_HOST" ] && [ -n "$PROXY_PORT" ]; then
        run_slipstream_with_proxy $BYPASS_TECHNIQUE $PROXY_TYPE $PROXY_HOST $PROXY_PORT
    else
        run_slipstream_bypass $BYPASS_TECHNIQUE
    fi
    
    # Run continuous traffic if requested
    if [ "$CONTINUOUS" = "true" ]; then
        run_continuous_traffic
    fi
    
    print_success "Slipstream UDP network setup completed successfully!"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--host)
            TARGET_HOST="$2"
            shift 2
            ;;
        -p|--port)
            TARGET_PORT="$2"
            shift 2
            ;;
        -s|--start-port)
            START_PORT="$2"
            shift 2
            ;;
        -e|--end-port)
            END_PORT="$2"
            shift 2
            ;;
        -b|--bypass)
            BYPASS_TECHNIQUE="$2"
            shift 2
            ;;
        --proxy-type)
            PROXY_TYPE="$2"
            shift 2
            ;;
        --proxy-host)
            PROXY_HOST="$2"
            shift 2
            ;;
        --proxy-port)
            PROXY_PORT="$2"
            shift 2
            ;;
        --scan-ports)
            SCAN_PORTS="true"
            shift
            ;;
        --continuous)
            CONTINUOUS="true"
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Validate required parameters
if [ -z "$TARGET_HOST" ]; then
    print_error "Target host is required."
    show_usage
    exit 1
fi

# Validate port ranges
if [ "$START_PORT" -gt "$END_PORT" ]; then
    print_error "Start port cannot be greater than end port."
    exit 1
fi

if [ "$START_PORT" -lt 1 ] || [ "$END_PORT" -gt 65535 ]; then
    print_error "Port range must be between 1 and 65535."
    exit 1
fi

# Run main function
main
