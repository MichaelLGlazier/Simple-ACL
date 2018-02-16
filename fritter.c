#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>

/*
*Checks to see that the file exist and that
*it has a corresponding acl file
*@return 1 if file is good, -1 if bad
*/
int inspectInput(char* file, uid_t euid, uid_t ruid){
	int len = 0;
	char* acl;

	len = strlen(file);
	acl = (char*)malloc((len + 1) + 4); //file.acl\0

	memset(acl, 0, (len + 1) + 4);
	strncpy(acl, file, len); //don't copy \0
	strncat(acl, ".acl", len + 5);

	if((access(file, F_OK) == 0 )&& (access(acl, F_OK) == 0)){
		free(acl);
		return 1;
	}
	else{	
		printf("%s\n", strerror(errno));
		free(acl);
		return -1;
	}
}
char* input(){
	char buf[100];
	char* inStr, *temp;
	char c = 0;
	int i = 0;
	int x = 40;
	inStr = malloc(x);
	sprintf(buf, "Enter an input string: ");
	write(1, buf, strlen(buf));
	while(read(0, &c, 1) != 0 && c != '\n'){
		if(c < 32 || c > 127){
			//invalid input
			free(inStr);
			sprintf(buf, "\nInvalid character input!\n");
			exit(1);
		}
		inStr[i] = c;
		i++;
		//allocate more memory if needed
		if(i == x){
			if(i == INT_MAX){
				printf("\nInput can not get any larger.\n");
				return inStr;
			}
			//overflow check
			if((x * 2) < x){
				x = INT_MAX;
			}
			else{
				x = x * 2;
			}
			temp = realloc(inStr, x);
			if(temp == NULL){
				free(inStr);
				exit(-1);
			}

			//make inStr point equal to temp
			inStr = temp;
		}
	}
	inStr[i] = '\0';

	printf("\"%s\" appended to target file.\n", inStr);

	return inStr;
}
int switchMode(){
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	getresuid(&ruid, &euid, &suid);
	return 0;
}
int testPermission(char* file, uid_t e, uid_t r){
	struct passwd *pwd;
	struct stat info1, info2;
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	int aclFd;
	int len = 0;
	char* acl;
	
	getresuid(&ruid, &euid, &suid);
	pwd = getpwuid(ruid);

	printf("RUID: %s\n", pwd->pw_name);
	len = strlen(file);
	acl = (char*)malloc((len + 1) + 4); //file.acl\0

	memset(acl, 0, (len + 1) + 4);
	strncpy(acl, file, len); //don't copy \0
	strncat(acl, ".acl", len + 5);

	//test that file and file.acl are owned by same user
	if(stat(file, &info1)){
		printf("%s: %s\n", file, strerror(errno));
		free(acl);
		exit(-1);
	}
	if(stat(acl, &info2)){
		printf("%s: %s\n", acl, strerror(errno));
		free(acl);
		exit(-1);
	}	

	if((int)info1.st_uid != (int)info2.st_uid){
		printf("%s and %s are not owned by same user.\n", file, acl);
		free(acl);
		exit(-1);
	}

	/*===BEGIN PRIVILEGE===*/
	seteuid(e); //privileges ended after function return 
	aclFd = open(acl, O_RDONLY);
	if(aclFd == -1){
		printf("%s: %s\n", acl, strerror(errno));
		free(acl);
		exit(-1);
	}

	int x = 10;
	int i = 0;
	char *userID = malloc(x);
	char *temp;
	char c = 0;
	int err = -1;
	int n = 0;
	char *truncUserID;
	while(err != 0){
		i = 0;
		memset(userID, 0, x);
		//get line from file
		while((err = read(aclFd, &c, 1)) != 0 && c != '\n'){
			userID[i] = c;
			i++;
			if(i == x){
				if(i == INT_MAX){
					printf("\nFile read error. Can't allocate enough space.\n");
					free(userID);
					free(acl);
					exit(-1);
				}
				//overflow check
				if((x * 2) < x){
					x = INT_MAX;
				}
				else{
					x = x * 2;
				}
				temp = realloc(userID, x);
				if(temp == NULL){
					free(userID);
					free(acl);
					exit(-1);
				}

				//make inStr point to temp
				userID = temp;
			}
		}
		userID[i] = '\0';
		/*
		//check value of line
		id = atoi(userID);
		if(id == (int)ruid){
			free(acl);
			free(userID);
			return 1;
		}
		*/
		//check usernameID
		if(strlen(userID) >= 2){
			truncUserID = malloc(strlen(userID) - 2);
			for(n = 0; n < (strlen(userID) - 2); n++){
				truncUserID[n] = userID[n + 1];
			}
			truncUserID[n] = '\0';
			
			if(strncmp(pwd->pw_name, truncUserID, strlen(pwd->pw_name)) == 0){
				free(truncUserID);
				free(userID);
				free(acl);
				return 1;
			}
			free(truncUserID);
		}

	}

	free(userID);
	free(acl);
	return -1;

}

int main(int argc, char* argv[]){
	if(argc == 1){
		printf("Need a file to append\n");
		exit(1);
	}
	else if(argc == 2){
		//get privilages
		uid_t ruid;
		uid_t euid;
		uid_t suid;
		getresuid(&ruid, &euid, &suid);
		/*===END PRIVILEGE===*/
		seteuid(ruid); //end privilages
		//test that user gave valid input file
		if(inspectInput(argv[1], euid, ruid) == -1){
			printf("File does not exist or corresponding .acl file does not exist.\n");
			exit(1);
		}

		//test if user is in .ACL
		if(testPermission(argv[1], euid, ruid) != 1){
			printf("You do not have proper permissions to edit this file.\n");
			exit(1);
		}
		/*===END PRIVILEGE===*/
		seteuid(ruid); //end privilages
		//Set up pipe and fork
		pid_t child;
		int fd[2];
		if(pipe(fd)){
			printf("Pipe failed.\n");
			exit(1);
		}
		child = fork();
		if(child > 0){
			//parent case
			//close write descriptor
			close(fd[1]);

			/*BEGIN PRIVILEGE*/
			if(seteuid(euid)){
				printf("setuid: %s\n", strerror(errno));
				exit(-1);
			}
			//open file
			int file = 0;
			if((file = open(argv[1], O_WRONLY | O_APPEND)) < 0){
				printf("%s open: %s\n", argv[1], strerror(errno));
				exit(1);
			}

			int x = 40, i = 0;
			char c = 1;
			char *buf = NULL;
			char *temp = NULL;
			buf = malloc(x);
			while(read(fd[0], &c, 1) != 0){
				buf[i] = c;
				i++;
				if(i == x){
					if(i == INT_MAX){

					}
					//overflow check
					if((x * 2) < x){
						x = INT_MAX;
					}
					else{
						x = x * 2;
					}
					temp = realloc(buf, x);
					if(temp == NULL){
						free(buf);
						exit(-1);
					}

					//make inStr point to temp
					buf = temp;
				}
			}

			write(file, buf, i);
			if(seteuid(ruid)){
				printf("%s\n", strerror(errno));
				free(buf);
				exit(-1);
			}
			/*END PRIVILEGE*/
			free(buf);
		}
		else if(child == 0){
			//child case
			//close rd descriptor
			close(fd[0]);
			//get input from user
			char* in = input();

			write(fd[1], in, strlen(in));

			//close write end
			close(fd[1]);
			free(in);
			exit(1);
		}
		else{
			printf("%s\n", strerror(errno));
			exit(1);
		}

	}
	else{
		printf("Too many arguments\n");
		exit(1);
	}
}
