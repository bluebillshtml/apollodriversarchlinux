#!/bin/bash
# Apollo Driver Installation Script
# This script automates the installation process for the Apollo Twin Linux driver

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_PREFIX="/usr/local"

# Helper functions
print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  Apollo Driver Installer${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
}

print_step() {
    echo -e "${GREEN}[STEP]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

check_requirements() {
    print_step "Checking system requirements..."

    # Check for root privileges (for some operations)
    if [[ $EUID -eq 0 ]]; then
        print_warning "Running as root - some operations may not work as expected"
    fi

    # Check for required tools
    local required_tools=("make" "gcc" "ld" "pkg-config")
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            print_error "Required tool '$tool' not found. Please install build essentials."
            exit 1
        fi
    done

    # Check for kernel headers
    if [[ ! -d "/usr/src/linux-headers-$(uname -r)" ]] && [[ ! -d "/lib/modules/$(uname -r)/build" ]]; then
        print_error "Kernel headers not found. Install linux-headers package for your kernel."
        exit 1
    fi

    # Check for ALSA development files
    if ! pkg-config --exists alsa; then
        print_warning "ALSA development files not found. Some features may not work."
    fi

    print_success "System requirements check passed"
}

build_project() {
    print_step "Building project components..."

    # Create build directory
    mkdir -p "$BUILD_DIR"

    # Build kernel module
    print_step "Building kernel module..."
    if ! make -C kernel; then
        print_error "Kernel module build failed"
        exit 1
    fi

    # Build user-space components
    print_step "Building user-space components..."
    if ! make -C userspace; then
        print_error "User-space build failed"
        exit 1
    fi

    # Build development tools
    print_step "Building development tools..."
    if ! make -C tools; then
        print_error "Tools build failed"
        exit 1
    fi

    print_success "Build completed successfully"
}

install_files() {
    local destdir="${1:-}"

    print_step "Installing files to ${destdir:-system}..."

    # Install kernel module
    print_step "Installing kernel module..."
    make -C kernel DESTDIR="$destdir" install

    # Install user-space components
    print_step "Installing user-space components..."
    make -C userspace DESTDIR="$destdir" install

    # Install configuration
    print_step "Installing configuration files..."
    make DESTDIR="$destdir" install-config

    # Install documentation
    print_step "Installing documentation..."
    make DESTDIR="$destdir" install-docs

    # Install development tools
    print_step "Installing development tools..."
    make -C tools DESTDIR="$destdir" install

    print_success "Installation completed"
}

setup_udev() {
    print_step "Setting up udev rules..."

    if [[ ! -f "udev/99-apollo.rules" ]]; then
        print_error "udev rules file not found"
        return 1
    fi

    sudo cp udev/99-apollo.rules /usr/lib/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger

    print_success "udev rules installed and reloaded"
}

setup_service() {
    print_step "Setting up systemd service..."

    if [[ ! -f "userspace/apollo.service" ]]; then
        print_error "Service file not found"
        return 1
    fi

    sudo cp userspace/apollo.service /usr/lib/systemd/system/
    sudo systemctl daemon-reload

    print_success "Systemd service installed"
}

load_module() {
    print_step "Loading kernel module..."

    if ! sudo modprobe apollo 2>/dev/null; then
        print_warning "Failed to load kernel module. This may be normal if no device is connected."
        print_warning "Try: sudo modprobe apollo"
    else
        print_success "Kernel module loaded"
    fi
}

test_installation() {
    print_step "Testing installation..."

    # Test tools
    if [[ -x "tools/apollo_detect" ]]; then
        print_step "Testing device detection..."
        ./tools/apollo_detect >/dev/null 2>&1 || print_warning "Device detection test failed"
    fi

    # Test CLI tool
    if [[ -x "userspace/apolloctl" ]]; then
        print_step "Testing CLI tool..."
        ./userspace/apolloctl --help >/dev/null 2>&1 || print_warning "CLI tool test failed"
    fi

    print_success "Basic tests completed"
}

show_post_install() {
    echo
    echo -e "${BLUE}Installation Complete!${NC}"
    echo
    echo "Next steps:"
    echo "1. Connect your Apollo Twin device"
    echo "2. Authorize the device: sudo thunderboltctl authorize <domain>:<port>"
    echo "3. Start the service: sudo systemctl start apollo"
    echo "4. Enable auto-start: sudo systemctl enable apollo"
    echo "5. Test audio: aplay -l | grep Apollo"
    echo
    echo "Useful commands:"
    echo "  Device status: apollo_detect"
    echo "  Control device: apolloctl status"
    echo "  View logs: journalctl -u apollo"
    echo
    echo "Documentation: /usr/share/doc/apollo-driver/"
    echo "Source code: $SCRIPT_DIR"
    echo
}

usage() {
    echo "Apollo Driver Installation Script"
    echo
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  -h, --help          Show this help"
    echo "  -p, --prefix DIR    Installation prefix (default: /usr/local)"
    echo "  -d, --destdir DIR   Destination directory for packaging"
    echo "  --no-service        Skip systemd service setup"
    echo "  --no-udev          Skip udev rules setup"
    echo "  --no-test          Skip post-installation tests"
    echo
    echo "Examples:"
    echo "  $0                    # Full installation"
    echo "  $0 --destdir /tmp/pkg # Package build"
    echo "  sudo $0               # System installation (recommended)"
}

main() {
    local destdir=""
    local setup_service_flag=true
    local setup_udev_flag=true
    local test_flag=true

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -p|--prefix)
                INSTALL_PREFIX="$2"
                shift 2
                ;;
            -d|--destdir)
                destdir="$2"
                shift 2
                ;;
            --no-service)
                setup_service_flag=false
                shift
                ;;
            --no-udev)
                setup_udev_flag=false
                shift
                ;;
            --no-test)
                test_flag=false
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    print_header

    check_requirements
    build_project

    if [[ -n "$destdir" ]]; then
        # Package build mode
        install_files "$destdir"
    else
        # System installation mode
        if [[ $EUID -ne 0 ]]; then
            print_error "System installation requires root privileges"
            print_error "Run: sudo $0"
            exit 1
        fi

        install_files
        [[ "$setup_udev_flag" == true ]] && setup_udev
        [[ "$setup_service_flag" == true ]] && setup_service
        load_module
        [[ "$test_flag" == true ]] && test_installation
    fi

    show_post_install
}

# Run main function
main "$@"
