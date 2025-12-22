# Apollo Twin Linux Driver - Top Level Makefile
# SPDX-License-Identifier: GPL-2.0-only

.PHONY: all kernel userspace tools install uninstall clean distclean help

# Default target
all: kernel userspace tools

# Build kernel module
kernel:
	@echo "Building kernel module..."
	$(MAKE) -C kernel

# Build user-space components
userspace:
	@echo "Building user-space components..."
	$(MAKE) -C userspace

# Build development tools
tools:
	@echo "Building development tools..."
	$(MAKE) -C tools

# Install everything
install: install-kernel install-userspace install-config install-docs
	@echo "Installation complete. Run 'sudo modprobe apollo' to load the driver."

# Install kernel module
install-kernel: kernel
	@echo "Installing kernel module..."
	$(MAKE) -C kernel install

# Install user-space components
install-userspace: userspace
	@echo "Installing user-space components..."
	$(MAKE) -C userspace install

# Install configuration files
install-config:
	@echo "Installing configuration files..."
	install -d $(DESTDIR)/etc/apollo
	install -m 644 config/apollo.conf $(DESTDIR)/etc/apollo/

# Install documentation
install-docs:
	@echo "Installing documentation..."
	install -d $(DESTDIR)/usr/share/doc/apollo-driver
	install -m 644 README.md OVERVIEW.md PROJECT_SUMMARY.md $(DESTDIR)/usr/share/doc/apollo-driver/
	install -m 644 docs/*.md $(DESTDIR)/usr/share/doc/apollo-driver/

# Uninstall everything
uninstall: uninstall-kernel uninstall-userspace uninstall-config uninstall-docs
	@echo "Uninstallation complete."

# Uninstall kernel module
uninstall-kernel:
	@echo "Uninstalling kernel module..."
	$(MAKE) -C kernel uninstall

# Uninstall user-space components
uninstall-userspace:
	@echo "Uninstalling user-space components..."
	$(MAKE) -C userspace uninstall

# Uninstall configuration
uninstall-config:
	@echo "Uninstalling configuration files..."
	rm -rf $(DESTDIR)/etc/apollo

# Uninstall documentation
uninstall-docs:
	@echo "Uninstalling documentation..."
	rm -rf $(DESTDIR)/usr/share/doc/apollo-driver

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	$(MAKE) -C kernel clean
	$(MAKE) -C userspace clean
	$(MAKE) -C tools clean

# Clean everything including generated files
distclean: clean
	@echo "Cleaning all generated files..."
	rm -f *.tar.gz
	rm -rf build/

# Package for distribution
dist: distclean
	@echo "Creating distribution package..."
	mkdir -p build/
	git archive --format=tar.gz --prefix=apollo-driver-$(shell git describe --tags --abbrev=0 2>/dev/null || echo "0.1.0")/ -o build/apollo-driver-$(shell git describe --tags --abbrev=0 2>/dev/null || echo "0.1.0").tar.gz HEAD

# Development targets
check: all
	@echo "Running basic checks..."
	$(MAKE) -C kernel check
	$(MAKE) -C userspace check

# Test targets (require device)
test: all
	@echo "Running tests (requires Apollo Twin device)..."
	@echo "Note: Tests require root privileges and connected device"
	@echo "Run 'sudo make test' to execute"

test-kernel: kernel
	@echo "Testing kernel module..."
	sudo dmesg -C  # Clear kernel log
	sudo insmod kernel/apollo.ko
	sleep 2
	lsmod | grep -q apollo && echo "✓ Module loaded successfully" || echo "✗ Module failed to load"
	sudo rmmod apollo
	dmesg | grep -i apollo | tail -10

test-audio: install
	@echo "Testing audio functionality..."
	@echo "Note: Requires Apollo Twin device to be connected and authorized"
	@echo "Check if device appears in ALSA:"
	aplay -l | grep -i apollo || echo "Device not found - check Thunderbolt connection"

# Help target
help:
	@echo "Apollo Twin Linux Driver Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build everything (default)"
	@echo "  kernel       - Build kernel module only"
	@echo "  userspace    - Build user-space components only"
	@echo "  tools        - Build development tools only"
	@echo "  install      - Install everything to system"
	@echo "  uninstall    - Remove everything from system"
	@echo "  clean        - Remove build artifacts"
	@echo "  distclean    - Remove all generated files"
	@echo "  dist         - Create distribution package"
	@echo "  check        - Run basic build checks"
	@echo "  test         - Run basic functionality tests"
	@echo "  test-kernel  - Test kernel module loading"
	@echo "  test-audio   - Test audio functionality"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Installation requires root privileges."
	@echo "See docs/INSTALL.md for detailed setup instructions."
