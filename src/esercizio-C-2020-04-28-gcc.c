#include <stdio.h>
#include <stdlib.h>

#include <unistd.h> 	// for check_file_existence

#include <sys/types.h> 	// fork
#include <unistd.h> 	// fork
#include <sys/wait.h> 	// wait
#include <sys/stat.h>	// open
#include <fcntl.h>		// open
#include <fcntl.h>
#include <string.h>


int check_file_existence(char * fname);
void make_dir(char * dir_path);


int main(){
	char * dir_path = "../src";

	if(check_file_existence(dir_path)){
		// src folder exists
		printf("%s : exists\n", dir_path);
	}else{
		// src folder do not exists
		printf("%s : do not exists\n", dir_path);
		// create dir
		make_dir(dir_path);
	}

	char * file_hello_world = "../src/hello_world.c";
	if(! check_file_existence(file_hello_world)){
		// file_hello_world do not exists
		// create open and write the code
		int fd = open("hello_world.c", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
		if(fd == -1){
			perror("open()");
			exit(1);
		}

		char * lines[] = {	"#include <stdio.h>\n",
							"int main() {\n",
							"	printf(\"Hello, World!\");\n",
							"	return 0;\n",
							"}"
		};
		int array_lines_length = sizeof(lines) / sizeof(char *);

		for(int i=0 ; i<array_lines_length ; i++){
			int res = write(fd, lines[i], strlen(lines[i]));
			if(res == -1){
				perror("write()");
			}
		}

		close(fd);

	}

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
