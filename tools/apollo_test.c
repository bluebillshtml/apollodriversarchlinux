// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Driver Test Suite
 *
 * Runs basic functionality tests for the Apollo driver components.
 * Useful for development and CI/CD integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define TEST_PASSED 0
#define TEST_FAILED 1
#define TEST_SKIPPED 2

typedef struct {
    const char *name;
    const char *description;
    int (*test_func)(void);
    int requires_device;
} test_case_t;

static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;
static int skip_count = 0;

#define TEST_CASE(name, desc, func, req_dev) \
    { name, desc, func, req_dev }

// Forward declarations of test functions
static int test_build_system(void);
static int test_kernel_module_compilation(void);
static int test_userspace_compilation(void);
static int test_tools_compilation(void);
static int test_configuration_files(void);
static int test_device_detection(void);
static int test_kernel_module_loading(void);
static int test_alsa_device(void);
static int test_control_daemon(void);
static int test_audio_loopback(void);

static const test_case_t test_cases[] = {
    TEST_CASE("build_system", "Test build system integrity", test_build_system, 0),
    TEST_CASE("kernel_compilation", "Test kernel module compilation", test_kernel_module_compilation, 0),
    TEST_CASE("userspace_compilation", "Test user-space compilation", test_userspace_compilation, 0),
    TEST_CASE("tools_compilation", "Test tools compilation", test_tools_compilation, 0),
    TEST_CASE("config_files", "Test configuration file validity", test_configuration_files, 0),
    TEST_CASE("device_detection", "Test device detection (requires device)", test_device_detection, 1),
    TEST_CASE("kernel_loading", "Test kernel module loading (requires device)", test_kernel_module_loading, 1),
    TEST_CASE("alsa_device", "Test ALSA device registration (requires device)", test_alsa_device, 1),
    TEST_CASE("control_daemon", "Test control daemon functionality (requires device)", test_control_daemon, 1),
    TEST_CASE("audio_loopback", "Test audio loopback (requires device)", test_audio_loopback, 1),
};

static void print_test_header(void) {
    printf("Apollo Driver Test Suite\n");
    printf("========================\n\n");
}

static void print_test_result(const char *name, const char *description, int result) {
    const char *status;

    switch (result) {
        case TEST_PASSED:
            status = "PASS";
            pass_count++;
            break;
        case TEST_FAILED:
            status = "FAIL";
            fail_count++;
            break;
        case TEST_SKIPPED:
            status = "SKIP";
            skip_count++;
            break;
        default:
            status = "UNKNOWN";
            break;
    }

    printf("[%s] %s - %s\n", status, name, description);
    test_count++;
}

static void print_test_summary(void) {
    printf("\nTest Summary\n");
    printf("============\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    printf("Skipped: %d\n", skip_count);

    if (fail_count > 0) {
        printf("\nSome tests failed. Check the output above for details.\n");
        exit(EXIT_FAILURE);
    } else {
        printf("\nAll tests passed!\n");
        exit(EXIT_SUCCESS);
    }
}

static int run_command(const char *cmd, char *output, size_t output_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    if (output && output_size > 0) {
        size_t len = fread(output, 1, output_size - 1, fp);
        if (len > 0) {
            output[len] = '\0';
        }
    }

    int status = pclose(fp);
    return WEXITSTATUS(status);
}

static int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

static int test_build_system(void) {
    // Check for required build files
    if (!file_exists("Makefile")) return TEST_FAILED;
    if (!file_exists("kernel/Makefile")) return TEST_FAILED;
    if (!file_exists("userspace/Makefile")) return TEST_FAILED;
    if (!file_exists("tools/Makefile")) return TEST_FAILED;

    // Check for source files
    if (!file_exists("kernel/apollo_main.c")) return TEST_FAILED;
    if (!file_exists("userspace/apollod.c")) return TEST_FAILED;

    return TEST_PASSED;
}

static int test_kernel_module_compilation(void) {
    int ret = run_command("make -C kernel clean && make -C kernel", NULL, 0);
    return (ret == 0 && file_exists("kernel/apollo.ko")) ? TEST_PASSED : TEST_FAILED;
}

static int test_userspace_compilation(void) {
    int ret = run_command("make -C userspace clean && make -C userspace", NULL, 0);
    return (ret == 0 && file_exists("userspace/apollod") && file_exists("userspace/apolloctl")) ?
           TEST_PASSED : TEST_FAILED;
}

static int test_tools_compilation(void) {
    int ret = run_command("make -C tools clean && make -C tools", NULL, 0);
    return (ret == 0 && file_exists("tools/apollo_detect") &&
            file_exists("tools/apollo_dump") && file_exists("tools/apollo_test")) ?
           TEST_PASSED : TEST_FAILED;
}

static int test_configuration_files(void) {
    // Check config file exists and is readable
    if (!file_exists("config/apollo.conf")) return TEST_FAILED;

    FILE *fp = fopen("config/apollo.conf", "r");
    if (!fp) return TEST_FAILED;

    char line[256];
    int valid_lines = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Check for basic key=value format
        if (strchr(line, '=') != NULL) {
            valid_lines++;
        }
    }
    fclose(fp);

    return (valid_lines > 0) ? TEST_PASSED : TEST_FAILED;
}

static int test_device_detection(void) {
    if (!file_exists("tools/apollo_detect")) return TEST_FAILED;

    char output[1024];
    int ret = run_command("./tools/apollo_detect", output, sizeof(output));

    // Device detection can return success even if no device is found
    // We just check that the tool runs without crashing
    return (ret == 0) ? TEST_PASSED : TEST_FAILED;
}

static int test_kernel_module_loading(void) {
    // This test requires root privileges and actual hardware
    if (getuid() != 0) {
        printf("  (skipping - requires root privileges)\n");
        return TEST_SKIPPED;
    }

    // Try to load the module
    int ret = run_command("insmod kernel/apollo.ko 2>/dev/null || true", NULL, 0);

    // Check if module is loaded
    ret = run_command("lsmod | grep -q apollo", NULL, 0);
    if (ret == 0) {
        // Clean up - unload the module
        run_command("rmmod apollo 2>/dev/null || true", NULL, 0);
        return TEST_PASSED;
    }

    return TEST_FAILED;
}

static int test_alsa_device(void) {
    // This requires the device to be connected and working
    int ret = run_command("aplay -l | grep -q Apollo", NULL, 0);
    return (ret == 0) ? TEST_PASSED : TEST_FAILED;
}

static int test_control_daemon(void) {
    // This requires the device and daemon to be working
    if (!file_exists("userspace/apollod")) return TEST_FAILED;

    // Start daemon in background
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execl("./userspace/apollod", "apollod", "-f", NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent process
        sleep(2);  // Give daemon time to start

        // Test CLI tool
        int ret = run_command("./userspace/apolloctl status", NULL, 0);

        // Kill daemon
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);

        return (ret == 0) ? TEST_PASSED : TEST_FAILED;
    }

    return TEST_FAILED;
}

static int test_audio_loopback(void) {
    // This requires working audio I/O
    // Start a short loopback test
    int ret = run_command("timeout 5s speaker-test -D hw:Apollo -c 2 -t sine -f 1000 -l 1 2>/dev/null", NULL, 0);
    return (ret == 0) ? TEST_PASSED : TEST_FAILED;
}

int main(int argc, char *argv[]) {
    int run_device_tests = 0;
    int i;

    // Check for --device flag to run hardware-dependent tests
    if (argc > 1 && strcmp(argv[1], "--device") == 0) {
        run_device_tests = 1;
    }

    print_test_header();

    for (i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        const test_case_t *test = &test_cases[i];

        // Skip device-dependent tests unless --device flag is used
        if (test->requires_device && !run_device_tests) {
            print_test_result(test->name, test->description, TEST_SKIPPED);
            continue;
        }

        printf("Running %s...\n", test->name);
        int result = test->test_func();
        print_test_result(test->name, test->description, result);
    }

    if (!run_device_tests) {
        printf("\nNote: Hardware-dependent tests were skipped.\n");
        printf("Run '%s --device' with Apollo device connected to run all tests.\n", argv[0]);
    }

    print_test_summary();

    return 0;
}
