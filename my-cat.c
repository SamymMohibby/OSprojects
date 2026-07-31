#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    char buffer[4096];

    if (argc == 1) {
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            fprintf(stderr, "my-cat: cannot open file\n");
            return 1;
        }

        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            printf("%s", buffer);
        }

        fclose(fp);
    }

    return 0;
}
