#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    printf("hello from mlibc\n");
    printf("argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]=%s\n", i, argv[i]);
    }

    char *p = malloc(64);
    strcpy(p, "malloc works");
    printf("malloc: %s\n", p);
    free(p);

    return 0;
}
