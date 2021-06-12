#include <r_socket.h>
#include <string.h>
#include <vector>
#include <iostream>

char* build_cmd(const char* prefix, char* arg){
	size_t len = strlen(prefix) + strlen(arg) + 1;
	char* cmd = (char*) malloc(sizeof(char) * len);
	snprintf(cmd, len, "%s%s", prefix, arg);
	return cmd;
}

char* append_args(const char* cmd, char** args){
	char** args_index = args;
	bool requiresQuotes = false;
	size_t len = strlen(cmd) + 1;
	while(*args_index != NULL){
		len += strlen(*args_index) + 1;
		++args_index;
	}

	char* ret = (char*) malloc(sizeof(char) * len);
	snprintf(ret, len, "%s", cmd);
	args_index = args;
	while(*args_index != NULL){
		if(strstr(*args_index, " ") != NULL){
			len += 2;
			requiresQuotes = true;
		}

		strcat(ret, " ");
		if(requiresQuotes){
			strcat(ret, "\"");
		}
		strcat(ret, *args_index);
		if(requiresQuotes){
			strcat(ret, "\"");
		}
		requiresQuotes = false;
		++args_index;
	}
	printf("CMD: %s\n", ret);
	return ret;
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
	char toolOpt[] = " -t ";
	unsigned strlength = strlen(pinRoot) + strlen(toolDir) + strlen(toolName) + strlen(toolOpt) + strlen(pinExecutable);
	char* pin_prefix = (char*) malloc(sizeof(char) * strlength);
	strcpy(pin_prefix, pinRoot);
	strcat(pin_prefix, pinExecutable);
	strcat(pin_prefix, toolOpt);
	strcat(pin_prefix, toolDir);
	strcat(pin_prefix, toolName);

	char* cmd = append_args(pin_prefix, &argv[1]);

	free(pin_prefix);
	system(cmd);
	return 0;
}