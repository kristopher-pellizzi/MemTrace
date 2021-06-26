#include <string.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <errno.h>

extern char** environ;

char** append_args(char** cmd, char** args){
	unsigned index = 3;
	char** args_index = args;

	while(*args_index != NULL){
		cmd[index++] = *args_index;
		++args_index;
	}

	return cmd;
}

int main(int argc, char** argv) {
	#ifndef PIN_ROOT
		fprintf(stderr, "Unknown Intel PIN root directory path");
		exit(1);		
	#endif

	#ifndef TOOL_DIR
		fprintf(stderr, "Unknown tool directory path\n");
		exit(2);
	#endif

	char** args = argv;
	if(argc < 2){
		fprintf(stderr, "Specify program to launch\n");
		return 2;
	}

	const char pinRoot[] = PIN_ROOT;
	const char toolDir[] = TOOL_DIR;
	const char pinExecutable[] = "/pin";
	char toolName[] = "MemTrace.so";
	char toolOpt[] = "-t";
	unsigned execLen = strlen(pinRoot) + strlen(pinExecutable) + 1;
	unsigned toolLen = strlen(toolDir) + strlen(toolName) + 1;
	char* executable = (char*) malloc(sizeof(char) * execLen);
	char* toolPath = (char*) malloc(sizeof(char) * toolLen);
	strcpy(executable, pinRoot);
	strcat(executable, pinExecutable);
	strcpy(toolPath, toolDir);
	strcat(toolPath, toolName);

	int argv_num = argc - 1 + 4;
	char** new_argv = (char**) malloc(sizeof(char*) * argv_num);
	new_argv[0] = executable;
	new_argv[1] = toolOpt;
	new_argv[2] = toolPath;
	new_argv[argv_num - 1] = NULL;

	char** cmd = append_args(new_argv, &argv[1]);

	execve(executable, cmd, environ);
	printf("%s\n", "Execve failed");
	printf("Error: %s\n", strerror(errno));
	return 0;
}