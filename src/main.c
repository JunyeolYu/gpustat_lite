#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <nvml.h>
#include <ncurses.h>

void handleError(nvmlReturn_t result) {
    if (NVML_SUCCESS != result) {
        endwin();
        printf("Error: %s\n", nvmlErrorString(result));
        nvmlShutdown();
        exit(1);
    }
}

void displayDeviceInfo(nvmlDevice_t device, int index) { //, int cap, int tot_mem) {
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlMemory_t memory;
    nvmlUtilization_t utilization;
    unsigned int pwr;

    handleError(nvmlDeviceGetName(device, name, sizeof(name)));
    handleError(nvmlDeviceGetMemoryInfo(device, &memory));
    handleError(nvmlDeviceGetUtilizationRates(device, &utilization));
    handleError(nvmlDeviceGetPowerUsage(device, &pwr));

    mvprintw(index, 0, "G%d | %3u W | %5d MiB | %3u%%\n",
           index,
           pwr/1000,
           //cap,
           (int) (memory.used / 1024 /1024),
           //tot_mem,
           utilization.gpu
           );
}

void clearScreen() {
    erase();
}

int main(int argc, char *argv[]) {
    unsigned int device_count, i;
    nvmlDevice_t device;
    int opt;
    int gpu_id = -1;
    int interval = 1;

    while ((opt = getopt(argc, argv, "i:n:")) != -1) {
        switch (opt) {
            case 'i':
                gpu_id = atoi(optarg);
                break;
            case 'n':
                interval = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-i gpu_id] [-n interval]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    handleError(nvmlInit());
    handleError(nvmlDeviceGetCount(&device_count));

    if (gpu_id != -1) {
        if (gpu_id >= (int)device_count) {
            fprintf(stderr, "Error: GPU ID %d out of range (0-%d)\n", gpu_id, device_count - 1);
            nvmlShutdown();
            exit(EXIT_FAILURE);
        }
    }

    initscr();
    nodelay(stdscr, TRUE);
    noecho();

    while (1) {
        clearScreen();

        if (gpu_id == -1) {
            for (i = 0; i < device_count; i++) {
                handleError(nvmlDeviceGetHandleByIndex(i, &device));
                displayDeviceInfo(device, i);
            }
        } else {
            handleError(nvmlDeviceGetHandleByIndex(gpu_id, &device));
            displayDeviceInfo(device, gpu_id);
        }

        refresh();
        sleep(interval);

        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;
    }

    endwin();
    handleError(nvmlShutdown());

    return 0;
}
