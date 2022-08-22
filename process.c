#include <process.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <parse.h>
#include <fcntl.h>
int organize(const CMD *cmdList, int back);
void zombies(int x);
void environment(const CMD *cmdList);
int stat(int status);
void background(const CMD *cmdList, int x);
int pipes(const CMD *cmdList, int back);
int simples(const CMD *cmdList, int back);
void redirection(const CMD *cmdList);

int process(const CMD *cmdList)
{
    signal(SIGCHLD, zombies);
    organize(cmdList, 0);
    return 0;
}

void redirection(const CMD *cmdList)
{
    if(cmdList->fromType == RED_IN_HERE){
            char temp[11] = "tempXXXXXX";
            int temp_fd = mkstemp(temp);
            //printf("temp-fd: %d", temp_fd);
            write(temp_fd, cmdList->fromFile, strlen(cmdList->fromFile));
            close(temp_fd);
            int new_stdin_fd = open(temp, O_RDONLY);
            //printf("stdin fd: %d and name of file: %s\n", new_stdin_fd, cmdList->fromFile);
            dup2(new_stdin_fd, STDIN_FILENO); //overwrite stdin
            //printf("%ddasda", x);
            close(new_stdin_fd);

        }
    if(cmdList->fromType == RED_IN){ //redirection in <
        int new_stdin_fd = open(cmdList->fromFile, O_RDONLY);
        //printf("stdin fd: %d and name of file: %s\n", new_stdin_fd, cmdList->fromFile);
        dup2(new_stdin_fd, STDIN_FILENO); //overwrite stdin
        //printf("%ddasda", x);
        close(new_stdin_fd);  
    }//redirection in
    if(cmdList->toType == RED_OUT){ //redirection out >
        //printf("cmd file: %s\n", cmdList->toFile);
        int new_stdout_fd = open(cmdList->toFile, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        //printf("int for stdout: %d\n", new_stdout_fd);
        dup2(new_stdout_fd, STDOUT_FILENO); //overwrites stdout
        close(new_stdout_fd);
        
    }
    if(cmdList->toType == RED_OUT_APP){ //redirection out >>
        int new_stdout_fd = open(cmdList->toFile, O_CREAT | O_WRONLY | O_APPEND, S_IRWXU);
        //printf("int for stdout: %d\n", new_stdout_fd);
        dup2(new_stdout_fd, STDOUT_FILENO); //overwrites stdout
        close(new_stdout_fd);
    }
}

int simples(const CMD *cmdList, int back)
{
    if (strcmp(cmdList->argv[0], "cd") == 0 && !back) {
        char *name;
        environment(cmdList);
        redirection(cmdList);
        if (cmdList->argc == 2) {
            name = cmdList->argv[1];
        } 
        else if (cmdList->argc == 1) {
            name = getenv("HOME");
            
        } 
        else {
            fprintf(stderr, "error cd\n");
            return stat(1);
        }

        if (chdir(name) != -1) {
            return stat(0);
        } 
        else{
            perror("cd");
            return stat(errno);
        }
    } 
    else if (strcmp(cmdList->argv[0], "dirs") == 0 && !back) {
        if (cmdList->argc != 1) {
            fprintf(stderr, "error dirs\n");
            return stat(1);
        }
        char *name = malloc(sizeof(char) * PATH_MAX);
        if (getcwd(name, PATH_MAX) != NULL) {
            printf("%s\n", name);
            free(name);
            return stat(0);
        } 
        else {
            perror("dirs");
            free(name);
            return stat(errno);
        }
    } 
    else if (strcmp(cmdList->argv[0], "wait") == 0 && !back) {
        if (cmdList->argc != 1) {
            fprintf(stderr, "error wait\n");
            return stat(1);
        }
        
        while (waitpid((pid_t)(-1), NULL, WNOHANG) > 0);
        return stat(0);

    } 
    else {
        pid_t p = fork();
        int status;
        if (p < 0) {       
            perror(cmdList->argv[0]);
            return stat(errno);
        }
        else if (p == 0) {   
            environment(cmdList);
            redirection(cmdList);
            if (strcmp(cmdList->argv[0], "cd") == 0) {
                char *name;
                if (cmdList->argc == 2){
                    name = cmdList->argv[1];
                }
                    
                else if (cmdList->argc == 1){
                    name = getenv("HOME");
                }
                else {
                    fprintf(stderr, "error dirs\n");
                    exit(1);
                }
                if (chdir(name) != -1){
                    //printf("name: %s\n", name);
                    exit(0);
                }
                else{
                    perror("cd");
                    exit(errno);
                }                    
            } 
            else if (strcmp(cmdList->argv[0], "dirs") == 0) {
                if (cmdList->argc != 1) {
                    fprintf(stderr, "error dirs\n");
                    exit(1);
                }
                char *name = malloc(sizeof(char) * PATH_MAX); //initialize name
                if (getcwd(name, PATH_MAX) != NULL) {
                    //printf("name: %s\n", name);
                    printf("%s\n", name);
                    free(name);
                    exit(0);
                } 
                else {
                    //printf("name: %s\n", name);
                    free(name);
                    perror("dirs");
                    exit(errno);    
                }
            } else {
                execvp(cmdList->argv[0], cmdList->argv);
                perror(cmdList->argv[0]);
                exit(errno);
            }
        } 
        else {              
            if (!back) {
                //printf("p:%d", p);
                waitpid(p, &status, 0);
                status = STATUS(status);
            } 
            else {
                status = 0;
                fprintf(stderr, "Backgrounded: %d\n", p);
                
            }
        }
        return stat(status);
    }
}

int pipes(const CMD *cmdList, int back)
{
    int fd[2], status, inP, ents;
    ents = 1;        
    inP = 0;  
    pid_t p;              
    const CMD *po = cmdList;
    while(po->type == PIPE){
        po = po->left;
        ents++;
    }
    ents--;
    pid_t totalEnt[ents+1]; //table of entries       
    const CMD *commands[ents+1]; //create const
    int count = ents;   
    const CMD *point;     
    for(point = cmdList; point->type == PIPE; point = point->left) {
        commands[count] = point->right;
        count--;
    }
    commands[count] = point;
    ents++;
    for(int i = 0; i <= ents-2; i++) {   
        int pipefd = pipe(fd);
        p = fork();  
        if (pipefd || p < 0) {      
            perror("pipe");//pass pipe error
            return stat(errno);
        }
        else if (p == 0) { // if child   
            close(fd[0]);          
            if (inP != 0) {        
                dup2(inP, 0);
                close(inP);
            }
            if (fd[1] != 1) {      
                dup2 (fd[1], 1);
                close (fd[1]); //close write side
            }
            redirection(commands[i]);                

            if (commands[i]->type != SIMPLE) {
               exit(organize(commands[i]->left, back));
            } 
            else {                                 
                execvp(commands[i]->argv[0], commands[i]->argv);
            }
        } 
        else {
            totalEnt[i] = p;//add child into totalEnt
            if (i > 0){           
                close(inP); //close read side
                //printf("close: %d", inP);
            }
            inP = fd[0];
            close(fd[1]); //close write side
        }
    }

    if ((p = fork()) < 0) {     
        perror("pipe");           
        return stat(errno);
    }

    else if (p == 0) {  //if child
        if (inP != 0) {  //set to read pipe
            dup2(inP, 0);
            close(inP);
        }
        redirection(commands[ents-1]);

        if (commands[ents-1]->type != SIMPLE) {
            int error = organize(commands[ents-1]->left, back);
            exit(error);
        } 
        else {            
            execvp(commands[ents-1]->argv[0], commands[ents-1]->argv);
        }
    } 
    else {         
        totalEnt[ents-1] = p;    
        close(inP);//close read pipe
    }

    int finalStatus = 0;
    for (int i = 0; i < ents; ) {   // wait for children to die
        int j = 0;
        p = waitpid((pid_t)(-1), &status, WNOHANG);
        while (j < ents && totalEnt[j] != p){
            j++; //count j that are not child pids
        }
        if (j < ents) {   
            if (status != 0){ // child failed
                finalStatus = status;
            }  
            i++;
        }
    }
    const int f = STATUS(finalStatus);
    return stat(f);
}

int organize(const CMD *cmdList, int back)
{
    if (cmdList->type == SIMPLE) {
        return simples(cmdList, back);
    } 
    else if (cmdList->type == PIPE) {
        return pipes(cmdList, back);
    } 
    else if (cmdList->type == SEP_BG) {
        background(cmdList, 1);
        return 0;
    } 
    else if (cmdList->type == SUBCMD) {
        pid_t p = fork();
        int status;
        if (p < 0) {//fork check
            perror("subcommand");
            return stat(errno);
        }
        else if (p == 0) {//child
            redirection(cmdList);
            //printf("Here");
            exit(organize(cmdList->left, 0));//exit with error number 
        } 
        else {    
            if (back) {
                fprintf(stderr, "Backgrounded: %d\n", p);//backgrounded error
                status = 0;
            } 
            else {
                signal(SIGCHLD, zombies);//reap zombies
                waitpid(p, &status, 0); 
                status = STATUS(status);//status macro
            }
    }
    return stat(status);
    } 
    else if (cmdList->type == SEP_OR) {
        int status;
        if (back) {
            pid_t p = fork();

            if (p < 0) { // fork error
                perror(cmdList->argv[0]);
                return stat(errno);
            }
            else if (p == 0) { //if child
                if (organize(cmdList->left, 0) != 0)
                    organize(cmdList->right, 0);
                exit(0);
            } 
            else {                                    
                fprintf(stderr, "Backgrounded: %d\n", p);
                status = 0;
            }

            return stat(status);
        } 
        else {
            if(organize(cmdList->left, back) == 0){
                return 0;
            }
            else{
                return organize(cmdList->right, back);
            }
        }
    } 
    else if (cmdList->type == SEP_AND) {
        int status; //initial variable for status
        if (back) {
            pid_t p = fork();
            if (p < 0) {        
                perror(cmdList->argv[0]);
                return stat(errno);
            }
            else if (p == 0) { //child
                int org = organize(cmdList->left, 0);
                if (org == 0){//recursive run
                    organize(cmdList->right, 0);
                }
                exit(0);//shouldnt reach?
            } 
            else {                
                fprintf(stderr, "Backgrounded: %d\n", p);//standard error for backgrounded
                status = 0;
            }
            return stat(status); //return status
        } 
        else {
            if ((status = organize(cmdList->left, back)) == 0){ //if status is still 0 recurse or else return
                return organize(cmdList->right, back);
            }
            else{
                return status;
            }
        }
    } 
    else if (cmdList->type == SEP_END) {
        if (cmdList->right != NULL) {
            organize(cmdList->left, 0);
            return organize(cmdList->right, back);
        } else {
            return organize(cmdList->left, 0);
        }
    }
    return 0;
}

void background(const CMD *cmdList, int x)
{
    if (x) {
        if (cmdList->left->type != SEP_BG){
            organize(cmdList->left, 1);
            if (cmdList->right != NULL){
                organize(cmdList->right, 0);
            }
        }
        else {
            //recursive into backgrounded items
            background(cmdList->left, 0);
            if (cmdList->right != NULL){
                organize(cmdList->right, 0);
            }
        } 
    }
    else {
        organize(cmdList->left, 1); //run cmd to left
        organize(cmdList->right, 1); //run cmd to right
    }
}

void environment(const CMD *cmdList)
{
    if(cmdList->nLocal != 0){ //set local variables
        for(int i = 0; i < cmdList->nLocal; i++){
            setenv(cmdList->locVar[i], cmdList->locVal[i], 1);
        }
    }
}

void zombies(int x)
{
    //initial variables
    pid_t p;      
    int status;   

    while ((p = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) { //wait for all child procecces
        status = STATUS(status); //use parse.h mac
        fprintf(stderr, "Completed: %d (%d)\n", p, status); //print to stderror
    }
}

int stat(int status)
{
    char s[128];    
    sprintf(s, "%d", status); //store value into s
    setenv("?", s, 1); //environment set ?
    return status;
}