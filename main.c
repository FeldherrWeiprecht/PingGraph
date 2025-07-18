#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HOSTS 100
#define MAX_LENGTH 256

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

int main()
{
    char *hosts[MAX_HOSTS];
    int host_count = read_hosts(hosts, MAX_HOSTS);

    if (host_count <= 0) {
        return 1;
    }

    printf("\nEntered hosts:\n\n");
    for (int i = 0; i < host_count; i++) {
        printf("Host %d: %s\n", i + 1, hosts[i]);
    }

    for (int i = 0; i < host_count; i++) {
        free(hosts[i]);
    }

    return 0;
}
