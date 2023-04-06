/* ###  Preprocessor Directives  ### */
/* Enabling POSIX Compliance */
#define _POSIX_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1

/* Included Libraries */

#include <stdio.h>      // I/O Operations (fgets, printf, ...)
#include <stdlib.h>     // STD Library (malloc, ...)
#include <unistd.h>     // POSIX API (read, write, fork, ...)
#include <sys/types.h>  // Additional Types (pid_t, ...)
#include <sys/wait.h>   // Hold Processes (wait(), waitpid(), waitid())
#include <errno.h>      // Errno Retrieving
#include <signal.h>     // Signal Processing
#include <dirent.h>     // Directory Processing (opendir, closedir)
#include <fcntl.h>      // More directory processing (open, close)

/* Macro Definitions */

#define PROMPT "linux> " // Prompt to be printed at the start of the shell
#define PROMPTSIZE 256   // Maximum number of chars in the prompt
#define TOKENSIZE 128    // Maximum number of tokens the user can input
#define ENVSIZE 4        // Maximum number of strings the env can contain
#define ENVFIELDSIZE 256 // Maximum size of any field of the ENV
#define SHELL "simpsh"   // Shell ID for env
#define TERM "dumb"      // Terminal type 
#define INPUT 1          // Redirect file into input
#define OUTPUT 2         // Redirect output into file
#define MAXREDIR 2       // Max number of redirections (2, ie ./exec < input > output)
/* Path to add to env. Searched for system executables by the shell, to add more directories 
 * to path, simply append :/DIR_TO_ADD, as Shell automatically parses and searches any directories
 * separated by colons */
#define PATH "/usr/bin/:"     

typedef void (*sighandler_t)(int); // Signal handler type



/* ###  Global Variables  ### */

// Interruption State. 0 by default, 1 if SIGINT is caught
volatile sig_atomic_t interrupt_state = 0;  



/* ###  Function Templates  ### */

/* Wrapper Functions */

// Fork wrapper function
pid_t Fork (void);

// Wait wrapper function
pid_t Wait (int *child_status);

// Signal wrapper function, provides an interface for sigaction
void Signal (int signum, sighandler_t handler);

/* Signal Handlers */

// SIGINT handler function
static void SIGINT_HANDLER ();

/* String Functions */

/* Takes a string and Tokenizes it into multiple smaller strings separated by delim.
 * Removes every instance of the delim found in the input string. */
char **Strtok (char *str, char delim, int *redir);

/* Returns the line typed into STDIN as a string. Takes no arguments.
 * If something goes catastropically wrong (or EOF), it calls 
 * DIE_WITH_ERROR to exit gracefully. */
char *Getline ();

/* Appends t to s */
void Append (char *s, char *t);

/* Prepends t to s */
void Prepend (char *s, char *t);

/* Uses string comparison to find the greater string. If s > t, returns 1. If t > s, returns 0. */
int Strcmp (char *s, char *t);

/* Running Commands */

/* Executes the command given in the args, as env */
int Execute (char **args, char **env, int *redir);

/* Redirects I/O from/to stdin/stdout */
int Redirect (char *file_name, int direction);

/* Setting up ENV */
char **Initenv ();

void Setenv (char *err_str, int err_int);

/* Gets a file path for bin */
void Getpath (char *bin);

/* Error Handling */

/* Take a string message to send to perror, then kill the program */
void DIE_WITH_ERROR (char *msg);



/* ###  Main  ### */

int main() {
    // Installing default signal handler
    Signal (SIGINT, SIGINT_HANDLER);
    int retval;

    /* Initialize the environment */
    char **env = Initenv();

    // Logic loop of the shell. sets env, gets command from user, forks & execs
    while (1) {
        //Setenv (env, retval);
        // Display prompt
        printf (PROMPT);
        
        // reset interrupt state flag
        interrupt_state = 0;

        char *inputstr = Getline ();

        /* If interrupted, go to next iteration of the loop */
        if (interrupt_state) {
            free (inputstr);
            continue;
        }

        int *redir = malloc (sizeof (int));
        /* Get a string array of arguments the user entered */
        char **tokens = Strtok (inputstr, 0x20, redir);

        /* Free input str */
        free (inputstr);

        /* Update ? */
        if ((retval = Execute (tokens, env, redir)) != 255)
            Setenv (env[3], retval);

        /* Freeing redirection int */
        free(redir);

        /* Freeing the tokens */
        if (*tokens) {
            for (int i = 0; tokens[i]; i++)
                free (tokens[i]);
            free (tokens);
        }
        
    }

    /* Freeing the environment */
    for (int i = 0; env[i]; i++)
        free (env[i]);
    free (env);
    return 0;
}



/* ###  Function Definitions  ### */

// Error message
void DIE_WITH_ERROR (char *msg) {
    perror (msg);
    exit (0);
}

/* Wrapper Functions */

// Fork wrapper function
pid_t Fork (void) {
    pid_t cpid;

    if ((cpid = fork ()) < 0)
        DIE_WITH_ERROR ("Fork error");
    return cpid;
}

// Wait wrapper function
pid_t Wait (int *child_status) {
    // Blocks until child is done
    pid_t wait_pid = wait (child_status);
    // If the child exited wait prematurely, notify the user and exit
    if (!interrupt_state && !WIFEXITED(*child_status)) {
        DIE_WITH_ERROR("Child exited abnormally");
        exit(0);
    }
    return wait_pid;
}

// Signal wrapper function for signal handling
void Signal (int signum, sighandler_t handler) {
    struct sigaction action;
    // sets new action to input handler
    action.sa_handler = handler;
    sigemptyset (&action.sa_mask);

    // if sigaction fails, return with error
    if (sigaction (signum, &action, NULL) != 0)
        DIE_WITH_ERROR ("Signal error");
}

// SIGINT handler function
static void SIGINT_HANDLER () {
    // Set interrupt state to 1 such that user does not enter fork and exec, 
    // Otherwise, seg fault
    interrupt_state = 1;
    // print newline
    write (STDOUT_FILENO, "\n", 1);

    // Reaps all child processes (naive solution, but without the while loop it doesn't properly kill the child process)
    while (errno != ECHILD)
        waitpid(-1, 0, WNOHANG);
}

char *Getline () {
    // Allocate space for input string
    char *inputstr = malloc(PROMPTSIZE * sizeof (char));

    // Get user input
    if (fgets (inputstr, PROMPTSIZE, stdin) == NULL) {
        if (feof (stdin)) {
            printf("\nexit\n");
            free (inputstr);
            exit (0);
        }
        else if (!interrupt_state)
            DIE_WITH_ERROR ("STDIN");
    }
//    /* Flushing stdin stream if user input more than 256 characters */
//    if (inputstr[sizeof(inputstr) - 1]) {
//        perror("STDIN");
//        char waste_ch;
//        /* Consume input until EOF */
//        while ((waste_ch = fgetc(STDIN_FILENO)) != 0x0a && waste_ch != EOF);
//    }
    /* Return the input */
    return inputstr;
}

char **Strtok (char *str, char delim, int *redir) {
    // Parse the string into multiple substrings
    // malloc 128 strings (max possible, might be a bit excessive)
    char **tokens = malloc (TOKENSIZE * sizeof(char *));
    *tokens = 0x0;
    int token_index = 0, redir_count = 0;
    *redir = 0;

    // temporary pointer to not move the token string pointer beyond where it should go
    char *tmpptr;
    char *str_begin = str;

    while (*str) {
        // malloc size for the token string
        tokens[token_index] = malloc (PROMPTSIZE * sizeof(char));
        *tokens[token_index] = 0x0;

        // temporary pointer to the first character in the token string
        tmpptr = tokens[token_index];

        // delete all leading whitespace
        while (*str && (*str == 0x0a || *str == delim || *str == 0x09))
            str++;

        // read the word until whitespace (also accounts for redirection)
        while (*str && (*str != 0x0a && *str != delim && *str != 0x09) && (*str != 0x3c && *str != 0x3e))
            *tmpptr++ = *str++;

        /* At most, we can have 2 redirections, i.e. ./exec < input.txt > output.txt */
        if (str != str_begin) {
            if (redir_count < MAXREDIR) {
                /* If redirect symbol was found */
                if (*str == 0x3c){ // '<', Input redirection
                    *str = 0x20;
                    *redir = 1;
                    redir_count++;
                } 
                else if (*str == 0x3e) { // '>' Output redirection
                    *str = 0x20;
                    *redir = 2;
                    redir_count++;
                }
            }
            /* If 2 redirections, assume user inputed as ./exec < input.txt > output.txt */
            else if (*redir != 3) {
                *redir = 3;
            }
            else {
                errno = 1;
                DIE_WITH_ERROR ("simpsh");
            }
        }
        /* Only if token isnt empty */
        if (!Strcmp (tokens[token_index], "")) {
            *tmpptr = 0x0; // null terminate the strings
            /* Increment index only if str hasn't been fully read and if the token wasn't a redir */
            if (*str)
                token_index++;
        }
    }
    // Null terminate token
    tokens[token_index] = NULL;
    return tokens;
}

/* Forks the shell and executes the process in the string array args. Returns the exit status of the child process */
int Execute (char **args, char **env, int *redir) {
    int child_status, exit_status = 0;
    if (!args || !*args || *args[0] == 0x0)
        return 255;

    /* Exit if user chose to exit */
    if (Strcmp(args[0], "exit")) {
        if (args[1] == 0x0)
            exit(0);
        /* If 2nd argument is not null, attempt to parse the integer and exit(int) */
        errno = 0;
        int exit_val = strtol(args[1], NULL, 10);

        /* if errno changed after above command, something went wrong. */
        if (errno != 0)
            DIE_WITH_ERROR("strtol");

        /* Otherwise, exit with code found from 2nd arg */
        exit(exit_val);
    }

    /* Fork the process and check for errors */
    if (!interrupt_state && args[0] != 0x0 && *args[0] != 0x0 && (Fork()) == 0) {  // In child, exec the called function
        /* If number of redirections is 2, (indicated by 3 because), redirect
         *  assume first file is input and the second file is output. ie the user entered ./exec < input > output
         */
        if (*redir == 3) {
            Redirect(args[1], INPUT);
            Redirect(args[2], OUTPUT);
        }
        else if (*redir != 0) {
            Redirect(args[1], *redir);
            args[1] = NULL;
        }
        /* Attempting to exec the user input */
        if (*args[0] != 0x0 && execve (args[0], args, env) < 0) {
            /* Only search path if file not found in cwd, and if argument does not start with / or . */
            if (errno == ENOENT && *args[0] != 0x2f && *args[0] != 0x2e) {
                /* get the path of the binary */
                Getpath (args[0]);

                /* Try to execute again */
                if (execve(args[0], args, env) < 0) {
                    /* Failed to execute */
                    /* Free used memory */
                    for (int i = 0; args[i]; i++)
                        free (args[i]);
                    free (args);

                    /* Exit the child with error code 255 */
                    exit(255);
                }
            }
            else {
                perror("simpsh");
            }
        }
    }
    /* In parent, wait on child and get exit status */
    else if (!interrupt_state && args[0] != 0x0) {
        Wait (&child_status);
        if (WIFEXITED(child_status)) {
            exit_status = WEXITSTATUS(child_status);
        }
    }
    return exit_status;
}

int Redirect (char *file_name, int direction) {
    int file;
    /* If user is redirecting from file to stdin */
    if (direction == INPUT) {
        /* Open file */
        if ((file = open (file_name, O_RDONLY, 00400|00200)) < 0)
            perror ("simpsh");
        else {
            /* duplicate file to stdin and exit successfully */
            dup2 (file, STDIN_FILENO);
            close (file);
            return 0;
        }
    }
    /* If user is redirecting from stdout to file */
    else if (direction == OUTPUT) {
        /* Open file write only, with r/w permissions for user */
        if ((file = open (file_name, O_WRONLY|O_CREAT, 00400|00200)) < 0)
            perror ("simpsh");
        else {
            /* Duplicate stdout to file and exit successfully */
            dup2 (file, STDOUT_FILENO);
            close (file);
            return 0;
        }

    }
    /* Something went wrong if we made it here. */
    return -255;
}

/* Initializes the environment */
char **Initenv () {
    /* Allocate 4 string pointers to env (last one for null terminator) */
    char **env = malloc((ENVSIZE + 1) * sizeof(char *));

    /* Allocate space for 3 strings (SHELL, PATH, ?=)*/
    for (int i = 0; i < ENVSIZE; i++)
        env[i] = malloc(ENVFIELDSIZE * sizeof(char));

    /* Setting static parts of env */
    *env[0] = 0x0;
    *env[1] = 0x0;
    *env[2] = 0x0;
    *env[3] = 0x0;

    Append (env[0], "SHELL=\0");
    Append (env[1], "PATH=\0");
    Append (env[2], "TERM=\0");
    Append (env[3], "?=");

    /* Null terminating env string arr */
    env[4] = NULL;

    Append (env[0], SHELL);
    Append (env[1], PATH);
    Append (env[2], TERM);

    return env;
}

void Setenv (char *err_str, int err_int) {
    /* Making err_str (which is env[2]) empty */
    *err_str = 0x0;

    /* Making err_str contain "?=err_int" */
    snprintf(err_str, 6, "%s%d", "?=", err_int);
}

/* Locates the binary in the PATH, where bin is the binary to find */
void Getpath (char *bin) {
    /* Setting up path string array */
    int *placeholder = malloc (sizeof(int));
    *placeholder = 0;

    char **path = Strtok (PATH, 0x3a, placeholder); // Gets all the dirs in PATH (assumes PATH is formatted correctly)
    free(placeholder);

    char **curr_path = path;
    char *retval = malloc(PROMPTSIZE);

    *retval = 0x0;
    int check = 0;

    /* Setting up dirs */
    DIR *dir;                // File descriptor
    struct dirent *file;     // Directory

    /* While there is a path to check, a matching file already hasn't been found
     * and as directory can be opened. */
    while (*curr_path && !check && (dir = opendir (*curr_path))) {
        /* Ensure file can was opened */
        if (!dir) {
            /* if file was unable to be opened, print error, free, and return */
            perror("simpsh");
            free(path);
            free(retval);
            return;
        }
        /* Read all files in *curr_path */
        while (!check && (file = readdir (dir)) ) {
            /* Find a matching executable in dir */
            if ((check = Strcmp (bin, file->d_name)))
                break;
        }
        /* Close dir */
        closedir(dir);

        /* If and only if file has not been found, check the next dir in PATH */
        if (!check)
            curr_path++;
    }
    /* Only change anything if binary was found */
    if (check) {
        /* Prepend the found path to the binary */
        Append(retval, *curr_path);
        Append(retval, bin);

        /* Putting retval back in bin, since it needs to be there */
        *bin = 0x0;
        Append(bin, retval);

    }
    /* If binary wasn't found, print an error message */
    else {
        perror(bin);
        *bin = 0x0;
    }
    /* Freeing allocated memory */
    free(retval);
    free(path);
}


/* Identical to p0 solution */
void Append (char *s, char *t) {
    /* Find end of string s */
    while (*s)
        s++;
    /* add t to end of s */
    while (*t)
        *s++ = *t++;
    /* null terminate s */
    *s = 0x0;
}

/* Relatively simple string comparison function. Assumes both strings are null terminated */
int Strcmp (char *s, char *t) {
    /* Iterates over both strings until it finds the end of one */
    while (*s && *t) {
        /* Checking to ensure every character is equal */
        if (*s++ != *t++)
            return 0;
    }
    // if s or t still have characters, the string is not equal
    if (*s || *t)
        return 0;
    return 1;
}

/* ### Builtins ### */

/* Exits the shell if the user entered exit */
void exit_shell(int code) {
    exit(code);
}
