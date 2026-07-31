#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "my-zip: file1 [file2 ...]\n");
        return 1;
    }

    int current = EOF;
    int count = 0;

    for (int i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            fprintf(stderr, "my-zip: cannot open file\n");
            return 1;
        }

        int ch;

        while ((ch = fgetc(fp)) != EOF) {
            if (current == EOF) {
                current = ch;
                count = 1;
            } else if (ch == current) {
                count++;
            } else {
                fwrite(&count, sizeof(int), 1, stdout);
                fputc(current, stdout);

                current = ch;
                count = 1;
            }
        }

        fclose(fp);
    }

    if (current != EOF) {
        fwrite(&count, sizeof(int), 1, stdout);
        fputc(current, stdout);
    }

    return 0;
}
