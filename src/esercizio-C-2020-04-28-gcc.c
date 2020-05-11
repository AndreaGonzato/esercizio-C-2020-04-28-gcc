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
void make_dir(char * dir_path);
char * concat(const char *s1, const char *s2);
void create_hello_world();
void parent_process_signal_handler(int signum);
void fork_and_compile();

char * dir_path = "/home/andrea/Scrivania/src";

int main(){


	if(check_file_existence(dir_path)){
		// src folder exists
		printf("%s : exists\n", dir_path);
	}else{
		// src folder do not exists
		printf("%s : do not exists\n", dir_path);
		// create dir
		make_dir(dir_path);
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

	int fd = open("output.txt", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
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

    wd = inotify_add_watch(inotifyFd, file_hello_world, IN_MODIFY);
    if (wd == -1) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }

    int BUF_LEN = 4096;
    char buf[BUF_LEN];
    time_t last_modify = NULL;
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

            if(event->mask == IN_MODIFY && last_modify+1 < time(NULL)){
            	printf("modificato\n"); //TEST
            	fork_and_compile();
            	last_modify = time(NULL);
            }

            p += sizeof(struct inotify_event) + event->len;
            // event->len is length of (optional) file name
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

void make_dir(char * dir_path){
	pid_t child_pid = fork();
	switch(child_pid){
		case 0:{
			// child
			char * newargv[] = { "mkdir", dir_path, NULL };
			char * newenviron[] = { NULL };
			execve("/bin/mkdir", newargv, newenviron);
			break;
		}
		case -1:
			printf("fork() fail!\n");
			exit(1);
		default:
			// father
			waitpid(child_pid, NULL, 0);
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

	printf("[parent] parent_process_signal_handler\n");
}


void fork_and_compile(){
	int wstatus;
	int result_from_child = -1;

	pid_t child_pid;
	child_pid = fork();

	switch (child_pid) {
		case 0:{
			char * newargv[] = {"gcc", "hello_world.c", "-o", "hello", NULL };
			char * newenviron[] = { NULL };
			execve("/usr/bin/gcc", newargv, newenviron);
			perror("execve()");
			break;
		}
		case -1:
			printf("fork() failed\n");
			exit(1);
			break;
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
				printf("[parent] il valore restituito dal processo figlio è %d\n", result_from_child);
			}
			exit(0);
	}
}
