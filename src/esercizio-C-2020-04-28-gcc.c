#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h> 	// for check_file_existence

#include <sys/types.h> 	// fork
#include <unistd.h> 	// fork
#include <sys/wait.h> 	// wait
#include <sys/stat.h>	// open
#include <fcntl.h>		// open
#include <sys/inotify.h>// inotify
#include <fcntl.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>
#include <time.h>
#include <errno.h>

int check_file_existence(char * fname);
char * concat(const char *s1, const char *s2);
void create_hello_world();
void parent_process_signal_handler(int signum);
void fork_and_compile();
void fork_and_execute();

extern char **environ;

char * dir_path = "/home/andrea/Scrivania/src";
int fd;

int * pid_list;
int pid_list_length = 0;

int main(){


	if(check_file_existence(dir_path)){
		// src folder exists
		printf("%s : exists\n", dir_path);
	}else{
		// src folder do not exists
		printf("%s : do not exists\n", dir_path);
		// create dir
		mkdir(dir_path, 00755);
	}

	int res = chdir(dir_path);
	if(res == -1){
		perror("chdir()");
		exit(1);
	}

	char * file_hello_world = "hello_world.c";
	if(! check_file_existence(file_hello_world)){
		// file_hello_world do not exists
		// create open and write the source code for hello_word.c
		create_hello_world();
	}

	fd = open("output.txt", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fd == -1){
		perror("open()");
		exit(1);
	}

	if (signal(SIGCHLD, parent_process_signal_handler) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}


	// watch hello_world.c for this task use inotify
	int wd;
	int inotifyFd;
	int num_bytes_read;

	// inotify_init() initializes a new inotify instance and
	// returns a file descriptor associated with a new inotify event queue.
    inotifyFd = inotify_init();
    if (inotifyFd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    wd = inotify_add_watch(inotifyFd, dir_path, IN_MODIFY);
    if (wd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    int BUF_LEN = 4096;
    char buf[BUF_LEN];
    clock_t last_modify = clock();
    clock_t difference;
    int msec;
    // loop forever
    while(1) {
    	num_bytes_read = read(inotifyFd, buf, BUF_LEN);
        if (num_bytes_read == 0) {
            printf("read() from inotify fd returned 0!");
            exit(EXIT_FAILURE);
        }
        if (num_bytes_read == -1) {
        	if (errno == EINTR) {
				printf("read(): EINTR\n");
				continue;
        	} else {
                perror("read()");
                exit(EXIT_FAILURE);
        	}
        }

        // process all of the events in buffer returned by read()
        struct inotify_event *event;
        for (char * p = buf; p < buf + num_bytes_read; ) {
            event = (struct inotify_event *) p;
            difference = clock() - last_modify;
            msec = difference * 1000 / CLOCKS_PER_SEC;
            if(event->mask == IN_MODIFY  && msec > 1000){
            	printf("modificato\n"); //TEST
            	fork_and_compile();
            	last_modify = clock();
            }

            p += sizeof(struct inotify_event) + event->len;
        }
    }

	close(fd);
	close(inotifyFd);

	exit(0);
}


int check_file_existence(char * fname) {

	if( access( fname, F_OK ) != -1 ) {
	    // file exists
		return 1;
	} else {
	    // file doesn't exist
		return 0;
	}
}

char * concat(const char *s1, const char *s2){
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    if(result == NULL){
    	perror("malloc()");
    }
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

void create_hello_world(){
	int fd = open("hello_world.c", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fd == -1){
		perror("open()");
		exit(1);
	}

	char * lines[] = {	"#include <stdio.h>",
						"int main() {",
						"	printf(\"Hello, World!\");",
						"	return 0;",
						"}"
	};
	int array_lines_length = sizeof(lines) / sizeof(char *);

	for(int i=0 ; i<array_lines_length ; i++){
		int res = write(fd, concat(lines[i], "\n"), strlen(lines[i])+1);
		if(res == -1){
			perror("write()");
		}
	}
	close(fd);
}

void parent_process_signal_handler(int signum) {
	// riceviamo SIGCHLD: Child stopped or terminated

	printf("[process_signal_handler]\n");


	if(pid_list_length > 0){
		printf("zombie\n");
		printf("pid_list_length before wait: %d\n", pid_list_length);
		waitpid(pid_list[pid_list_length-1], NULL, 0);
		pid_list_length--;
		printf("pid_list_length after wait: %d\n", pid_list_length);
	}

}


void fork_and_compile(){
	int wstatus;
	int result_from_child = -1;

	pid_t child_pid;
	child_pid = fork();

	switch (child_pid) {
		case 0:{
			char * newargv[] = {"gcc", "hello_world.c", "-o", "hello", NULL };
			close(fd);
			execve("/usr/bin/gcc", newargv, environ);
			perror("execve()");
			exit(1);
		}
		case -1:
			printf("fork() failed\n");
			exit(1);
		default:
			do {
				pid_t ws = waitpid(child_pid, &wstatus, WUNTRACED | WCONTINUED);
				if (ws == -1) {
					perror("[parent] waitpid");
					exit(EXIT_FAILURE);
				}
				if (WIFEXITED(wstatus)) {
					result_from_child = WEXITSTATUS(wstatus);
					//printf("[parent] child process è terminato, ha restituito: %d\n", result_from_child);
				} else if (WIFSIGNALED(wstatus)) {
					printf("[parent] child process killed by signal %d\n", WTERMSIG(wstatus));
				} else if (WIFSTOPPED(wstatus)) {
					printf("[parent] child process stopped by signal %d\n", WSTOPSIG(wstatus));
				} else if (WIFCONTINUED(wstatus)) {
					printf("[parent] child process continued\n");
				}
			} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

			if (result_from_child != -1) {
				printf("[parent] compilation exit value: %d\n", result_from_child);
				if(result_from_child == 0){
					// correct compile then execute
					fork_and_execute();
				}
			}
	}
}

void fork_and_execute(){


	pid_t child_pid;
	pid_list_length++;
	pid_list = realloc(pid_list, pid_list_length*sizeof(pid_t));
	if(pid_list == NULL){
		perror("realloc()");
	}

	child_pid = fork();
	switch (child_pid) {
		case -1:
			printf("fork() failed\n");
			exit(1);
		case 0:{
			pid_list[pid_list_length-1] = child_pid;

		    if (dup2(fd, STDOUT_FILENO) == -1) {
		    	perror("problema con dup2");
		    	exit(EXIT_FAILURE);
		    }
		    close(fd);

			char * newargv[] = {"./hello ", NULL };
			char * newenv[] = { NULL };
			execve(concat(dir_path,"/hello"), newargv, newenv);

			perror("execve()");
			exit(1);
		}
		default:{
			//father
			pid_list[pid_list_length-1] = child_pid;

			/*
			int res = wait(NULL);
			if(res == -1){
				perror("wait()");
				exit(1);
			}
			*/
		}
	}

}
