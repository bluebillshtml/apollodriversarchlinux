// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Device Detection Tool
 *
 * Scans the system for Apollo Twin devices and reports their status.
 * Useful for debugging Thunderbolt connections and device enumeration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define THUNDERBOLT_PATH "/sys/bus/thunderbolt/devices"
#define PCI_PATH "/sys/bus/pci/devices"
#define APOLLO_VENDOR_ID "1176"

static void print_usage(const char *program_name) {
    printf("Apollo Device Detection Tool\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose    Show detailed device information\n");
    printf("  -t, --thunderbolt Scan Thunderbolt devices only\n");
    printf("  -p, --pci        Scan PCI devices only\n");
    printf("  -h, --help       Show this help\n");
}

static int is_apollo_device(const char *device_path) {
    char vendor_path[512];
    char vendor[16];

    snprintf(vendor_path, sizeof(vendor_path), "%s/vendor", device_path);

    FILE *fp = fopen(vendor_path, "r");
    if (!fp) return 0;

    if (fgets(vendor, sizeof(vendor), fp)) {
        // Remove newline
        vendor[strcspn(vendor, "\n")] = 0;
        // Check for Apollo vendor ID (SIS used by UA)
        if (strcmp(vendor, "0x" APOLLO_VENDOR_ID) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static int is_thunderbolt_apollo(const char *device_path) {
    char name_path[512];
    char name[256];

    snprintf(name_path, sizeof(name_path), "%s/device_name", device_path);

    FILE *fp = fopen(name_path, "r");
    if (!fp) return 0;

    if (fgets(name, sizeof(name), fp)) {
        name[strcspn(name, "\n")] = 0;
        if (strstr(name, "Apollo") != NULL) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static void print_device_info(const char *device_path, int verbose) {
    char info_path[512];
    char info[256];

    printf("Device: %s\n", device_path);

    if (verbose) {
        // Try to read various device attributes
        const char *attrs[] = {
            "vendor", "device", "subsystem_vendor", "subsystem_device",
            "class", "device_name", "authorized", "unique_id", NULL
        };

        for (int i = 0; attrs[i]; i++) {
            snprintf(info_path, sizeof(info_path), "%s/%s", device_path, attrs[i]);
            FILE *fp = fopen(info_path, "r");
            if (fp) {
                if (fgets(info, sizeof(info), fp)) {
                    info[strcspn(info, "\n")] = 0;
                    printf("  %s: %s\n", attrs[i], info);
                }
                fclose(fp);
            }
        }
    }
    printf("\n");
}

static int scan_devices(const char *bus_path, int (*device_check)(const char *),
                       int verbose, const char *bus_name) {
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir(bus_path);
    if (!dir) {
        fprintf(stderr, "Failed to open %s: %s\n", bus_path, strerror(errno));
        return 0;
    }

    printf("Scanning %s devices...\n", bus_name);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char device_path[512];
        snprintf(device_path, sizeof(device_path), "%s/%s", bus_path, entry->d_name);

        if (device_check(device_path)) {
            print_device_info(device_path, verbose);
            found++;
        }
    }

    closedir(dir);

    if (found == 0) {
        printf("No Apollo devices found on %s bus.\n\n", bus_name);
    } else {
        printf("Found %d Apollo device(s) on %s bus.\n\n", found, bus_name);
    }

    return found;
}

static void check_kernel_module(void) {
    printf("Checking kernel module status...\n");

    // Check if module is loaded
    FILE *fp = popen("lsmod | grep apollo", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            printf("✓ Apollo kernel module is loaded\n");
        } else {
            printf("✗ Apollo kernel module is not loaded\n");
            printf("  Run 'sudo modprobe apollo' to load it\n");
        }
        pclose(fp);
    }

    // Check if ALSA device is available
    fp = popen("aplay -l | grep -i apollo", "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            printf("✓ Apollo ALSA device is available\n");
        } else {
            printf("✗ Apollo ALSA device not found\n");
            printf("  Check Thunderbolt connection and device authorization\n");
        }
        pclose(fp);
    }

    printf("\n");
}

static void check_thunderbolt_daemon(void) {
    printf("Checking Thunderbolt daemon status...\n");

    FILE *fp = popen("systemctl is-active bolt", "r");
    if (fp) {
        char status[32];
        if (fgets(status, sizeof(status), fp)) {
            status[strcspn(status, "\n")] = 0;
            if (strcmp(status, "active") == 0) {
                printf("✓ Thunderbolt daemon (bolt) is running\n");
            } else {
                printf("✗ Thunderbolt daemon (bolt) is not running\n");
                printf("  Run 'sudo systemctl start bolt' to start it\n");
            }
        }
        pclose(fp);
    }

    printf("\n");
}

static int activate_apollo_device(void) {
    FILE *fp;
    char path[512];

    /* Try to set boot attribute to 1 */
    snprintf(path, sizeof(path), "/sys/bus/thunderbolt/devices/0-1/boot");
    fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "1");
        fclose(fp);
        printf("  Set boot attribute to 1\n");
    }

    /* Try to set various control attributes */
    const char *control_files[] = {
        "/sys/bus/thunderbolt/devices/0-1/wakeup",
        "/sys/bus/thunderbolt/devices/domain0/iommu_dma_protection",
        NULL
    };

    for (int i = 0; control_files[i]; i++) {
        fp = fopen(control_files[i], "w");
        if (fp) {
            fprintf(fp, "1");
            fclose(fp);
            printf("  Enabled %s\n", control_files[i]);
        }
    }

    /* Wait a moment for changes to take effect */
    sleep(1);

    /* Check if PCI devices appeared */
    FILE *lspci = popen("lspci -n | grep 1176", "r");
    if (lspci) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), lspci)) {
            pclose(lspci);
            return 1; /* Success - PCI device found */
        }
        pclose(lspci);
    }

    return 0; /* No PCI device found */
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    int thunderbolt_only = 0;
    int pci_only = 0;
    int opt;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "vtph")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 't':
                thunderbolt_only = 1;
                break;
            case 'p':
                pci_only = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_SUCCESS;
        }
    }

    printf("Apollo Twin Device Detection Tool\n");
    printf("==================================\n\n");

    int total_found = 0;

    // Check system status
    check_kernel_module();
    check_thunderbolt_daemon();

    // Scan devices
    if (!pci_only) {
        total_found += scan_devices(THUNDERBOLT_PATH, is_thunderbolt_apollo,
                                  verbose, "Thunderbolt");
    }

    if (!thunderbolt_only) {
        total_found += scan_devices(PCI_PATH, is_apollo_device,
                                  verbose, "PCI");
    }

    if (total_found == 0) {
        printf("No Apollo devices detected.\n");
        printf("\nTroubleshooting steps:\n");
        printf("1. Ensure the device is connected via Thunderbolt\n");
        printf("2. Authorize the device: sudo thunderboltctl authorize <domain>:<port>\n");
        printf("3. Load the kernel module: sudo modprobe apollo\n");
        printf("4. Check kernel logs: dmesg | grep -i apollo\n");
    } else {
    printf("Apollo device(s) detected successfully!\n");

    /* Try to activate the device */
    printf("Attempting device activation...\n");
    if (activate_apollo_device()) {
        printf("✓ Device activation successful!\n");
        printf("Check for new PCI devices: lspci | grep 1176\n");
    } else {
        printf("✗ Device activation failed\n");
    }

    printf("You can now use the device with ALSA/PipeWire applications.\n");
    }

    return EXIT_SUCCESS;
}
