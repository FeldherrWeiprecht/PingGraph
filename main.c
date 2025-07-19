#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HOSTS 100
#define MAX_LENGTH 256
#define MAX_BAR_WIDTH 50

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

void ping_hosts(char *hosts[], int count)
{
    float latencies[MAX_HOSTS];

    for (int i = 0; i < count; i++) {
        latencies[i] = get_ping_latency(hosts[i]);
    }

    float max_latency = get_max_latency(latencies, count);

    printf("\nPinging hosts:\n\n");

    for (int i = 0; i < count; i++) {
        printf("%-20s ", hosts[i]);

        draw_bar(latencies[i], max_latency);

        if (latencies[i] >= 0.0) {
            printf("  %.2f ms", latencies[i]);
        } else {
            printf("  timeout");
        }

        printf("\n");
    }

    printf("\n");
}

int main()
{
    char *hosts[MAX_HOSTS];
    int host_count = read_hosts(hosts, MAX_HOSTS);

    if (host_count <= 0) {
        return 1;
    }

    ping_hosts(hosts, host_count);

    for (int i = 0; i < host_count; i++) {
        free(hosts[i]);
    }

    return 0;
}
