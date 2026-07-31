#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int search_stream(FILE *fp, const char *term)
{
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    while ((length = getline(&line, &capacity, fp)) != -1) {
        (void)length;

        if (strstr(line, term) != NULL) {
            printf("%s", line);
        }
    }

    free(line);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "my-grep: searchterm [file ...]\n");
        return 1;
    }

    const char *term = argv[1];

    if (argc == 2) {
        return search_stream(stdin, term);
    }

    for (int i = 2; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            fprintf(stderr, "my-grep: cannot open file\n");
            return 1;
        }

        search_stream(fp, term);
        fclose(fp);
    }

    return 0;
}
