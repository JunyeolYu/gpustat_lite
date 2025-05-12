#include <ncurses.h>
#include <nvml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_GPUS 8
#define MAX_ITEMS 100
#define NVML_DEVICE_NAME_BUFFER_SIZE 64

void normalize_name(char *name) {
  const char *remove_words[] = {"NVIDIA", "GeForce", "Graphics", NULL};
  char temp[NVML_DEVICE_NAME_BUFFER_SIZE];
  strcpy(temp, name);

  for (int i = 0; remove_words[i] != NULL; i++) {
    char *pos;
    while ((pos = strstr(temp, remove_words[i])) != NULL) {
      memmove(pos, pos + strlen(remove_words[i]),
              strlen(pos + strlen(remove_words[i])) + 1);
    }
  }

  // Remove leading/trailing spaces
  char *start = temp;
  while (*start == ' ')
    start++;

  char *end = start + strlen(start) - 1;
  while (end > start && *end == ' ')
    *end-- = '\0';

  strncpy(name, start, NVML_DEVICE_NAME_BUFFER_SIZE - 1);
  name[NVML_DEVICE_NAME_BUFFER_SIZE - 1] = '\0';
}

void handleError(nvmlReturn_t result) {
  if (NVML_SUCCESS != result) {
    endwin();
    printf("Error: %s\n", nvmlErrorString(result));
    nvmlShutdown();
    exit(1);
  }
}

void displayDeviceInfo(nvmlDevice_t device, int id,
                       int index) { //, int cap, int tot_mem) {
  nvmlMemory_t memory;
  nvmlUtilization_t utilization;
  unsigned int pwr;

  handleError(nvmlDeviceGetMemoryInfo(device, &memory));
  handleError(nvmlDeviceGetUtilizationRates(device, &utilization));
  handleError(nvmlDeviceGetPowerUsage(device, &pwr));

  mvprintw(index, 0, "G%d | %3u W | %5d MiB | %3u%%\n", id, pwr / 1000,
           // cap,
           (int)(memory.used / 1024 / 1024),
           // tot_mem,
           utilization.gpu);
}

void displayDeviceInfo_name(nvmlDevice_t device, int index, char *name,
                            int name_width) { //, int cap, int tot_mem) {
  nvmlMemory_t memory;
  nvmlUtilization_t utilization;
  unsigned int pwr;

  handleError(nvmlDeviceGetMemoryInfo(device, &memory));
  handleError(nvmlDeviceGetUtilizationRates(device, &utilization));
  handleError(nvmlDeviceGetPowerUsage(device, &pwr));

  char format[64];
  snprintf(format, sizeof(format), "%%-%ds | %%3u W | %%5d MiB | %%3u%%%%\n",
           name_width);
  mvprintw(index, 0, format, name, pwr / 1000,
           // cap,
           (int)(memory.used / 1024 / 1024),
           // tot_mem,
           utilization.gpu);
}

void displayDeviceInfo_horizontal(int n, nvmlDevice_t *device, int *id) {
  nvmlMemory_t memory;
  nvmlUtilization_t utilization;
  unsigned int pwr;

  int i = 0;
  for (i = 0; i < n; i++) {
    handleError(nvmlDeviceGetMemoryInfo(device[i], &memory));
    handleError(nvmlDeviceGetUtilizationRates(device[i], &utilization));
    handleError(nvmlDeviceGetPowerUsage(device[i], &pwr));

    printw("G%d, %u W, %d MiB, %u%%", id[i], pwr / 1000,
           (int)(memory.used / 1024 / 1024), utilization.gpu);
    if (i != n - 1)
      printw(" | ");
  }
}

void clearScreen() { erase(); }

int compare_ints(const void *a, const void *b) {
  return (*(int *)a - *(int *)b);
}

void parse_gpu_ids(char *optarg, int *gpus, unsigned int *gpu_count) {
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
  unsigned int i, j;
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
  bool is_vertical = true;
  char **names = NULL;

  handleError(nvmlInit());
  handleError(nvmlDeviceGetCount(&device_count));
  selected_device_count = device_count;

  while ((opt = getopt(argc, argv, "i:n:hl")) != -1) {
    switch (opt) {
    case 'i':
      parse_gpu_ids(optarg, gpu_ids, &selected_device_count);
      selected = true;
      break;
    case 'n':
      interval = atoi(optarg);
      break;
    case 'l':
      is_vertical = false;
      break;
    case 'h':
    default:
      fprintf(stderr, "Usage: %s [options] command\n", argv[0]);
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "  -i Target GPU IDs\n");
      fprintf(stderr, "  -n seconds to wait between updates\n");
      fprintf(stderr, "  -l Display GPU status in a single horizontal line\n");
      fprintf(stderr, "  -h Show this help\n");
      exit(EXIT_FAILURE);
    }
  }

  if (!selected) {
    for (i = 0; i < selected_device_count; i++) {
      gpu_ids[i] = i;
    }
  }

  for (i = 0; i < selected_device_count; i++) {
    if (gpu_ids[i] < 0 && gpu_ids[i] >= (int)device_count) {
      fprintf(stderr, "Error: No GPUs were found. ID %d out of range \
                                    (0-%d)\n",
              gpu_ids[i], device_count - 1);
      nvmlShutdown();
      exit(EXIT_FAILURE);
    }
  }

  // get device handles
  names = (char **)malloc(sizeof(char *) * selected_device_count);
  int max_name_len = 0;
  for (i = 0; i < selected_device_count; i++) {
    handleError(nvmlDeviceGetHandleByIndex(gpu_ids[i], &device[i]));
    names[i] = (char *)malloc(NVML_DEVICE_NAME_BUFFER_SIZE * sizeof(char));
    handleError(
        nvmlDeviceGetName(device[i], names[i], NVML_DEVICE_NAME_BUFFER_SIZE));
    // remove tags
    normalize_name(names[i]);
    int len = strlen(names[i]);
    if (len > max_name_len)
      max_name_len = len;
  }

  initscr();
  nodelay(stdscr, TRUE);
  noecho();

  while (1) {
    clearScreen();

    if (is_vertical) {
      // print device info line by line
      for (i = 0; i < selected_device_count; i++) {
        displayDeviceInfo_name(device[i], i, names[i], max_name_len);
      }
    } else {
      displayDeviceInfo_horizontal(selected_device_count, device, gpu_ids);
    }

    refresh();
    sleep(interval);

    int ch = getch();
    if (ch == 'q' || ch == 'Q')
      break;
  }

  endwin();
  handleError(nvmlShutdown());

  return 0;
}
