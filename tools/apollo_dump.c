// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Register Dump Tool
 *
 * Dumps device registers and memory for reverse engineering.
 * WARNING: This tool requires root privileges and direct hardware access.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

#define MAX_DUMP_SIZE (1024 * 1024)  // 1MB max dump

static void print_usage(const char *program_name) {
    printf("Apollo Register Dump Tool\n");
    printf("Usage: %s [options] <device> <offset> [size]\n\n", program_name);
    printf("Arguments:\n");
    printf("  device    PCI device (e.g., 0000:01:00.0) or resource file\n");
    printf("  offset    Register offset in hex (e.g., 0x00)\n");
    printf("  size      Number of bytes to dump (default: 256)\n\n");
    printf("Options:\n");
    printf("  -r, --resource  Dump from PCI resource file (default)\n");
    printf("  -m, --mem       Dump from /dev/mem (requires root)\n");
    printf("  -b, --binary    Binary output (default: hex)\n");
    printf("  -w, --word      32-bit word format\n");
    printf("  -d, --dword     64-bit word format\n");
    printf("  -h, --help      Show this help\n\n");
    printf("Examples:\n");
    printf("  %s /sys/bus/pci/devices/0000:01:00.0/resource0 0x00 256\n", program_name);
    printf("  %s -m 0xfebf1000 0x00 1024\n", program_name);
    printf("  %s -w /dev/apollo 0x10\n\n", program_name);
    printf("WARNING: Direct hardware access can be dangerous!\n");
}

static int find_pci_resource(const char *device, char *resource_path, size_t path_size) {
    char config_path[512];
    FILE *fp;
    uint16_t vendor_id, device_id;

    // Check if it's already a resource path
    if (strstr(device, "/resource")) {
        strncpy(resource_path, device, path_size);
        return 0;
    }

    // Try to find the PCI device and get its BAR0
    snprintf(config_path, sizeof(config_path), "/sys/bus/pci/devices/%s/config", device);

    fp = fopen(config_path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open PCI config: %s\n", config_path);
        return -1;
    }

    // Read vendor and device ID to verify it's an Apollo device
    if (fread(&vendor_id, 2, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (fread(&device_id, 2, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    // Check for Apollo vendor ID (SIS)
    if (vendor_id != 0x13f4) {
        fprintf(stderr, "Warning: Device vendor ID 0x%04x doesn't match Apollo (0x13f4)\n", vendor_id);
    }

    // Construct resource0 path
    snprintf(resource_path, path_size, "/sys/bus/pci/devices/%s/resource0", device);

    return 0;
}

static void *map_device_region(const char *path, size_t size, int *fd) {
    struct stat st;
    void *map;

    *fd = open(path, O_RDONLY);
    if (*fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fstat(*fd, &st) < 0) {
        fprintf(stderr, "Failed to stat %s: %s\n", path, strerror(errno));
        close(*fd);
        return NULL;
    }

    if (size > st.st_size) {
        size = st.st_size;
    }

    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, *fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap %s: %s\n", path, strerror(errno));
        close(*fd);
        return NULL;
    }

    return map;
}

static void dump_hex(const uint8_t *data, size_t size, uintptr_t base_addr) {
    size_t i, j;

    for (i = 0; i < size; i += 16) {
        printf("%08lx: ", base_addr + i);

        // Hex bytes
        for (j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
        }

        printf(" ");

        // ASCII representation
        for (j = 0; j < 16 && i + j < size; j++) {
            uint8_t c = data[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }

        printf("\n");
    }
}

static void dump_words(const uint32_t *data, size_t size, uintptr_t base_addr) {
    size_t i;

    for (i = 0; i < size / 4; i += 4) {
        printf("%08lx: %08x %08x %08x %08x\n",
               base_addr + i * 4,
               data[i], data[i+1], data[i+2], data[i+3]);
    }
}

static void dump_dwords(const uint64_t *data, size_t size, uintptr_t base_addr) {
    size_t i;

    for (i = 0; i < size / 8; i += 2) {
        printf("%08lx: %016lx %016lx\n",
               base_addr + i * 8,
               data[i], data[i+1]);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    int use_mem = 0;
    int binary_output = 0;
    int word_format = 0;
    int dword_format = 0;
    char *device;
    uintptr_t offset = 0;
    size_t size = 256;
    char resource_path[512];
    void *map;
    int fd;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "rmbwdh")) != -1) {
        switch (opt) {
            case 'r':
                use_mem = 0;
                break;
            case 'm':
                use_mem = 1;
                break;
            case 'b':
                binary_output = 1;
                break;
            case 'w':
                word_format = 1;
                break;
            case 'd':
                dword_format = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_SUCCESS;
        }
    }

    if (argc - optind < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    device = argv[optind];
    offset = strtoul(argv[optind + 1], NULL, 0);

    if (argc - optind >= 3) {
        size = strtoul(argv[optind + 2], NULL, 0);
    }

    if (size > MAX_DUMP_SIZE) {
        fprintf(stderr, "Dump size too large (max %d bytes)\n", MAX_DUMP_SIZE);
        return EXIT_FAILURE;
    }

    printf("Apollo Register Dump Tool\n");
    printf("=========================\n");
    printf("Device: %s\n", device);
    printf("Offset: 0x%lx\n", offset);
    printf("Size: %zu bytes\n\n", size);

    if (use_mem) {
        // Direct /dev/mem access
        uintptr_t phys_addr = offset;  // Assume offset is physical address

        if (getuid() != 0) {
            fprintf(stderr, "Root privileges required for /dev/mem access\n");
            return EXIT_FAILURE;
        }

        map = map_device_region("/dev/mem", size, &fd);
        if (!map) {
            return EXIT_FAILURE;
        }

        // Adjust pointer to offset
        uint8_t *data = (uint8_t *)map + (phys_addr & 0xFFF);  // Page offset

        printf("Dumping from physical address 0x%lx:\n", phys_addr);
    } else {
        // PCI resource file access
        if (find_pci_resource(device, resource_path, sizeof(resource_path)) < 0) {
            return EXIT_FAILURE;
        }

        map = map_device_region(resource_path, size, &fd);
        if (!map) {
            return EXIT_FAILURE;
        }

        uint8_t *data = (uint8_t *)map + offset;

        printf("Dumping from %s + 0x%lx:\n", resource_path, offset);
    }

    // Dump the data
    if (binary_output) {
        fwrite((uint8_t *)map + offset, 1, size, stdout);
    } else if (word_format && (size % 4 == 0)) {
        dump_words((uint32_t *)((uint8_t *)map + offset), size, offset);
    } else if (dword_format && (size % 8 == 0)) {
        dump_dwords((uint64_t *)((uint8_t *)map + offset), size, offset);
    } else {
        dump_hex((uint8_t *)map + offset, size, offset);
    }

    munmap(map, size);
    close(fd);

    return EXIT_SUCCESS;
}
