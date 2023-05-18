#include "main.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/** The max characters of one cmdline (incl. NULL terminator) */
#define MAX_LINE 8192

/** The max number of pipes that can occur in one cmdline */
#define MAX_PIPE 16

#define EMOJI_BIRD "\xF0\x9F\x90\xA6"
char *prompt = "ttsh[%d] " EMOJI_BIRD " ";

/** Whether or not to show the prompt */
int show_prompt = 1;

char *cmdname;


/** Run a node and obtain an exit status. */
int invoke_node(node_t *node) {
    /* You can modify any part of the body of this function.
       Adding new functions is, of course, also allowed. */

    LOG("Invoke: %s", inspect_node(node));

    switch (node->type) {
    case N_COMMAND:
        /* change directory (cd with no argument is not supported.) */
        if (strcmp(node->argv[0], "cd") == 0) {
            if (node->argv[1] == NULL) {
                return 0; // do nothing
            } else if (chdir(node->argv[1]) == -1) {
                perror("cd");
                return errno;
            } else {
                return 0;
            }
        }

        /* Simple command execution (Task 1) */
        int status_1;
        pid_t pid_1;
        fflush(stdout);
        pid_1 = fork();
        
        if (pid_1 == 0){
            execvp(*node->argv, &node->argv[0]);
            exit(0);
        } else {
            wait(&status_1);
        }
        // char *argv[] = {"whoami", NULL};
        // execvp("whoami", argv);

        break;

    case N_PIPE: /* foo | bar */
        LOG("node->lhs: %s", inspect_node(node->lhs));
        LOG("node->rhs: %s", inspect_node(node->rhs));

        /* Pipe execution (Tasks 3 and A) */
        pid_t pid_3;
        int status_3;
        int fd_3[2];
        fflush(stdout);
        
        if (pipe(fd_3) < 0) {
            perror("pipe error¥n");
            exit(-1);
        }
        
        pid_3 = fork();

        if (pid_3 == 0){
            dup2(fd_3[1], 1);
            close(fd_3[0]);
            close(fd_3[1]);
            execvp(*node->lhs->argv, &node->lhs->argv[0]);
            exit(0);
        }

        pid_3 = fork();
        
        if (pid_3 == 0){
            dup2(fd_3[0], 0);
            close(fd_3[0]);
            close(fd_3[1]);
            execvp(*node->rhs->argv, &node->rhs->argv[0]);
            exit(0);
        }

        close(fd_3[0]);
        close(fd_3[1]);
        wait(&status_3);
        wait(&status_3);

        break;
        
    case N_REDIRECT_IN:     /* foo < bar */
    case N_REDIRECT_OUT:    /* foo > bar */
    case N_REDIRECT_APPEND: /* foo >> bar */
        LOG("node->filename: %s", node->filename);

        /* Redirection (Task 4) */
        int status_4;
        int fd_4;
        pid_t pid_4;
        fflush(stdout);
        pid_4 = fork();
        
        if (pid_4 == 0){
            switch (node->type){
                case N_REDIRECT_APPEND:
                    if ((fd_4 = open(node->filename, O_WRONLY |O_APPEND | O_CREAT, 0666)) < 0){
                        perror("file open error¥n");
                        exit(-1);
                    }else{
                        dup2(fd_4, 1);
                        break;
                    }
                    
                    
                case N_REDIRECT_IN:
                    if ((fd_4 = open(node->filename, O_RDONLY)) < 0){
                        perror("file open error¥n");
                        exit(-1);
                    }else{
                        dup2(fd_4, 0);
                        break;
                    }

                case N_REDIRECT_OUT:
                default:
                    if ((fd_4 = open(node->filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0){
                        perror("file open error¥n");
                        exit(-1);
                    }else{
                        dup2(fd_4, 1);
                        break;
                    }
            }
            close(fd_4);
            switch (node->lhs->type){
                case N_REDIRECT_IN:
                case N_REDIRECT_OUT:
                case N_REDIRECT_APPEND:
                case N_PIPE:
                    invoke_node(node->lhs);
                    break;
                default:
                    break;
            }
            execvp(*node->lhs->argv, &node->lhs->argv[0]);
            exit(0);
        } else {
            wait(&status_4);
        }
        break;

    case N_SEQUENCE: /* foo ; bar */
        LOG("node->lhs: %s", inspect_node(node->lhs));
        LOG("node->rhs: %s", inspect_node(node->rhs));

        /* Sequential execution (Task 2) */
        invoke_node(node->lhs);
        invoke_node(node->rhs);
        break;

    case N_AND: /* foo && bar */
    case N_OR:  /* foo || bar */
        LOG("node->lhs: %s", inspect_node(node->lhs));
        LOG("node->rhs: %s", inspect_node(node->rhs));

        /* Branching (Task B) */

        break;

    case N_SUBSHELL: /* ( foo... ) */
        LOG("node->lhs: %s", inspect_node(node->lhs));

        /* Subshell execution (Task C) */

        break;

    default:
        assert(false);
    }
    return 0;
}

void parse_options(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "qp")) != -1) {
        switch (opt) {
        case 'q': /* -q: quiet */
            l_set_quiet(1);
            break;
        case 'p': /* -p: no-prompt */
            show_prompt = 0;
            break;
        case '?':
        default:
            fprintf(stderr, "Usage: %s [-q] [-p] [cmdline ...]\n", cmdname);
            exit(EXIT_FAILURE);
        }
    }
    if (getenv("NO_EMOJI") != NULL) {
        prompt = "ttsh[%d]> ";
    }
}

int invoke_line(char *line) {
    LOG("Input line='%s'", line);
    node_t *node = yacc_parse(line);
    if (node == NULL) {
        LOG("Obtained empty line: ignored");
        return 0;
    }
    if (!l_get_quiet()) {
        dump_node(node, stdout);
    }
    int exit_status = invoke_node(node);
    free_node(node);
    return exit_status;
}

int main(int argc, char **argv) {
    cmdname = argv[0];
    parse_options(argc, argv);
    if (optind < argc) {
        /* Execute each cmdline in the arguments if exists */
        int exit_status;
        for (int i = optind; i < argc; i++) {
            exit_status = invoke_line(argv[i]);
        }
        return exit_status;
    }

    for (int history_id = 1;; history_id++) {
        char line[MAX_LINE];
        if (show_prompt) {
            printf(prompt, history_id);
        }
        /* Read one line */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* EOF: Ctrl-D (^D) */
            return EXIT_SUCCESS;
        }
        /* Erase line breaks */
        char *p = strchr(line, '\n');
        if (p) {
            *p = '\0';
        }
        invoke_line(line);
    }
}
