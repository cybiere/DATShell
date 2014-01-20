#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


#define ARGS_SPACE 5

#define FORK_FAILURE 1
#define OPEN_FAILURE 2
#define DUP_FAILURE 3
#define PIPE_FAILURE 4
#define EXEC_FAILURE -1

#define REDIRECT_NONE 0
#define REDIRECT_OUT_TRUNC 1
#define REDIRECT_OUT_APPEND 2
#define REDIRECT_IN 4
#define REDIRECT_ERR 8
#define REDIRECT_PIPE 16

#define WAIT 0
#define NOWAIT 1

#define BG_NONEW 0
#define BG_FREE 1


void prompt(int retVal){
	char promptSeparator,hostname[40];
	if(!getuid()) promptSeparator = '#'; else promptSeparator = '$';
	gethostname(hostname,40);
	printf("[%d] %s@%s:%s %c ",retVal,getenv("USER"),hostname,getenv("PWD"),promptSeparator);
}

int getcmd(char *** ret,int retVal,int *redir,char **fileOut,char **fileIn,char **fileErr, int *wait){
	int argc,freeblocks;
	char *buf, **argv,pipe=0;

	*wait = WAIT;
	argc = 0;
	freeblocks = ARGS_SPACE;
	argv = (char **)malloc(ARGS_SPACE*sizeof(char *));
	if(*redir != REDIRECT_PIPE) prompt(retVal);
	scanf("%ms",&buf);
	argv[argc] = buf;
	argc++;
	freeblocks--;

	while(getchar() != '\n' && pipe == 0){
		if(freeblocks == 0){
			argv = (char **)realloc(argv,ARGS_SPACE*sizeof(char *));
			freeblocks = ARGS_SPACE;
		}
		scanf("%ms",&buf);
		if(strcmp(buf,">") == 0){
			if(getchar() != '\n'){
				*redir = REDIRECT_OUT_TRUNC;
				scanf("%ms",fileOut);
			}
		}else if(strcmp(buf,">>") == 0){
			if(getchar() != '\n'){
				*redir = REDIRECT_OUT_APPEND;
				scanf("%ms",fileOut);
			} 
		}else if(strcmp(buf,"<") == 0){
			if(getchar() != '\n'){
				*redir = REDIRECT_IN;
				scanf("%ms",fileIn);
			}
		}else if(strcmp(buf,"2>") == 0){
			if(getchar() != '\n'){
				*redir = REDIRECT_ERR;
				scanf("%ms",fileErr);
			}
		}else if(strcmp(buf,"&") == 0){
			*wait = NOWAIT;
		}else if(strcmp(buf,"|") == 0){
			pipe = 1;
			*redir = REDIRECT_PIPE;
		}else{
			argv[argc] = buf;
			argc++;
			freeblocks--;
		}
	}
	if(freeblocks == 0){
		argv = (char **)realloc(argv,sizeof(char));
	}
	argv[argc] = NULL;
	*ret = argv;
	return argc;
}

void freeArgs(int argc, char **argv){
	for(;argc>=0;argc--)
		free(argv[argc]);
	free(argv);	
}

void processChildren(pid_t new){
	static int *array=NULL;
	static int nbprocessus=0;
	static int freespace=5;
	int i,son_status,pid;
	if(array == NULL) array = (int *)malloc(5*sizeof(int));

	if(new == BG_FREE){
		free(array);
		return;
	}
	if(new != BG_NONEW){
		if(freespace == 0){
			array = (int *)realloc(array,5*sizeof(int));
			freespace = 5;
		}
		array[nbprocessus] = new;
		nbprocessus++;
		freespace--;
	}
	for(i=0;i<nbprocessus;i++){
		if((pid = waitpid(array[i],&son_status,WNOHANG)) == array[i]){
			printf("\nProcess %d terminated with code %d\n",pid,WEXITSTATUS(son_status));
			array[i] = array[nbprocessus-1];
			array[nbprocessus-1] = 0;
			nbprocessus--;
			freespace++;
		}else if(pid == -1){
			perror("waitpid failed. ");
		}
	}
}


int main(void){ 
	char **argv,*fileIn=NULL,*fileOut=NULL,*fileErr=NULL,cwd[BUFSIZ];
	int argc, son_status, retVal=0,redir=REDIRECT_NONE,out=1,in=0,err=2,wait=WAIT,tube[2];
	pid_t pid;


	printf("Welcome on DATShell! (Damn, Another Tiny Shell !).\n");
	while(1){
		processChildren(BG_NONEW);
		argc = getcmd(&argv,retVal,&redir,&fileOut,&fileIn,&fileErr,&wait);
		if(strcmp(argv[0],"exit") == 0){
			printf("Exiting DatShell!\n");
			freeArgs(argc,argv);
			processChildren(BG_FREE);
			return EXIT_SUCCESS;
		}else if(strcmp(argv[0],"cd") == 0){
			if(argc == 1)
				chdir(getenv("HOME"));
			else if(chdir(argv[1])){
				perror("Unable to cd ");
			}
			setenv("PWD",getcwd(cwd,BUFSIZ),1);
		} else {
			pid = fork();
			switch(pid){
				case -1:
					perror("Unable to fork.");
					freeArgs(argc,argv);
					return FORK_FAILURE;
					break;
				case 0:
					switch(redir){
						case REDIRECT_OUT_TRUNC:
							out = open(fileOut,O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
							if(out == -1){
								perror("Unable to open file ");
								exit(OPEN_FAILURE);
							}
							if(dup2(out,STDOUT_FILENO) == -1){
								perror("Unable to redirect ");
								exit(DUP_FAILURE);
							}
							break;
						case REDIRECT_OUT_APPEND:
							out = open(fileOut,O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP);
							if(out == -1){
								perror("Unable to open file ");
								exit(OPEN_FAILURE);
							}
							if(dup2(out,STDOUT_FILENO) == -1){
								perror("Unable to redirect ");
								exit(DUP_FAILURE);
							}
							break;
						case REDIRECT_IN:
							in = open(fileIn,O_RDONLY);
							if(in == -1){
								perror("Unable to open file ");
								exit(OPEN_FAILURE);
							}
							if(dup2(in,STDIN_FILENO) == -1){
								perror("Unable to redirect ");
								exit(DUP_FAILURE);
							}
							break;
						case REDIRECT_ERR:
							err = open(fileErr,O_WRONLY | O_CREAT | O_APPEND,S_IRUSR | S_IWUSR | S_IRGRP);
							if(err == -1){
								perror("Unable to open file ");
								exit(OPEN_FAILURE);
							}
							if(dup2(err,STDERR_FILENO) == -1){
								perror("Unable to redirect ");
								exit(DUP_FAILURE);
							}
							break;
						case REDIRECT_PIPE:
							if(pipe(tube) == -1){
								perror("Unable to create pipe ");
								exit(PIPE_FAILURE);
							}
							switch(fork()){
								case -1:
									perror("Unable to fork !");
									return FORK_FAILURE;
									break;
								case 0:
									close(tube[0]);
									if(dup2(tube[1],STDOUT_FILENO) == -1){
										perror("Unable to redirect ");
										exit(DUP_FAILURE);
									}

									execvp(argv[0],argv);
									fprintf(stderr,"ttsh : command not found : %s\n",argv[0]);
									freeArgs(argc,argv);
									return EXEC_FAILURE;
									break;
								default :
									argc = getcmd(&argv,retVal,&redir,&fileOut,&fileIn,&fileErr,&wait);
									close(tube[1]);
									if(dup2(tube[0],STDIN_FILENO) == -1){
										perror("Unable to redirect ");
										exit(DUP_FAILURE);
									}
									execvp(argv[0],argv);
									fprintf(stderr,"ttsh : command not found : %s\n",argv[0]);
									freeArgs(argc,argv);
							}
							break;
					}
						execvp(argv[0],argv);
						fprintf(stderr,"ttsh : command not found : %s\n",argv[0]);
						freeArgs(argc,argv);
						return EXEC_FAILURE;
					break;
				default :
					if(redir == REDIRECT_PIPE){
						argc = getcmd(&argv,retVal,&redir,&fileOut,&fileIn,&fileErr,&wait);

					}
					redir = REDIRECT_NONE;
					if(wait == WAIT){
						waitpid(pid,&son_status,0);
						retVal = WEXITSTATUS(son_status);
					}else{
						processChildren(pid);
					}
			}
		}
		freeArgs(argc,argv);
		free(fileIn);
		free(fileOut);
		free(fileErr);
		fileIn = NULL;
		fileOut = NULL;
		fileErr = NULL;
	}

	return 0;
}
