#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char error_message[] = "An error has occurred\n";

static void print_error(void)
{
    write(STDERR_FILENO, error_message, strlen(error_message));
}

static char **shell_paths = NULL;
static size_t path_count = 0;

static void free_paths(void)
{
    for (size_t i = 0; i < path_count; i++) {
        free(shell_paths[i]);
    }
    free(shell_paths);
    shell_paths = NULL;
    path_count = 0;
}

static int set_paths(char **paths, size_t count)
{
    char **new_paths = NULL;

    if (count > 0) {
        new_paths = malloc(count * sizeof(char *));
        if (new_paths == NULL) {
            return -1;
        }

        for (size_t i = 0; i < count; i++) {
            new_paths[i] = strdup(paths[i]);
            if (new_paths[i] == NULL) {
                for (size_t j = 0; j < i; j++) {
                    free(new_paths[j]);
                }
                free(new_paths);
                return -1;
            }
        }
    }

    free_paths();
    shell_paths = new_paths;
    path_count = count;
    return 0;
}

static char *trim_whitespace(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r') {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    char *end = text + strlen(text) - 1;
    while (end > text &&
           (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    return text;
}

static char **split_arguments(char *command, int *argument_count)
{
    size_t capacity = 8;
    size_t count = 0;
    char **arguments = malloc(capacity * sizeof(char *));

    if (arguments == NULL) {
        return NULL;
    }

    char *save_ptr = NULL;
    char *token = strtok_r(command, " \t\r\n", &save_ptr);

    while (token != NULL) {
        if (count + 1 >= capacity) {
            capacity *= 2;
            char **resized = realloc(arguments, capacity * sizeof(char *));
            if (resized == NULL) {
                free(arguments);
                return NULL;
            }
            arguments = resized;
        }

        arguments[count++] = token;
        token = strtok_r(NULL, " \t\r\n", &save_ptr);
    }

    arguments[count] = NULL;
    *argument_count = (int)count;
    return arguments;
}

static char *find_executable(const char *command)
{
    for (size_t i = 0; i < path_count; i++) {
        size_t length = strlen(shell_paths[i]) + strlen(command) + 2;
        char *full_path = malloc(length);

        if (full_path == NULL) {
            return NULL;
        }

        snprintf(full_path, length, "%s/%s", shell_paths[i], command);

        if (access(full_path, X_OK) == 0) {
            return full_path;
        }

        free(full_path);
    }

    return NULL;
}

static int execute_builtin(char **arguments, int argument_count)
{
    if (argument_count == 0) {
        return 1;
    }

    if (strcmp(arguments[0], "exit") == 0) {
        if (argument_count != 1) {
            print_error();
            return 1;
        }

        free_paths();
        exit(0);
    }

    if (strcmp(arguments[0], "cd") == 0) {
        if (argument_count != 2 || chdir(arguments[1]) != 0) {
            print_error();
        }
        return 1;
    }

    if (strcmp(arguments[0], "path") == 0) {
        if (set_paths(&arguments[1], (size_t)(argument_count - 1)) != 0) {
            print_error();
        }
        return 1;
    }

    return 0;
}

static pid_t execute_external(char **arguments, const char *output_file)
{
    char *executable = find_executable(arguments[0]);

    if (executable == NULL) {
        print_error();
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        print_error();
        free(executable);
        return -1;
    }

    if (pid == 0) {
        if (output_file != NULL) {
            int output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (output_fd < 0) {
                print_error();
                free(executable);
                _exit(1);
            }

            if (dup2(output_fd, STDOUT_FILENO) < 0 ||
                dup2(output_fd, STDERR_FILENO) < 0) {
                print_error();
                close(output_fd);
                free(executable);
                _exit(1);
            }

            close(output_fd);
        }

        execv(executable, arguments);
        print_error();
        free(executable);
        _exit(1);
    }

    free(executable);
    return pid;
}

static pid_t process_command(char *command)
{
    command = trim_whitespace(command);
    if (*command == '\0') {
        return -1;
    }

    char *output_file = NULL;
    char *redirect = strchr(command, '>');

    if (redirect != NULL) {
        if (strchr(redirect + 1, '>') != NULL) {
            print_error();
            return -1;
        }

        *redirect = '\0';
        char *right_side = trim_whitespace(redirect + 1);
        char *left_side = trim_whitespace(command);

        if (*left_side == '\0' || *right_side == '\0') {
            print_error();
            return -1;
        }

        char *save_ptr = NULL;
        char *filename = strtok_r(right_side, " \t\r\n", &save_ptr);
        char *extra = strtok_r(NULL, " \t\r\n", &save_ptr);

        if (filename == NULL || extra != NULL) {
            print_error();
            return -1;
        }

        output_file = filename;
        command = left_side;
    }

    int argument_count = 0;
    char **arguments = split_arguments(command, &argument_count);

    if (arguments == NULL) {
        print_error();
        return -1;
    }

    if (argument_count == 0) {
        free(arguments);
        return -1;
    }

    if (execute_builtin(arguments, argument_count)) {
        free(arguments);
        return -1;
    }

    pid_t pid = execute_external(arguments, output_file);
    free(arguments);
    return pid;
}

static void process_line(char *line)
{
    size_t capacity = 8;
    size_t count = 0;
    pid_t *children = malloc(capacity * sizeof(pid_t));

    if (children == NULL) {
        print_error();
        return;
    }

    char *save_ptr = NULL;
    char *command = strtok_r(line, "&", &save_ptr);

    while (command != NULL) {
        pid_t pid = process_command(command);

        if (pid > 0) {
            if (count == capacity) {
                capacity *= 2;
                pid_t *resized = realloc(children, capacity * sizeof(pid_t));
                if (resized == NULL) {
                    print_error();
                    break;
                }
                children = resized;
            }

            children[count++] = pid;
        }

        command = strtok_r(NULL, "&", &save_ptr);
    }

    for (size_t i = 0; i < count; i++) {
        waitpid(children[i], NULL, 0);
    }

    free(children);
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    int interactive = 1;

    if (argc > 2) {
        print_error();
        exit(1);
    }

    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            exit(1);
        }
        interactive = 0;
    }

    char *initial_path[] = {"/bin"};
    if (set_paths(initial_path, 1) != 0) {
        print_error();
        if (input != stdin) {
            fclose(input);
        }
        exit(1);
    }

    char *line = NULL;
    size_t capacity = 0;

    while (1) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }

        if (getline(&line, &capacity, input) == -1) {
            break;
        }

        process_line(line);
    }

    free(line);
    free_paths();

    if (input != stdin) {
        fclose(input);
    }

    return 0;
}
