#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

typedef struct Line {
    char *text;
    struct Line *next;
} Line;

static void free_lines(Line *head)
{
    while (head != NULL) {
        Line *next = head->next;
        free(head->text);
        free(head);
        head = next;
    }
}

static void malloc_error(Line *head, FILE *input, FILE *output)
{
    fprintf(stderr, "malloc failed\n");

    free_lines(head);

    if (input != NULL && input != stdin) {
        fclose(input);
    }

    if (output != NULL && output != stdout) {
        fclose(output);
    }

    exit(1);
}

static int files_are_same(const char *input_name, const char *output_name)
{
    struct stat input_stat;
    struct stat output_stat;

    /* Immediately detect identical path names. */
    if (strcmp(input_name, output_name) == 0) {
        return 1;
    }

    /*
     * If both files already exist, compare their device and inode numbers.
     * This also detects symbolic links and hard links to the same file.
     */
    if (stat(input_name, &input_stat) == 0 &&
        stat(output_name, &output_stat) == 0) {
        return input_stat.st_dev == output_stat.st_dev &&
               input_stat.st_ino == output_stat.st_ino;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    FILE *output = stdout;

    Line *head = NULL;
    char *line = NULL;
    size_t capacity = 0;
    ssize_t length;

    if (argc > 3) {
        fprintf(stderr, "usage: reverse <input> <output>\n");
        exit(1);
    }

    if (argc == 3 && files_are_same(argv[1], argv[2])) {
        fprintf(stderr, "Input and output file must differ\n");
        exit(1);
    }

    if (argc >= 2) {
        input = fopen(argv[1], "r");

        if (input == NULL) {
            fprintf(stderr, "error: cannot open file '%s'\n", argv[1]);
            exit(1);
        }
    }

    if (argc == 3) {
        output = fopen(argv[2], "w");

        if (output == NULL) {
            fprintf(stderr, "error: cannot open file '%s'\n", argv[2]);
            fclose(input);
            exit(1);
        }
    }

    /*
     * Add each line to the beginning of the linked list.
     * This automatically stores the lines in reverse order.
     */
    while ((length = getline(&line, &capacity, input)) != -1) {
        Line *new_line = malloc(sizeof(Line));

        if (new_line == NULL) {
            free(line);
            malloc_error(head, input, output);
        }

        new_line->text = line;
        new_line->next = head;
        head = new_line;

        /*
         * Make getline allocate a new buffer for the next line.
         * The current buffer now belongs to the linked-list node.
         */
        line = NULL;
        capacity = 0;
    }

    /*
     * getline may allocate a buffer before reaching EOF,
     * so release any buffer that was not added to the list.
     */
    free(line);

    /*
     * If getline failed because memory allocation failed,
     * print the required error message.
     */
    if (ferror(input) && errno == ENOMEM) {
        malloc_error(head, input, output);
    }

    for (Line *current = head; current != NULL; current = current->next) {
        fprintf(output, "%s", current->text);
    }

    free_lines(head);

    if (input != stdin) {
        fclose(input);
    }

    if (output != stdout) {
        fclose(output);
    }

    return 0;
}