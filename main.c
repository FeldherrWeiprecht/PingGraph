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
#define INTERVAL_SECONDS 3
#define LOG_FILENAME "pinggraph.log"

typedef struct
{
    int count;
    float sum;
    float min;
    float max;
} latency_stat;

int read_hosts(char *hosts[], int max_hosts)
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

void draw_bar(float latency, float max_latency)
{
    int width = 0;

    if (latency > 0.0 && max_latency > 0.0) {
        width = (int)((latency / max_latency) * MAX_BAR_WIDTH);
        if (width > MAX_BAR_WIDTH) {
            width = MAX_BAR_WIDTH;
        }
    }

    if (latency >= 0.0) {
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

    if (latency >= 0.0) {
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
        fprintf(log_file, "[%s] %s  timeout\n", timestamp, host);
    }
}

void ping_hosts(char *hosts[], int count, latency_stat stats[], FILE *log_file)
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
        }
    }

    float max_latency = get_max_latency(latencies, count);

    printf("\nPinging hosts:\n\n");

    for (int i = 0; i < count; i++) {
        printf("%-20s ", hosts[i]);

        draw_bar(latencies[i], max_latency);

        if (latencies[i] >= 0.0) {
            float avg = stats[i].sum / stats[i].count;
            printf("  %.2f ms  (min: %.2f  max: %.2f  avg: %.2f)", latencies[i], stats[i].min, stats[i].max, avg);
        } else {
            printf("  timeout");
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

int main()
{
    char *hosts[MAX_HOSTS];
    latency_stat stats[MAX_HOSTS] = {0};
    int host_count = read_hosts(hosts, MAX_HOSTS);

    if (host_count <= 0) {
        return 1;
    }

    FILE *log_file = fopen(LOG_FILENAME, "w");
    if (log_file == NULL) {
        printf("Failed to open log file.\n");
        return 1;
    }

    while (1) {
        ping_hosts(hosts, host_count, stats, log_file);
        wait_seconds(INTERVAL_SECONDS);
    }

    for (int i = 0; i < host_count; i++) {
        free(hosts[i]);
    }

    fclose(log_file);

    return 0;
}
