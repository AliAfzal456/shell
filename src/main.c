#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <fcntl.h>

#include "sfish.h"

#define KRED "\033[1;31m"
#define KGRN "\033[1;32m"
#define KYEL "\033[1;33m"
#define KBLU "\033[1;34m"
#define KMAG "\033[1;35m"
#define KCYN "\033[1;36m"
#define KWHT "\033[1;37m"
#define KBWN "\033[0;33m"
#define ANSI_COLOR_RESET   "\x1b[0m"


void removeSpaces(char *string){
  int i = 0;
  int x = 0;

  for(i=0, x=0; string[i]; ++i){
    if(!isspace(string[i]) || (i>0 && !isspace(string[i-1]))){
      string[x++] = string[i];
    }
  }
  string[x] = '\0';
}

void helpChild(sigset_t mask, sigset_t previous){
    sigprocmask(SIG_BLOCK, &mask, &previous);

    if (fork() == 0){
        printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                         "help             Prints out this menu\n",
                         "pwd              Prints the current working directory\n",
                         "exit             Exit the shell program");
        _exit(errno);
    }

    sigsuspend(&previous);
    // unblock sig child
    sigprocmask(SIG_SETMASK, &previous, NULL);

}

int countTimes(char *string, char character){
    int amount = 0;
    while (*string != '\0'){
        if (*string == character){
            amount +=1;
        }
        string+=1;
    }
    return amount;
}

char *arrayLoc;
volatile sig_atomic_t pid;

void sigchld_handler(int s){
    pid = waitpid(-1, NULL, 0);
}


void sigint_handler(int s){
    fflush(stdout);
    pid = waitpid(-1, NULL, 0);

    if (pid != -1){
    }
}

void sigpause_handler(int s){
    pid = waitpid(-1, NULL, 0);
    fflush(stdout);
}

int index_of(const char *string, char search) {
    const char *moved_string = strchr(string, search);
    /* If not null, return the difference. */
    if (moved_string) {
        return moved_string - string;
    }
    /* Character not found. */
    return -1;
}


char *trim(char *str)
{
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if( str == NULL ) { return NULL; }
    if( str[0] == '\0' ) { return str; }

    len = strlen(str);
    endp = str + len;

    while( isspace((unsigned char) *frontp) ) { ++frontp; }
    if( endp != frontp )
    {
        while( isspace((unsigned char) *(--endp)) && endp != frontp ) {}
    }

    if( str + len - 1 != endp )
            *(endp + 1) = '\0';
    else if( frontp != str &&  endp == frontp )
            *str = '\0';

    endp = str;
    if( frontp != str )
    {
            while( *frontp ) { *endp++ = *frontp++; }
            *endp = '\0';
    }


    return str;
}

int main(int argc, char *argv[], char* envp[]) {
    char* input;
    bool exited = false;

    if(!isatty(STDIN_FILENO)) {
        // If your shell is reading from a piped file
        // Don't have readline write anything to that file.
        // Such as the prompt or "user input"
        if((rl_outstream = fopen("/dev/null", "w")) == NULL){
            perror("Failed trying to open DEVNULL");
            exit(EXIT_FAILURE);
        }
    }


    char* home; // home path
    int homeLength; // home length
    char* currentDIR; // current directory. FREE THIS
    char* token;   // tokenized input

    // set previous directory to null in environment vars
    char* prev = NULL;

    sigset_t mask, previous;
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP,sigpause_handler);
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);


    char delimiters[] = " \t\r\n\v\f";
    char* color = ANSI_COLOR_RESET;

    do {
        currentDIR = get_current_dir_name();

        // set the current directory name
        home = getenv("HOME");
        homeLength = strlen(home);
        char printVal[sizeof(currentDIR)];
        arrayLoc = printVal;

        if (strcmp(home, currentDIR) == 0){
            strcpy(printVal, color);
            strcat(printVal, "~");

            strcat(printVal, " :: aliafzal >> ");
            strcat(printVal, ANSI_COLOR_RESET);
        }
        else if (homeLength > 0 && strncmp(home, currentDIR, homeLength) == 0 && strcmp(home, currentDIR) != 0){
            strcpy(printVal, color);
            strcpy(printVal + strlen(color) + 1, currentDIR + homeLength);
            printVal[strlen(color)] = '~';

            strcat(printVal, " :: aliafzal >> ");
            strcat(printVal, ANSI_COLOR_RESET);
        }
        else{
            strcpy(printVal, color);
            strcat(printVal, currentDIR);

            strcat(printVal, " :: aliafzal >> ");
            strcat(printVal, ANSI_COLOR_RESET);
        }

        // read the input
        input = readline(printVal);
        // remove extra white spaces
        removeSpaces(input);

        // tokenize the input based on the space character
        // but first check if its an empy line. just ignore and go to next then.
        if (strlen(input) == 0){
            rl_free(input);
            free(currentDIR);
            continue;
        }

        int numGreat = countTimes(input, '>');
        int numLess = countTimes(input, '<');


        int indexG = index_of(input, '>');
        int indexL = index_of(input, '<');



        int isG = 0;
        int isL = 0;

        if ((indexG != -1 && indexL == -1))
            isG = 1;

        else if ((indexG == -1 && indexL != -1))
            isL = 1;

        else if(indexG < indexL)
            isG = 1;



        else if (indexG > indexL)
            isL = 1;

        if (isG == 1){
            char **tokens = malloc(sizeof(char**));
            // store the program name as the first argument
            // split on >
            token = strtok(input, ">");

            tokens[0] = token;
            int counter = 1;

            // then read the rest of the tokens, realloc args, and add them
            while((token = strtok(NULL, ">")) != NULL){
                // realloc args
                tokens = realloc(tokens, (counter + 1) * sizeof(char*));
                tokens[counter] = token;
                counter += 1;
            }

            // now take the tokens, and check if there are more than 2 tokens
            if (numGreat > 1){
                printf(SYNTAX_ERROR, "Error in formatting redirection");
            }

            // check if token1 is empty string
            else if (counter == 1){
                printf(SYNTAX_ERROR, "no exe specified in here???");
            }

            // so the tokens are [PROG + ARGS, EVERYTHING ELSE]
            else if ((token = strpbrk(tokens[1], "<")) != NULL){
                // if it has a < in it, then its double redirection
                token = strtok(tokens[1], "<");
                tokens[counter] = token;
                counter += 1;

                while ((token = strtok(NULL, "<")) != NULL){
                    tokens = realloc(tokens,(counter+1) * sizeof(char*));
                    tokens[counter] = token;
                    counter +=1;
                }
                tokens[1]= NULL;
                for (int i = 0; i < counter; i ++){
                    // remove whitespace from each string
                    if (tokens[i] != NULL){
                        removeSpaces(tokens[i]);
                    }
                }

                // tokens are appearing correctly, very good
                if (numLess > 1){
                    printf(SYNTAX_ERROR, "Error in formatting redirection");
                }

                // so the token array is: [PROG + ARGS, NULL, FILE1, FILE2]
                // parse prog+args into an array of its own.
                char **args = malloc(sizeof(char**));
                // store the program name as the first argument
                token = strtok(tokens[0], delimiters);
                args[0] = token;
                int size = 1;

                // then read the rest of the tokens, realloc args, and add them
                while((token = strtok(NULL, delimiters)) != NULL){
                    // realloc args
                    args = realloc(args, (size + 1) * sizeof(char*));
                    args[size] = token;
                    size += 1;
                }
                // lastly, add a null pointer to the end of the array
                args = realloc(args, (size + 1)* sizeof(char*));
                args[size] = NULL;

                /**/

                /******
                    HERE: OPEN THE FILES AND ASSOCIATE A FILE DESCRIPTOR WITH THEM
                    THEN IN \: http://www.cs.loyola.edu/~jglenn/702/Sls2005/Examples/dup2.html
                    */
                if(tokens[2][strlen(tokens[2]) - 1] == 32){
                    tokens[2][strlen(tokens[2]) - 1] = 0;
                }

                // and now we can run the program specified in args (if it exists)
                // if it has a / in it then look through regular directories to run it
                if ((token = strpbrk(args[0], "/")) != NULL){
                    // if it has a / then run it (or try to run it)
                    struct stat buffer;
                    int found = stat(args[0], &buffer);

                    if (found == 0){
                        // executable exists
                        sigprocmask(SIG_BLOCK, &mask, &previous);

                        if (fork() == 0){
                            int stdout = dup(STDOUT_FILENO);
                            int out = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);

                            int in = open (tokens[3], O_RDONLY);
                            dup2(in, STDIN_FILENO);
                            close(in);

                            dup2(out, STDOUT_FILENO);
                            close(out);

                            int execution = execv(args[0], args);
                            if (execution == -1){
                                dup2(stdout, STDOUT_FILENO);
                                close(stdout);
                                // exec failed. print error
                                printf(EXEC_ERROR, args[0]);
                            }
                            _exit(errno);
                        }

                        sigsuspend(&previous);
                        // unblock sig child
                        sigprocmask(SIG_SETMASK, &previous, NULL);
                        // and now back in parent
                        free(args);
                    }
                    else{
                        printf(EXEC_NOT_FOUND, args[0]);
                    }
                }

                else{
                    // path var
                    sigprocmask(SIG_BLOCK, &mask, &previous);

                    if (strcmp(args[0], "help") == 0){
                        if (fork() == 0){
                            int out = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(out, STDOUT_FILENO);
                            close(out);
                            printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                                             "help             Prints out this menu\n",
                                             "pwd              Prints the current working directory\n",
                                             "exit             Exit the shell program");
                            _exit(errno);
                        }
                    }

                    else if (strcmp(args[0], "pwd") == 0){
                        if (fork() == 0){
                            int out = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2 (out, STDOUT_FILENO);
                            close(out);
                            printf("%s\n", get_current_dir_name());
                            _exit(errno);
                        }
                    }
                    else if (fork() == 0){
                        int execution;
                        int stdout;
                        stdout = dup(STDOUT_FILENO);
                        errno = 0;
                        int out = open(tokens[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);

                        int in = open (tokens[3], O_RDONLY);

                        dup2(in, STDIN_FILENO);
                        close(in);

                        dup2(out, STDOUT_FILENO);
                        close(out);


                        execution = execvp(args[0], args);
                        if (execution == -1){
                            // exec failed. print error
                            dup2(stdout, STDOUT_FILENO);
                            close(stdout);
                            printf(EXEC_ERROR, args[0]);
                        }
                        free(args);
                        _exit(errno);
                    }

                    sigsuspend(&previous);
                    // unblock sig child
                    sigprocmask(SIG_SETMASK, &previous, NULL);

                    // and now back in parent
                    free(args);
                }
            }

            // else its a single redirection, so implement it accordingly
            else{
                for (int i = 0; i < counter; i ++){
                    // remove whitespace from each string
                    if (tokens[i] != NULL){
                        removeSpaces(tokens[i]);
                    }
                }

                // parse prog+args into an array of its own.
                char **args = malloc(sizeof(char**));
                // store the program name as the first argument
                token = strtok(tokens[0], delimiters);
                args[0] = token;
                int size = 1;

                // then read the rest of the tokens, realloc args, and add them
                while((token = strtok(NULL, delimiters)) != NULL){
                    // realloc args
                    args = realloc(args, (size + 1) * sizeof(char*));
                    args[size] = token;
                    size += 1;
                }
                // lastly, add a null pointer to the end of the array
                args = realloc(args, (size + 1)* sizeof(char*));
                args[size] = NULL;


                if(tokens[1][strlen(tokens[1]) - 1] == 32){
                    tokens[1][strlen(tokens[1]) - 1] = 0;
                }


                // and now we can run the program specified in args (if it exists)
                // if it has a / in it then look through regular directories to run it
                if ((token = strpbrk(args[0], "/")) != NULL){
                    // if it has a / then run it (or try to run it)
                    struct stat buffer;
                    int found = stat(args[0], &buffer);

                    if (found == 0){
                        // executable exists
                        sigprocmask(SIG_BLOCK, &mask, &previous);

                        if (fork() == 0){
                            int stdout = dup(STDOUT_FILENO);
                            int out = open(tokens[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(out, STDOUT_FILENO);
                            close(out);

                            int execution = execv(args[0], args);
                            if (execution == -1){
                                dup2(stdout, STDOUT_FILENO);
                                close(stdout);
                                // exec failed. print error
                                printf(EXEC_ERROR, args[0]);
                            }
                            _exit(errno);
                        }

                        sigsuspend(&previous);
                        // unblock sig child
                        sigprocmask(SIG_SETMASK, &previous, NULL);
                        // and now back in parent
                        free(args);
                    }
                    else{
                        printf(EXEC_NOT_FOUND, args[0]);
                    }
                }

                else{
                    // path var
                    sigprocmask(SIG_BLOCK, &mask, &previous);

                    if (strcmp(args[0], "help") == 0){
                        if (fork() == 0){
                            int out = open(tokens[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(out, STDOUT_FILENO);
                            close(out);
                            printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                                             "help             Prints out this menu\n",
                                             "pwd              Prints the current working directory\n",
                                             "exit             Exit the shell program");
                            _exit(errno);
                        }
                    }

                    else if (strcmp(args[0], "pwd") == 0){
                        if (fork() == 0){
                            int out = open(tokens[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2 (out, STDOUT_FILENO);
                            close(out);
                            printf("%s\n", get_current_dir_name());
                            _exit(errno);
                        }
                    }
                    else if (fork() == 0){
                        int execution;
                        int stdout;
                        stdout = dup(STDOUT_FILENO);
                        errno = 0;
                        int out = open(tokens[1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        dup2(out, STDOUT_FILENO);
                        close(out);

                        execution = execvp(args[0], args);
                        if (execution == -1){
                            // exec failed. print error
                            dup2(stdout, STDOUT_FILENO);
                            close(stdout);
                            printf(EXEC_ERROR, args[0]);
                        }
                        free(args);
                        _exit(errno);
                    }

                    sigsuspend(&previous);
                    // unblock sig child
                    sigprocmask(SIG_SETMASK, &previous, NULL);

                    // and now back in parent
                    free(args);
                }

            }
        free(tokens);
        }













        else if (isL == 1){
            char **tokens = malloc(sizeof(char**));
            // store the program name as the first argument
            // split on <
            token = strtok(input, "<");

            tokens[0] = token;
            int counter = 1;

            // then read the rest of the tokens, realloc args, and add them
            while((token = strtok(NULL, "<")) != NULL){
                // realloc args
                tokens = realloc(tokens, (counter + 1) * sizeof(char*));
                tokens[counter] = token;
                counter += 1;
            }

            // now take the tokens, and check if there are more than 2 tokens
            if (numLess > 1){
                printf(SYNTAX_ERROR, "Error in formatting redirection");
            }

            // check if token1 is empty string
            else if (counter == 1){
                printf(SYNTAX_ERROR, "no exe specified");
            }

            // so the tokens are [PROG + ARGS, EVERYTHING ELSE]
            else if ((token = strpbrk(tokens[1], ">")) != NULL){
                // if it has a < in it, then its double redirection
                token = strtok(tokens[1], ">");
                tokens[counter] = token;
                counter += 1;

                while ((token = strtok(NULL, ">")) != NULL){
                    tokens = realloc(tokens,(counter+1) * sizeof(char*));
                    tokens[counter] = token;
                    counter +=1;
                }
                tokens[1]= NULL;
                for (int i = 0; i < counter; i ++){
                    // remove whitespace from each string
                    if (tokens[i] != NULL){
                        removeSpaces(tokens[i]);\
                    }
                }

                // tokens are appearing correctly, very good
                if (numGreat > 1){
                    printf(SYNTAX_ERROR, "Error in formatting redirection");
                }

                // so the token array is: [PROG + ARGS, NULL, FILE1, FILE2]
                // parse prog+args into an array of its own.
                char **args = malloc(sizeof(char**));
                // store the program name as the first argument
                token = strtok(tokens[0], delimiters);
                args[0] = token;
                int size = 1;

                // then read the rest of the tokens, realloc args, and add them
                while((token = strtok(NULL, delimiters)) != NULL){
                    // realloc args
                    args = realloc(args, (size + 1) * sizeof(char*));
                    args[size] = token;
                    size += 1;
                }
                // lastly, add a null pointer to the end of the array
                args = realloc(args, (size + 1)* sizeof(char*));
                args[size] = NULL;

                /**/

                /******
                    */
                if(tokens[2][strlen(tokens[2]) - 1] == 32){
                    tokens[2][strlen(tokens[2]) - 1] = 0;
                }

                // and now we can run the program specified in args (if it exists)
                // if it has a / in it then look through regular directories to run it
                if ((token = strpbrk(args[0], "/")) != NULL){
                    // if it has a / then run it (or try to run it)
                    struct stat buffer;
                    int found = stat(args[0], &buffer);

                    if (found == 0){
                        // executable exists
                        sigprocmask(SIG_BLOCK, &mask, &previous);

                        if (fork() == 0){
                            int stdout = dup(STDOUT_FILENO);
                            int out = open(tokens[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);

                            int in = open (tokens[2], O_RDONLY);
                            dup2(in, STDIN_FILENO);
                            close(in);

                            dup2(out, STDOUT_FILENO);
                            close(out);

                            int execution = execv(args[0], args);
                            if (execution == -1){
                                dup2(stdout, STDOUT_FILENO);
                                close(stdout);
                                // exec failed. print error
                                printf(EXEC_ERROR, args[0]);
                            }
                            _exit(errno);
                        }

                        sigsuspend(&previous);
                        // unblock sig child
                        sigprocmask(SIG_SETMASK, &previous, NULL);
                        // and now back in parent
                        free(args);
                    }
                    else{
                        printf(EXEC_NOT_FOUND, args[0]);
                    }
                }

                else{
                    // path var
                    sigprocmask(SIG_BLOCK, &mask, &previous);

                    if (strcmp(args[0], "help") == 0){
                        if (fork() == 0){
                            int out = open(tokens[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2(out, STDOUT_FILENO);
                            close(out);
                            printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                                             "help             Prints out this menu\n",
                                             "pwd              Prints the current working directory\n",
                                             "exit             Exit the shell program");
                            _exit(errno);
                        }
                    }

                    else if (strcmp(args[0], "pwd") == 0){
                        if (fork() == 0){
                            int out = open(tokens[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                            dup2 (out, STDOUT_FILENO);
                            close(out);
                            printf("%s\n", get_current_dir_name());
                            _exit(errno);
                        }
                    }
                    else if (fork() == 0){
                        int execution;
                        int stdout;
                        stdout = dup(STDOUT_FILENO);
                        errno = 0;
                        int out = open(tokens[3], O_WRONLY | O_CREAT | O_TRUNC, 0666);

                        int in = open (tokens[2], O_RDONLY);

                        dup2(in, STDIN_FILENO);
                        close(in);

                        dup2(out, STDOUT_FILENO);
                        close(out);


                        execution = execvp(args[0], args);
                        if (execution == -1){
                            // exec failed. print error
                            dup2(stdout, STDOUT_FILENO);
                            close(stdout);
                            printf(EXEC_ERROR, args[0]);
                        }
                        free(args);
                        _exit(errno);
                    }

                    sigsuspend(&previous);
                    // unblock sig child
                    sigprocmask(SIG_SETMASK, &previous, NULL);

                    // and now back in parent
                    free(args);
                }
            }

            // else its a single redirection, so implement it accordingly
            else{
                for (int i = 0; i < counter; i ++){
                    // remove whitespace from each string
                    if (tokens[i] != NULL){
                        removeSpaces(tokens[i]);
                    }
                }

                // parse prog+args into an array of its own.
                char **args = malloc(sizeof(char**));
                // store the program name as the first argument
                token = strtok(tokens[0], delimiters);
                args[0] = token;
                int size = 1;

                // then read the rest of the tokens, realloc args, and add them
                while((token = strtok(NULL, delimiters)) != NULL){
                    // realloc args
                    args = realloc(args, (size + 1) * sizeof(char*));
                    args[size] = token;
                    size += 1;
                }
                // lastly, add a null pointer to the end of the array
                args = realloc(args, (size + 1)* sizeof(char*));
                args[size] = NULL;


                if(tokens[1][strlen(tokens[1]) - 1] == 32){
                    tokens[1][strlen(tokens[1]) - 1] = 0;
                }

                // and now we can run the program specified in args (if it exists)
                // if it has a / in it then look through regular directories to run it
                if ((token = strpbrk(args[0], "/")) != NULL){
                    // if it has a / then run it (or try to run it)
                    struct stat buffer;
                    int found = stat(args[0], &buffer);

                    if (found == 0){
                        // executable exists
                        sigprocmask(SIG_BLOCK, &mask, &previous);

                        if (fork() == 0){
                            int stdout = dup(STDIN_FILENO);
                            int out = open(tokens[1], O_RDONLY, 0666);
                            dup2(out, STDIN_FILENO);
                            close(out);

                            int execution = execv(args[0], args);
                            if (execution == -1){
                                dup2(stdout, STDIN_FILENO);
                                close(stdout);
                                // exec failed. print error
                                printf(EXEC_ERROR, args[0]);
                            }
                            _exit(errno);
                        }

                        sigsuspend(&previous);
                        // unblock sig child
                        sigprocmask(SIG_SETMASK, &previous, NULL);
                        // and now back in parent
                        free(args);
                    }
                    else{
                        printf(EXEC_NOT_FOUND, args[0]);
                    }
                }

                else{
                    // path var
                    sigprocmask(SIG_BLOCK, &mask, &previous);

                    if (strcmp(args[0], "help") == 0){
                        if (fork() == 0){
                            int out = open(tokens[1], O_RDONLY, 0666);
                            dup2(out, STDIN_FILENO);
                            close(out);
                            printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                                             "help             Prints out this menu\n",
                                             "pwd              Prints the current working directory\n",
                                             "exit             Exit the shell program");
                            _exit(errno);
                        }
                    }

                    else if (strcmp(args[0], "pwd") == 0){
                        if (fork() == 0){
                            int out = open(tokens[1], O_RDONLY, 0666);
                            dup2 (out, STDIN_FILENO);
                            close(out);
                            printf("%s\n", get_current_dir_name());
                            _exit(errno);
                        }
                    }
                    else if (fork() == 0){
                        int execution;
                        int stdout;
                        stdout = dup(STDIN_FILENO);
                        errno = 0;
                        int out = open(tokens[1], O_RDONLY, 0666);
                        dup2(out, STDIN_FILENO);
                        close(out);

                        execution = execvp(args[0], args);
                        if (execution == -1){
                            // exec failed. print error
                            dup2(stdout, STDIN_FILENO);
                            close(stdout);
                            printf(EXEC_ERROR, args[0]);
                        }
                        free(args);
                        _exit(errno);
                    }

                    sigsuspend(&previous);
                    // unblock sig child
                    sigprocmask(SIG_SETMASK, &previous, NULL);

                    // and now back in parent
                    free(args);
                }

            }
            free(tokens);
        }












        else if (strpbrk(input, "|") != NULL){
            // format is PROG | PROG | PROG | ... | PROG
            int numPipes = 0;
            int i;

            // We are computing the length once at this point
            // because it is a relatively lengthy operation,
            // and we don't want to have to compute it anew
            // every time the i < length condition is checked.
            int length = strlen(input);

            for (i = 0; i < length; i++)
            {
                if (input[i] == '|')
                {
                    numPipes++;
                }
            }


            // split it on pipes and store the tokens in an array
            char **tokens = malloc(sizeof(char**));
            // store the program name as the first argument
            // split on >
            token = strtok(input, "|");
            token = trim(token);

            tokens[0] = token;
            int counter = 1;



            // then read the rest of the tokens, realloc args, and add them
            while((token = strtok(NULL, "|")) != NULL){
                // realloc args
                tokens = realloc(tokens, (counter + 1) * sizeof(char*));
                tokens[counter] = token;
                counter += 1;
                token = trim(token);

            }

            // for (int i = 0; i < counter; i++)
            //      printf("__%s__\n", tokens[i]);
            if (counter  != (numPipes + 1))
                printf(SYNTAX_ERROR, "error in formatting");

            // and now the prog + args are in each of the token slots
            // create arrays for each of the arg
            // need 1 array that points to the start of the arguments
            // and 1 array that holds all the arguments

            else{
                char **allToks = malloc(sizeof(char**));
                char **commands = malloc(sizeof(char**));

                int index = 0;
                // do a for loop for all elements
                int execError = 0;
                for(int i = 0; i < counter; i++){
                    // for each element, split it and append it to allToks array


                    token = strtok(tokens[i], delimiters);
                    allToks[index] = token;
                    commands[index] = token;
                    index++;

                    // then read the rest of the tokens, realloc args, and add them
                    while((token = strtok(NULL, delimiters)) != NULL){
                        // realloc args
                        allToks = realloc(allToks, (index + 2) * sizeof(char*));
                        allToks[index] = token;
                        index++;
                    }
                    // lastly, add a null pointer to the end of the array
                    allToks = realloc(allToks, (index + 2)* sizeof(char*));
                    commands = realloc(commands, (index + 2)* sizeof(char*));
                    allToks[index] = NULL;
                    index ++;
                }

                commands[0] = allToks[0];
                int roll = 1;
                for (int i = 0; i < index; i++){
                    if (allToks[i] == NULL){
                        commands[roll] = allToks[i+1];
                        roll++;
                    }
                }

                int   p[2];
                pid_t pid;
                int   fd_in = 0;
                int eleNo  = 0;
                for (int i = 0; i < counter; i++)
                  {
                    pipe(p);
                    sigprocmask(SIG_BLOCK, &mask, &previous);
                    if ((pid = fork()) == 0)
                      {
                        dup2(fd_in, 0);
                        if (commands[i + 1] != NULL)
                          dup2(p[1], 1);
                        close(p[0]);

                        if (strcmp(commands[i], "help") == 0){
                            printf("%s%s%s%s\n", "cd [dir]         Change current directory to argument directory if it exists\n",
                                             "help             Prints out this menu\n",
                                             "pwd              Prints the current working directory\n",
                                             "exit             Exit the shell program");
                            exit(0);
                        }

                        else if (strcmp(commands[i], "pwd") == 0){
                            printf("%s\n", currentDIR);
                            exit(0);
                        }


                        else if (strstr(commands[i], "/") != NULL){
                            struct stat buffer;
                            int found = stat(commands[i], &buffer);

                            if (found == 0){
                                execError = execv(commands[i], allToks + eleNo);
                                exit(execError);
                            }
                            else{
                                exit(1);
                            }
                        }

                        else{
                            execError = execvp(commands[i], allToks + eleNo);
                            exit(execError);
                        }
                      }
                    else{
                        int childval=-1;
                        wait(&childval);//catching signal sent by exit of(child)

                        // unblock sig child
                        sigprocmask(SIG_SETMASK, &previous, NULL);
                        close(p[1]);
                        fd_in = p[0]; //save the input for the next command
                        while (allToks[eleNo] != NULL){
                            eleNo += 1;
                        }
                        eleNo += 1;
                        if ((childval/255)-255 == 1){
                            printf(EXEC_ERROR, "execution error"); //changing signal to exact value
                            break;
                        }
                        if ((childval/255) - 255 == -254){
                            printf(EXEC_NOT_FOUND, "EXEC not found");
                            break;
                        }
                      }
                  }
            free(tokens);
            free(allToks);
            }
        }










        else{
            token = strtok(input, delimiters);

            if (strcmp(token, "help") == 0){
                helpChild(mask, previous);
            }

            else if (strcmp(token, "cd") == 0){
                // check if there is another argument.
                token = strtok(NULL, delimiters);

                // check if the token exists
                if (token != NULL){
                    // so there is a second argument. Check what it is
                    if (strcmp(token, ".") == 0){
                        // if its a . then you need to go to the parent directory
                        if (prev == NULL){
                            prev = get_current_dir_name();
                        }
                        setenv("PWD", prev, 1);

                        // set our old directory to our current directory
                        setenv("OLPWD", currentDIR, 1);
                        // update the value of the prev variable to where we are now
                        free(prev);
                        prev = get_current_dir_name();

                        // change directory
                        int chdirreturn = chdir(".");;

                        if (chdirreturn != 0){
                            printf(BUILTIN_ERROR, "An error ocurred trying to get the current directory");
                        }
                    }

                    else if (strcmp(token, "..") == 0){
                        // if its .. then go to parent of parent
                        // set our current directory the previous directory
                        if (prev == NULL){
                            prev = get_current_dir_name();
                        }
                        setenv("PWD", prev, 1);

                        // set our old directory to our current directory
                        setenv("OLPWD", currentDIR, 1);
                        // update the value of the prev variable to where we are now
                        free(prev);
                        prev = get_current_dir_name();

                        // change directory
                        int chdirreturn = chdir("..");;

                        if (chdirreturn != 0){
                            printf(BUILTIN_ERROR, "An error ocurred trying to go to parent directory");
                        }
                    }

                    else if (strcmp(token, "-") == 0){
                        // if its - then go to previous directory
                        if (prev == NULL){
                            printf(BUILTIN_ERROR, "OLDPWD not set");
                        }
                        else{
                            // set our current directory the previous directory
                            if (prev == NULL){
                                prev = get_current_dir_name();
                            }
                            setenv("PWD", prev, 1);

                            // set our old directory to our current directory
                            setenv("OLPWD", currentDIR, 1);
                            // update the value of the prev variable to where we are now
                            free(prev);
                            prev = get_current_dir_name();

                            // change directory
                            int chreturn = chdir(getenv("PWD"));

                            if (chreturn != 0){
                                printf(BUILTIN_ERROR, "An error occured trying to go to the previous directory");
                            }
                        }
                    }

                    else{
                        // else its a user passed directory. go to it if you can, otherwise print an error.


                        // change directory
                        int chreturn = chdir(token);

                        if (chreturn != 0){
                            printf(BUILTIN_ERROR, "Directory does not exist");
                        }

                        else{
                            // directory doesnt exist, so previous should remain the same should return to same directory
                            chdir(currentDIR);

                            if (prev == NULL){
                                prev = get_current_dir_name();
                            }
                            setenv("PWD", prev, 1);

                            // set our old directory to our current directory
                            setenv("OLPWD", currentDIR, 1);
                            // update the value of the prev variable to where we are now
                            free(prev);
                            prev = get_current_dir_name();

                            chdir(token);
                        }
                    }

                }

                // else if it doesnt exist, cd back to home
                else{
                    setenv("PWD", home, 1);             // our current directory changes to the home directory
                    setenv("OLDPWD", currentDIR, 1);    // previous directory is the one we are currently at

                    if (prev == NULL){
                        prev = get_current_dir_name();
                    }

                    else{
                        free(prev);
                        prev = get_current_dir_name();
                    }

                    int chdirreturn = chdir(home);

                    if (chdirreturn != 0){
                        printf(BUILTIN_ERROR, "An error ocurred trying to switch to the home directory");
                    }
                }
            }

            else if (strcmp(token, "pwd") == 0){
                printf("%s\n", currentDIR);
            }

            else if (strcmp(token, "exit") == 0){
                rl_free(input);
                free(currentDIR);
                exit(3);
            }

            else if (strcmp(token, "color") == 0){
                // read the second argument, which gives the color
                token = strtok(NULL, delimiters);
                if (token != NULL){
                    if(strcmp(token, "RED") == 0){
                        color = KRED;
                    }
                    if(strcmp(token, "GRN") == 0){
                        color = KGRN;
                    }
                    if(strcmp(token, "YEL") == 0){
                        color = KYEL;
                    }
                    if(strcmp(token, "BLU") == 0){
                        color = KBLU;
                    }
                    if(strcmp(token, "MAG") == 0){
                        color = KMAG;
                    }
                    if(strcmp(token, "CYN") == 0){
                        color = KCYN;
                    }
                    if(strcmp(token, "WHT") == 0){
                        color = KWHT;
                    }
                    if(strcmp(token, "BWN") == 0){
                        color = KBWN;
                    }
                }
            }

            else if (strcmp(token, "kill") == 0){
                token = strtok(NULL, delimiters);

                if (token != NULL){
                    // second arg is the kill process num
                    if (token[0] == '%'){
                        // if its %kill, then kill process num
                        kill(111, SIGKILL);
                    }

                    else{
                        kill(atoi(token), SIGKILL);
                    }
                }
            }

            else if (strcmp(token, "jobs") == 0){
                for (int i = 0; i < 5; i++){
                    printf(JOBS_LIST_ITEM, i, "exec");
                }
            }

            else if (strcmp(token, "fg") == 0){
                token = strtok(NULL, delimiters);

                if (token != NULL){
                    // second arg is the kill process num
                    if (token[0] == '%'){
                        // if its %kill, then kill process num
                        kill(atoi(strtok(NULL, "%")), SIGCONT);
                    }
                }
            }


            // else its some kind of executable.
            // if the token has a '/' in it, then look for the directory
            else if (strchr(token, '/') != NULL){
                struct stat buffer;
                int found = stat(token, &buffer);

                if (found == 0){
                    // so the executable exists in this directory.
                    // get its arguments and store them in a char array
                    // use a dynamic char pointer to do it
                    // currently, we can hold 1 argument
                    char **args = malloc(sizeof(char**));
                    // store the program name as the first argument
                    args[0] = token;
                    int counter = 1;

                    // then read the rest of the tokens, realloc args, and add them
                    while((token = strtok(NULL, delimiters)) != NULL){
                        // realloc args
                        args = realloc(args, (counter + 1) * sizeof(char*));
                        args[counter] = token;
                        counter += 1;
                    }
                    // lastly, add a null pointer to the end of the array
                    args = realloc(args, (counter + 1)* sizeof(char*));
                    args[counter] = NULL;

                    sigprocmask(SIG_BLOCK, &mask, &previous);
                    pid_t pid;
                    if ((pid = fork()) == 0){
                        int execution = execv(args[0], args);

                        if (execution == -1){
                            // exec failed. print error
                            printf(EXEC_ERROR, args[0]);
                        }
                        free(args);
                        _exit(errno);
                    }

                    sigsuspend(&previous);
                    // unblock sig child
                    sigprocmask(SIG_SETMASK, &previous, NULL);

                    // and now back in parent
                    free(args);
                }

                else{
                    // else the executable with the / in it doesnt exist, so print out exec not found error
                    printf(EXEC_NOT_FOUND, token);
                }
            }

            // If EOF is read (aka ^D) readline returns NULL
            else if(token == NULL){
                continue;
            }


            // else its an executable that is in the path. look around for it, and if not exist, print out exec not found
            else{


                char **args = malloc(sizeof(char**));
                // store the program name as the first argument
                args[0] = token;
                int counter = 1;

                // then read the rest of the tokens, realloc args, and add them
                while((token = strtok(NULL, delimiters)) != NULL){
                    // realloc args
                    args = realloc(args, (counter + 1) * sizeof(char*));
                    args[counter] = token;
                    counter += 1;
                }
                // lastly, add a null pointer to the end of the array
                args = realloc(args, (counter + 1)* sizeof(char*));
                args[counter] = NULL;

                sigprocmask(SIG_BLOCK, &mask, &previous);

                if ((pid = fork()) == 0){
                    int execution = execvp(args[0], args);

                    // if this execution fails, its not an execution error. It just means that the command doesn't exist
                    if (execution == -1){
                        // exec failed. print error
                        printf(EXEC_NOT_FOUND, args[0]);
                    }
                    free(args);
                    _exit(errno);
                }
                sigsuspend(&previous);

                // unblock sig child
                sigprocmask(SIG_SETMASK, &previous, NULL);
                // and now back in parent
                free(args);

                //printf(EXEC_NOT_FOUND, input);
            }
        }
        // Readline mallocs the space for input. You must free it.
        rl_free(input);
        free(currentDIR);

    } while(!exited);


    return EXIT_SUCCESS;
}
