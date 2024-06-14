#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <nvml.h>
#include <ncurses.h>

#define MAX_GPUS 8
#define MAX_ITEMS 100

void handleError(nvmlReturn_t result) {
    if (NVML_SUCCESS != result) {
        endwin();
        printf("Error: %s\n", nvmlErrorString(result));
        nvmlShutdown();
        exit(1);
    }
}

void displayDeviceInfo(nvmlDevice_t device, int id, int index) { //, int cap, int tot_mem) {
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlMemory_t memory;
    nvmlUtilization_t utilization;
    unsigned int pwr;

    handleError(nvmlDeviceGetName(device, name, sizeof(name)));
    handleError(nvmlDeviceGetMemoryInfo(device, &memory));
    handleError(nvmlDeviceGetUtilizationRates(device, &utilization));
    handleError(nvmlDeviceGetPowerUsage(device, &pwr));

    mvprintw(index, 0, "G%d | %3u W | %5d MiB | %3u%%\n",
           id,
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

int compare_ints(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

void parse_gpu_ids(char* optarg, int* gpus, unsigned int* gpu_count) {
    char *token;
    int temp_gpus[MAX_ITEMS];
    unsigned temp_count = 0;

    // seperate by comma
    token = strtok(optarg, ",");
    while (token != NULL && temp_count < MAX_ITEMS) {
        temp_gpus[temp_count++] = atoi(token);
        token = strtok(NULL, ",");
    }

    // remove duplicates
    *gpu_count = 0;
    unsigned int i,j;
    for (i = 0; i < temp_count; i++) {
        for (j = 0; j < *gpu_count; j++) {
            if (gpus[j] == temp_gpus[i]) 
                break;
        }
        if (j == *gpu_count) {
            gpus[(*gpu_count)++] = temp_gpus[i];
        }
    }

    qsort(gpus, *gpu_count, sizeof(int), compare_ints);
}

int main(int argc, char *argv[]) {
    unsigned int device_count, i;
    nvmlDevice_t device[MAX_GPUS];
    int opt;
    int gpu_ids[MAX_GPUS];
    unsigned selected_device_count;
    int interval = 1;
    bool selected = false;

    handleError(nvmlInit());
    handleError(nvmlDeviceGetCount(&device_count));
    selected_device_count = device_count;

    while ((opt = getopt(argc, argv, "i:n:h")) != -1) {
        switch (opt) {
            case 'i':
                parse_gpu_ids(optarg, gpu_ids, &selected_device_count);
                selected = true;
                break;
            case 'n':
                interval = atoi(optarg);
                break;
            case 'h':
            default:
                fprintf(stderr, "Usage: %s [options] command\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -i Target GPU IDs\n");
                fprintf(stderr, "  -n seconds to wait between updates\n");
                fprintf(stderr, "  -h show this help\n");
                exit(EXIT_FAILURE);
        }
    }

    if (!selected) {
        for(i = 0; i < selected_device_count; i++) {
            gpu_ids[i] = i;
        }
    }

    for(i = 0; i < selected_device_count; i++) {
        if (gpu_ids[i] < 0 && gpu_ids[i] >= (int)device_count) {
            fprintf(stderr, "Error: No GPUs were found. ID %d out of range \
                                    (0-%d)\n", gpu_ids[i], device_count - 1);
            nvmlShutdown();
            exit(EXIT_FAILURE);
        }
    }

    // get device handles
    for(i = 0; i < selected_device_count; i++) {
        handleError(nvmlDeviceGetHandleByIndex(gpu_ids[i], &device[i]));
    }

    initscr();
    nodelay(stdscr, TRUE);
    noecho();

    while (1) {
        clearScreen();

        // print device info line by line
        for(i = 0; i < selected_device_count; i++) {
            displayDeviceInfo(device[i], gpu_ids[i], i);
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
