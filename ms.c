#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#ifndef BUFFSIZE
#define BUFFSIZE 4096
#endif

#ifndef MAX_CMD
#define MAX_CMD 100
#endif

typedef int bool;
enum {false, true};

typedef struct {
	int PIPE_LEFT;
	int PIPE_MIDDLE;
	int PIPE_RIGHT;
	int NORMAL;
	int PROG_NUM;
	int ARG_COUNT;
	int FORWARD_IN;
	int FORWARD_OUT;
	int FORWARD_ERROR;
	int FORWARDED;
	char *forward_fname;
	char *args[MAX_CMD];
} cmd;

void parse_command();
void parse_comb_dest();
void execute_prog();
void execute_normal();
void execute_pipe_left();
void execute_pipe_right();
void execute_pipe_middle();
void parse_forward();
void set_pathnames();
void start();
bool check_chdir();

char *read_buff;
cmd *commands[MAX_CMD];
int prog_count = 0;
int BACKGROUND = false;
int PIPEMIDDLE = false;
int CHANGE_DIR = false;
int last_cd_prog_num = -1;
int int_num = 0;
int NEXT_LINE = false;
int IN_EXEC = false;

int shell_pid;
static void int_handler(int signo){
        if(IN_EXEC == false) exit(0);
}

char *file_path;
char *file2_path;
char *error_path = "/tmp/file_error";
char gcwd[BUFFSIZE];

int main(int argc, char **argv){
	shell_pid = getpid();
	struct sigaction act;
	act.sa_handler = int_handler;
	act.sa_flags = SA_RESTART;
	sigaction(SIGINT, &act, NULL);
	
	read_buff = malloc(sizeof(char) * BUFFSIZE);
	
	getcwd(gcwd, BUFFSIZE);
	
	/*char *home = getenv("HOME");
        if(home != NULL && strcmp(home, gcwd) == 0) printf(">home>");
	else printf(">%s>", gcwd);*/
	int len = 0;
	while(true){
		
		char tmp_buff[BUFFSIZE];
		
		char *home = getenv("HOME");
		getcwd(gcwd, BUFFSIZE);
		
		if(NEXT_LINE){
			printf(">");
        	}else if((home != NULL) && (strcmp(home, gcwd) == 0)){
			printf(">home>");
        	}else{
			printf(">%s>", gcwd);
		}
		
		fgets(tmp_buff, BUFFSIZE, stdin);
		
		if(tmp_buff[strlen(tmp_buff) - 1] == '\n'){
                        tmp_buff[strlen(tmp_buff) - 1] = 0;
                }
		
		if(len == 0){
			strcpy(read_buff, tmp_buff);
		}else{
			strcat(read_buff, tmp_buff);
		}
		
		if(read_buff[strlen(read_buff) - 1] == '\\'){
			read_buff[strlen(read_buff) - 1] = 0;
			NEXT_LINE = true;
			len = strlen(read_buff);
			continue;
		}else{
			NEXT_LINE = false;
			len = 0;
		}
		
		PIPEMIDDLE = false;
		if(read_buff[strlen(read_buff) - 1] == '&'){
			BACKGROUND = true;
		}
		else{
			BACKGROUND = false;
			IN_EXEC = true;
		}
		
		parse_command(commands);
		
		last_cd_prog_num = -1;
		
		start();
		
		if(last_cd_prog_num > -1 && BACKGROUND == false) check_chdir(commands[last_cd_prog_num]);

	}
	
	
	return EXIT_SUCCESS;
}

void find_last_cd_prog_num(){
	for(int i = 0; i < prog_count; i++){
		char *arg = commands[i]->args[0];
		if(strlen(arg) == 2){
			if(arg[0] == 'c' && arg[1] == 'd'){
				last_cd_prog_num = commands[i]->PROG_NUM;
			}
		}
	}
}

void start(){
	if(BACKGROUND == true){
		struct sigaction act;
                act.sa_handler = SIG_IGN;
		act.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
                sigaction(SIGCHLD, &act, NULL);
	}else{
		struct sigaction act;
                act.sa_handler = SIG_DFL;
                act.sa_flags = SA_RESTART;
                sigaction(SIGCHLD, &act, NULL);
	}
	find_last_cd_prog_num();
	int pid, stat;
	if((pid = fork()) < 0){
		perror("fork error");
	}
	if(pid == 0){
		if(BACKGROUND == true){
			struct sigaction act;
                	act.sa_handler = SIG_IGN;
			act.sa_flags = SA_RESTART;
                	sigaction(SIGINT, &act, NULL);
			
			struct sigaction act2;
               		act2.sa_handler = SIG_DFL;
                	act2.sa_flags = SA_RESTART;
                	sigaction(SIGCHLD, &act2, NULL);
		}
		set_pathnames();
        	for(int i = 0; i < prog_count; i++){
                	execute_prog(commands[i]);
        	}
		exit(0);
	}else if(pid > 0){
		if(BACKGROUND == false){
			if((pid = waitpid(pid, &stat, 0)) < 0){
                        	perror("waitpid() error");
			}
			IN_EXEC = false;
                }
	}
}

void set_pathnames(){
	int pid = getpid();
        char *s_pid = malloc(sizeof(char) * BUFFSIZE);
        char *path1 = malloc(sizeof(char) * BUFFSIZE);
        sprintf(s_pid, "%d", pid);
        strcpy(path1, "/tmp/file");
        strcat(path1, s_pid);
        strcat(path1, ".1");
        file_path = path1;
	
	char *path2 = malloc(sizeof(char) * BUFFSIZE);
        strcpy(path2, "/tmp/file");
        strcat(path2, s_pid);
        strcat(path2, ".2");
        file2_path = path2;
}

void execute_prog(cmd *command){
	
	if(check_chdir(command)){
		//do nothing
	}else if(command->NORMAL){
		execute_normal(command);
	}else if(command->PIPE_RIGHT){
		execute_pipe_right(command);
	}else if(command->PIPE_LEFT){
		execute_pipe_left(command);
	}else if(command->PIPE_MIDDLE){
		execute_pipe_middle(command);
	}
	
}



void execute_pipe_right(cmd *command){
	PIPEMIDDLE = false;
	
	int pid, stat;
	int fd;
	
        if((pid = fork()) < 0){
                perror("fork error");
        }else if(pid == 0){
		fd = open(file_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
		
		//fd = open(fifo_path, O_WRONLY);

        	dup2(fd, 1);
		close(fd);
	
		if(command->FORWARDED){
                        if(command->FORWARD_IN){
                                int fd = open(command->forward_fname, O_RDONLY);
                                if(fd < 0){
                                        perror("open error");
                                }
                                dup2(fd, 0);
                                close(fd);
                        }else if(command->FORWARD_ERROR){
                                int fd = open(command->forward_fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                                dup2(fd, 2);
                                close(fd);
                        }
                }

                execvp(command->args[0], command->args);
        }else if(pid > 0){
                if((pid = waitpid(pid, &stat, 0)) < 0){
                        perror("waitpid() error");
                }
        }
}

void execute_pipe_left(cmd *command){
	int pid, stat;
	int fd;

        if((pid = fork()) < 0){
                perror("fork error");
        }else if(pid == 0){
                if(PIPEMIDDLE == false){
			fd = open(file_path, O_RDONLY);
			
			dup2(fd, 0);
			close(fd);
		}else{
			fd = open(file2_path, O_RDONLY);

                        dup2(fd, 0);
                        close(fd);
		}
		
		if(command->FORWARDED){
                        if(command->FORWARD_OUT){
                                int fd = open(command->forward_fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                                dup2(fd, 1);
                                close(fd);
                        }else if(command->FORWARD_ERROR){
                                int fd = open(command->forward_fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                                dup2(fd, 2);
                                close(fd);
				int fd2 = open(error_path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                                dup2(fd2, 1);
                                close(fd2);
                        }else if(command->FORWARD_IN){
				exit(0);
			}
                }

		execvp(command->args[0], command->args);
        }else if(pid > 0){
                if((pid = waitpid(pid, &stat, 0)) < 0){
                        perror("waitpid() error");
                }
		
        }
}

void execute_pipe_middle(cmd *command){
	int pid, stat;
        int fd, fd2;

        if((pid = fork()) < 0){
                perror("fork error");
        }else if(pid == 0){
                if(PIPEMIDDLE == false){
                        fd = open(file_path, O_RDONLY);

                        dup2(fd, 0);
                        close(fd);
                }else{
                        fd = open(file2_path, O_RDONLY);

                        dup2(fd, 0);
                        close(fd);
                }
		
		if(PIPEMIDDLE == false){
			fd2 = open(file2_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);
			
			dup2(fd2, 1);
			close(fd2);
		}else{
			fd2 = open(file_path, O_CREAT|O_WRONLY|O_TRUNC, 0666);

                        dup2(fd2, 1);
                        close(fd2);
		}
		
                execvp(command->args[0], command->args);
        }else if(pid > 0){
                if((pid = waitpid(pid, &stat, 0)) < 0){
                        perror("waitpid() error");
                }
		if(PIPEMIDDLE){
			PIPEMIDDLE = false;
		}else{
			PIPEMIDDLE = true;
		}
        }
}

void execute_normal(cmd *command){
	int pid, stat;
	
	if((pid = fork()) < 0){
		perror("fork error");
	}else if(pid == 0){
		if(command->FORWARDED){
			if(command->FORWARD_IN){
				int fd = open(command->forward_fname, O_RDONLY);
				if(fd < 0){
					perror("open error");
				}
				dup2(fd, 0);
				close(fd);
			}else if(command->FORWARD_OUT){
				int fd = open(command->forward_fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
				dup2(fd, 1);
				close(fd);
			}else if(command->FORWARD_ERROR){
				int fd = open(command->forward_fname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
				dup2(fd, 2);
				close(fd);
				int fd2 = open(error_path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
				dup2(fd2, 1);
				close(fd2);
			}
		}
		execvp(command->args[0], command->args);
	}else if(pid > 0){
		if((pid = waitpid(pid, &stat, 0)) < 0){
			perror("waitpid() error1");
		}
	}
}

bool check_chdir(cmd *command){
	char *home = getenv("HOME");
	char *parg = command->args[0];
	if(strlen(parg) > 1 && parg[0] == 'c' && parg[1] == 'd'){
		if(command->ARG_COUNT > 1){
			char *arg = command->args[1];
			if(arg[0] == '~' && home != NULL){
				chdir(home);
			}else{
				chdir(command->args[1]);
			}
			return true;
		}else if(command->ARG_COUNT == 1){
			getcwd(gcwd, BUFFSIZE);
			chdir(gcwd);
			return true;
		}
	}
	return false;
}

void parse_command(cmd *commands[]){
	//acmd **commands = malloc(sizeof(cmd*) * MAX_CMD);
        //char *args[MAX_CMD];
	char *rbuff = calloc(BUFFSIZE, sizeof(char));
	char *prog[MAX_CMD];
	char *ufwd[MAX_CMD];
	char *afwd[MAX_CMD];

	strncpy(rbuff, read_buff, strlen(read_buff));
	
        char *t, *st, *ft;
	
	t = strtok(rbuff, "|&");
	int count = 0;
	while(t != NULL){
		//prog[count] = malloc(sizeof(char) * BUFFSIZE);
		//strcpy(prog[count], t);
		prog[count] = t;
		t = strtok(NULL, "|&");
		count++;
	}

	prog_count = count;
	
	for(int i = 0; i < count; i++){
		commands[i] = malloc(sizeof(cmd));
		commands[i]->FORWARDED = false;
	}
	
	int n = 0;
	for(int i = 0; i < count; i++){
		
		char *temp = malloc(sizeof(char) * BUFFSIZE);
		strcpy(temp, prog[i]);
		
		ufwd[i] = prog[i];
		
		for(int j = 0; j < strlen(prog[i]); j++){	//forward_fname
			
			if(temp[j] == '<'){
				
				n = j - 1;
				ufwd[i] = malloc(sizeof(char) * BUFFSIZE);
				strncpy(ufwd[i], temp, n);
				char *afwd = malloc(sizeof(char) * BUFFSIZE);
				strcpy(afwd, temp + j + 1);
				char *temp = strtok(afwd, " ");
				commands[i]->forward_fname = temp;
				commands[i]->FORWARD_IN = true;
				commands[i]->FORWARD_OUT = false;
				commands[i]->FORWARD_ERROR = false;
				commands[i]->FORWARDED = true;
			}else if(temp[j] == '>'){
				if(temp[j + 1] == '>'){		//error
					j++;
					n = j - 2;
                                        ufwd[i] = malloc(sizeof(char) * BUFFSIZE);
                                        strncpy(ufwd[i], temp, n);
                                        char *afwd = malloc(sizeof(char) * BUFFSIZE);
                                        strcpy(afwd, temp + j + 1);
                                        char *temp = strtok(afwd, " ");
                                	commands[i]->forward_fname = temp;
                                        commands[i]->FORWARD_IN = false;
                                        commands[i]->FORWARD_OUT = false;
                                        commands[i]->FORWARD_ERROR = true;
					commands[i]->FORWARDED = true;
				}else{
					n = j - 1;
                                	ufwd[i] = malloc(sizeof(char) * BUFFSIZE);
                                	strncpy(ufwd[i], temp, n);
                                	char *afwd = malloc(sizeof(char) * BUFFSIZE);
                                	strcpy(afwd, temp + j + 1);
                                	char *temp = strtok(afwd, " ");
                                	commands[i]->forward_fname = temp;
                                	commands[i]->FORWARD_IN = false;
                                	commands[i]->FORWARD_OUT = true;
                                	commands[i]->FORWARD_ERROR = false;
					commands[i]->FORWARDED = true;
				} 
			}
		}
	}
	
	for(int i = 0; i < count; i++){
		//printf("%s\n", prog[i]);
		//commands[i] = malloc(sizeof(cmd));

		st = strtok(ufwd[i], " ");
		
		int j = 0;
		while(st != NULL){
			commands[i]->args[j] = st;
			st = strtok(NULL, " ");
			j++;
		}
		commands[i]->args[j] = NULL;
		commands[i]->PROG_NUM = i;
		commands[i]->ARG_COUNT = j;
		commands[i]->PIPE_RIGHT = false;
		commands[i]->PIPE_LEFT = false;
		commands[i]->PIPE_MIDDLE = false;
		commands[i]->NORMAL = false;
	}
	
	parse_comb_dest(commands);
}

void parse_comb_dest(cmd *commands[]){
	char pa[MAX_CMD];
	memset(pa, 'e', MAX_CMD);
	
	int count = 0;
	for(int i = 0; i < strlen(read_buff); i++){
		if(read_buff[i] == '|'){
			pa[count] = 'p';
			count++;
		}
		else if(read_buff[i] == '&' && i + 1 != strlen(read_buff)){
			if(read_buff[i++] == '&'){
				pa[count] = 'a';
				count++;
			}
		}else if(read_buff[i] == '&' && i + 1 == strlen(read_buff)){
			BACKGROUND = true;
		}
	} 
	if(prog_count > 1){
		for(int i = 0; i < prog_count; i++){
			int pnum = commands[i]->PROG_NUM;
			if(pnum == 0){
				if(pa[pnum] == 'p') commands[i]->PIPE_RIGHT = true;
				else if(pa[pnum] == 'a') commands[i]->NORMAL = true;
			}else{
				if(pa[pnum - 1] == 'p' && pa[pnum] == 'p') commands[i]->PIPE_MIDDLE = true;
				else if(pa[pnum - 1] == 'p') commands[i]->PIPE_LEFT = true;
				else if(pa[pnum] == 'p') commands[i]->PIPE_RIGHT = true;
				else if(pa[pnum - 1] == 'a' || pa[pnum] == 'a') commands[i]->NORMAL = true;
			}
		}
	}else if(prog_count == 1) commands[0]->NORMAL = true;
}
