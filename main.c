#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define MAX_HOSTS 100
#define MAX_LENGTH 256
#define MAX_BAR_WIDTH 50
#define DEFAULT_INTERVAL 3
#define LOG_FILENAME "pinggraph.log"

typedef struct
{
    int count;
    float sum;
    float min;
    float max;
    int timeout_count;
} latency_stat;

int load_hosts_from_file(const char *filename, char *hosts[], int max_hosts)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open host file: %s\n", filename);
        return 0;
    }

    int count = 0;
    char buffer[MAX_LENGTH];

    while (fgets(buffer, sizeof(buffer), file) != NULL && count < max_hosts) {
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) {
            continue;
        }
        hosts[count] = malloc(strlen(buffer) + 1);
        if (hosts[count] == NULL) {
            fclose(file);
            return 0;
        }
        strcpy(hosts[count], buffer);
        count++;
    }

    fclose(file);
    return count;
}

int read_hosts_interactive(char *hosts[], int max_hosts)
{
    int host_count = 0;
    char buffer[MAX_LENGTH];

    printf("PingGraph - Host Input\n");
    printf("Enter number of hosts: ");

    if (scanf("%d", &host_count) != 1 || host_count <= 0 || host_count > max_hosts) {
        printf("Invalid number of hosts.\n");
        return 0;
    }

    getchar();

    for (int i = 0; i < host_count; i++) {
        printf("Enter host %d: ", i + 1);
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("Error reading input.\n");
            return 0;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        hosts[i] = malloc(strlen(buffer) + 1);
        if (hosts[i] == NULL) {
            printf("Memory allocation failed.\n");
            return 0;
        }

        strcpy(hosts[i], buffer);
    }

    return host_count;
}

float get_ping_latency(const char *host)
{
    char command[MAX_LENGTH + 32];

#ifdef _WIN32
    snprintf(command, sizeof(command), "ping -n 1 %s", host);
    FILE *fp = _popen(command, "r");
#else
    snprintf(command, sizeof(command), "ping -c 1 %s", host);
    FILE *fp = popen(command, "r");
#endif

    if (fp == NULL) {
        return -1.0;
    }

    char line[512];
    float latency = -1.0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *ptr = NULL;

        if (strstr(line, "time=") != NULL) {
            ptr = strstr(line, "time=");
        } else if (strstr(line, "Zeit=") != NULL) {
            ptr = strstr(line, "Zeit=");
        }

        if (ptr != NULL) {
            sscanf(ptr, "%*[^=]=%f", &latency);
        }
    }

#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif

    return latency;
}

float get_max_latency(float *latencies, int count)
{
    float max = 0.0;

    for (int i = 0; i < count; i++) {
        if (latencies[i] > max) {
            max = latencies[i];
        }
    }

    return max;
}

void draw_bar(float latency, float max_latency, int no_color)
{
    int width = 0;

    if (latency > 0.0 && max_latency > 0.0) {
        width = (int)((latency / max_latency) * MAX_BAR_WIDTH);
        if (width > MAX_BAR_WIDTH) {
            width = MAX_BAR_WIDTH;
        }
    }

    if (!no_color && latency >= 0.0) {
        if (latency < 50.0) {
            printf("\033[32m");
        } else if (latency <= 150.0) {
            printf("\033[33m");
        } else {
            printf("\033[31m");
        }
    }

    for (int i = 0; i < width; i++) {
        printf("#");
    }

    for (int i = width; i < MAX_BAR_WIDTH; i++) {
        printf(" ");
    }

    if (!no_color && latency >= 0.0) {
        printf("\033[0m");
    }
}

void log_line(FILE *log_file, const char *host, float latency, latency_stat stat)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    if (latency >= 0.0) {
        float avg = stat.sum / stat.count;
        fprintf(log_file, "[%s] %s  %.2f ms  (min: %.2f  max: %.2f  avg: %.2f)\n",
                timestamp, host, latency, stat.min, stat.max, avg);
    } else {
        fprintf(log_file, "[%s] %s  timeout (timeouts: %d)\n", timestamp, host, stat.timeout_count);
    }
}

void ping_hosts(char *hosts[], int count, latency_stat stats[], FILE *log_file, int no_color)
{
    float latencies[MAX_HOSTS];

    for (int i = 0; i < count; i++) {
        latencies[i] = get_ping_latency(hosts[i]);

        if (latencies[i] >= 0.0) {
            stats[i].sum += latencies[i];
            stats[i].count += 1;
            if (stats[i].count == 1 || latencies[i] < stats[i].min) {
                stats[i].min = latencies[i];
            }
            if (latencies[i] > stats[i].max) {
                stats[i].max = latencies[i];
            }
        } else {
            stats[i].timeout_count += 1;
        }
    }

    float max_latency = get_max_latency(latencies, count);

    printf("\nPinging hosts:\n\n");

    for (int i = 0; i < count; i++) {
        printf("%-20s ", hosts[i]);

        draw_bar(latencies[i], max_latency, no_color);

        if (latencies[i] >= 0.0) {
            float avg = stats[i].sum / stats[i].count;
            printf("  %.2f ms  (min: %.2f  max: %.2f  avg: %.2f)", latencies[i], stats[i].min, stats[i].max, avg);
        } else {
            printf("  timeout (timeouts: %d)", stats[i].timeout_count);
        }

        printf("\n");

        log_line(log_file, hosts[i], latencies[i], stats[i]);
    }

    printf("\n");
    fflush(log_file);
}

void wait_seconds(int seconds)
{
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

int parse_args(int argc, char *argv[], int *interval, int *run_once, char **host_file, int *log_append, int *no_color)
{
    *interval = DEFAULT_INTERVAL;
    *run_once = 0;
    *host_file = NULL;
    *log_append = 0;
    *no_color = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            *interval = atoi(argv[i + 1]);
            if (*interval <= 0) {
                *interval = DEFAULT_INTERVAL;
            }
            i++;
        } else if (strcmp(argv[i], "--once") == 0) {
            *run_once = 1;
        } else if (strcmp(argv[i], "--hosts") == 0 && i + 1 < argc) {
            *host_file = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--log-append") == 0) {
            *log_append = 1;
        } else if (strcmp(argv[i], "--no-color") == 0) {
            *no_color = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("PingGraph - Network Latency Monitor\n\n");
            printf("Usage: pinggraph.exe [options]\n\n");
            printf("Options:\n");
            printf("  --interval N     Ping every N seconds (default: 3)\n");
            printf("  --once           Run one single ping round and exit\n");
            printf("  --hosts FILE     Load hosts from text file\n");
            printf("  --log-append     Append to existing log file instead of overwriting\n");
            printf("  --no-color       Disable ANSI colored output\n");
            printf("  --help, -h       Show this help message\n\n");
            exit(0);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int interval = 0;
    int run_once = 0;
    int log_append = 0;
    int no_color = 0;
    char *host_file = NULL;

    parse_args(argc, argv, &interval, &run_once, &host_file, &log_append, &no_color);

    char *hosts[MAX_HOSTS];
    latency_stat stats[MAX_HOSTS] = {0};
    int host_count = 0;

    if (host_file != NULL) {
        host_count = load_hosts_from_file(host_file, hosts, MAX_HOSTS);
    } else {
        host_count = read_hosts_interactive(hosts, MAX_HOSTS);
    }

    if (host_count <= 0) {
        return 1;
    }

    FILE *log_file = fopen(LOG_FILENAME, log_append ? "a" : "w");
    if (log_file == NULL) {
        printf("Failed to open log file.\n");
        return 1;
    }

    if (run_once) {
        ping_hosts(hosts, host_count, stats, log_file, no_color);
    } else {
        while (1) {
            ping_hosts(hosts, host_count, stats, log_file, no_color);
            wait_seconds(interval);
        }
    }

    for (int i = 0; i < host_count; i++) {
        free(hosts[i]);
    }

    fclose(log_file);

    return 0;
}
