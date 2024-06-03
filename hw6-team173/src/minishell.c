/*
* baa2165
* sjc2235
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>

#define BRIGHTBLUE "\x1b[34;1m"
#define DEFAULT    "\x1b[0m"
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define _GNU_SOURCE

volatile sig_atomic_t signal_val = 0;

void catch_signal(int sig) {
    signal_val = sig;
}

/*
Changes directory to specified path.
*/

char* change_directory(char* dir) {
    struct passwd *pw;
    uid_t uid;
    uid = getuid();
    if((pw = getpwuid(uid)) == NULL){
        fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
    }

    char buf[PATH_MAX];
    if (dir == NULL || dir[0] == '~') {
        // Construct the new directory path by expanding the tilde character
        char *home = pw->pw_dir;
        char *rest = (dir != NULL) ? dir+1 : ""; // Skip the tilde character if present
        snprintf(buf, PATH_MAX, "%s/%s", home, rest);
        dir = buf;
    }

    if(chdir(dir) != 0){
        fprintf(stderr, "Error: Cannot change directory to '%s'. %s.\n", dir,
                strerror(errno));    
    }

    char *cwd = getcwd(buf, PATH_MAX);
    if (cwd == NULL) {
        fprintf(stderr, "Error: Cannot get current working directory. %s.\n", 
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    return cwd;
}

/*
Takes a command and fills the token buffer with tokens
Returns number of tokens in command
*/

char **tokenize(char *command, int *num_tokens) {
    char **tokens = NULL;
    int space_rule = 1;
    int token_count = 0;
    int len = strlen(command);
    char *token = (char*) malloc(len + 1);
    if (token == NULL) {
        fprintf(stderr, "Error: malloc() failed. %s.\n", strerror(errno));
        return NULL;
    }
    int i = 0, j = 0;
    while (i < len) {
        if (command[i] == '"') {
            space_rule = !space_rule;
            i++;
        } else if (command[i] == ' ' && space_rule) {
            // end of token
            token[j] = '\0';
            if (j > 0) {
                token_count++;
                tokens = (char**) realloc(tokens, token_count * sizeof(char*));
                if (tokens == NULL) {
                    fprintf(stderr, "Error: realloc() failed. %s.\n", strerror(errno));
                    return NULL;
                }
                tokens[token_count - 1] = token;
                token = (char*) malloc(len + 1);
                if (tokens == NULL) {
                    fprintf(stderr, "Error: malloc() failed. %s.\n", strerror(errno));
                    return NULL;
                }
                j = 0;
            }
            i++;
        } else {
            token[j] = command[i];
            j++;
            i++;
        }
    }
    token[j] = '\0';
    if (j > 0) {
        token_count++;
        tokens = (char**) realloc(tokens, token_count * sizeof(char*));
        if (tokens == NULL) {
            fprintf(stderr, "Error: realloc() failed. %s.\n", strerror(errno));
            return NULL;
        }
        tokens[token_count - 1] = token;
    } else {
        free(token);
    }
    if (!space_rule) {
        fprintf(stderr, "Error: Malformed command.\n");
        return NULL;
    }
    *num_tokens = token_count;
    tokens = (char**) realloc(tokens, (token_count + 1) * sizeof(char*));
    if (tokens == NULL) {
        fprintf(stderr, "Error: realloc() failed. %s.\n", strerror(errno));
        return NULL;
    }
    tokens[token_count] = NULL;
    return tokens;
}

void free_tokens(char **tokens, int num_tokens) {
    for (int i = 0; i < num_tokens; i++) {
        free(*(tokens+i));
    }
    free(tokens);
}

int main() {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = catch_signal;
    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction(SIGINT)");
        return EXIT_FAILURE;
    }

    char command[MAX_COMMAND_LENGTH];
    char buf[PATH_MAX];
    
    while (1) {
        char *cwd = getcwd(buf, PATH_MAX);
        if (cwd == NULL) {
            fprintf(stderr, "Error: Cannot get current working directory. %s.\n", 
                    strerror(errno));
            exit(EXIT_FAILURE);
        }
        printf("[%s%s%s]$ ", BRIGHTBLUE, cwd, DEFAULT);
        fflush(stdout);
        // Read user input
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            if (errno == EINTR) {
                printf("\n");
                errno = 0;
                continue;
            } else if (feof(stdin)) {
                printf("\n");
                exit(EXIT_SUCCESS);
            } else if (ferror(stdin)) {
                fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        char *eoln = strchr(command, '\n');
        if (eoln != NULL) {
            *eoln = '\0';
        }
        
        int num_tokens = 0;
        char** tokens = tokenize(command, &num_tokens);

        fflush(stdout);
        if (!strncmp(command, "exit", 4)) {
            free_tokens(tokens, num_tokens);
            break;
        }
        if (num_tokens > 0) {
            if (!strcmp(tokens[0], "cd")) {
                if (num_tokens > 2) {
                    free_tokens(tokens, num_tokens);
                    fprintf(stderr, "Error: Too many arguments to cd.\n");
                } else {
                    change_directory(tokens[1]);
                    free_tokens(tokens, num_tokens);
                }
            } else {
                pid_t pid = fork(); //fork here
                if (pid == -1) {
                    fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
                }
                if (pid == 0) {
                    if (execvp(*tokens, tokens) == -1) {
                        fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
                        free_tokens(tokens, num_tokens);
                        exit(1);
                    }
                }
                free_tokens(tokens, num_tokens);
                waitpid(pid, NULL, 0);
            }
        }
    }
    return EXIT_SUCCESS;
}