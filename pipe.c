// pipe.c                                         Stan Eisenstat (11/09/09)
// Execute a chain of filters that do not take command line arguments; e.g.,
// "pipe ls wc" is equivalent to "ls | wc"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Print error message and die with STATUS
#define errorExit(status)  perror("pipe"), exit(status)

int main (int argc, char *argv[])
{
    struct entry {              // Table with (pid,status) for each child
	int pid, status;
    } table[argc];
    int fd[2],                  // Read and write file descriptors for pipe
	pid, status,            // Process ID and status of child
	fdin,                   // Read end of last pipe (or original stdin)
	i,j;

    if (argc < 2)  {
	printf ("Usage:  pipe filter1 filter2 ... filterN\n");
	exit(0);
    }

    fdin = 0;                                   // Remember original stdin
    for (i = 1; i < argc-1; i++) {              // Create chain of processes
	if (pipe(fd) || (pid = fork()) < 0)
	    errorExit (EXIT_FAILURE);

	else if (pid == 0) {                    // Child process
	    close (fd[0]);                      //  No reading from new pipe
	    if (fdin != 0)  {                   //  stdin = read[last pipe]
		dup2 (fdin, 0);
		close (fdin);
	    }
	    if (fd[1] != 1) {                   //  stdout = write[new pipe]
		dup2 (fd[1], 1);
		close (fd[1]);
	    }
	    execlp (argv[i], argv[i], NULL);    //  Overlay by i-th filter
	    errorExit (EXIT_FAILURE);

	} else {                                // Parent process
	    table[i].pid = pid;                 //  Save child pid
	    if (i > 1)                          //   Close read[last pipe]
		close (fdin);                   //    if not original stdin
	    fdin = fd[0];                       //  Remember read[new pipe]
	    close (fd[1]);                      //  No writing to new pipe
	}
    }

    if ((pid = fork()) < 0)                     // Create last process
	errorExit (EXIT_FAILURE);

    else if (pid == 0) {                        // Child process
	if (fdin != 0) {                        //  stdin = read[last pipe]
	    dup2 (fdin, 0);
	    close (fdin);
	}
	execlp (argv[argc-1], argv[argc-1], 0); //  Overlay by last filter
	errorExit (EXIT_FAILURE);

    } else {                                    // Parent process
	table[argc-1].pid = pid;                //  Save child pid
	if (i > 1)                              //  Close read[last pipe]
	    close (fdin);                       //   if not original stdin
    }
	 
    for (i = 1; i < argc; ) {                   // Wait for children to die
	pid = wait (&status);
	for (j = 1; j < argc && table[j].pid != pid; j++)
	    ;
	if (j < argc) {                         // Ignore zombie processes
	    table[j].status = status;
	    i++;
	}
    }

    for (i = 1; i < argc; i++) {                // Print information
	printf ("%-10s  pid=%d  signal=%d  status=%d\n",
	    argv[i], table[i].pid,
	    WTERMSIG (table[i].status),
	    WEXITSTATUS (table[i].status));
    }

    return EXIT_SUCCESS;
}
