#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "my-unzip: file1 [file2 ...]\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        FILE *fp = fopen(argv[i], "r");

        if (fp == NULL) {
            fprintf(stderr, "my-unzip: cannot open file\n");
            return 1;
        }

        int count;
        char ch;

        while (fread(&count, sizeof(int), 1, fp) == 1) {
            if (fread(&ch, sizeof(char), 1, fp) != 1) {
                break;
            }

            for (int j = 0; j < count; j++) {
                fputc(ch, stdout);
            }
        }

        fclose(fp);
    }

    return 0;
}
