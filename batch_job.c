// Jackson Shaw COMP7500 OS3 Project
// Compiled via Makfile. See aubatch.c for further details
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <cpu_time>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int seconds = atoi(argv[1]);
    sleep(seconds);

    return EXIT_SUCCESS;
}
