#include <r_socket.h>
#include <string.h>
#include <vector>
#include <iostream>

static void r2cmd(R2Pipe *r2, const char *cmd) {
	char *msg = r2pipe_cmd (r2, cmd);
	if (msg) {
		printf ("%s\n", msg);
		free (msg);
	}
}

static void storeFuncs(R2Pipe *r2){
    char* msg = r2pipe_cmd(r2, "afl");
	std::vector<char*> lines;
	FILE* out;

    if(msg){
		out = fopen("./funcs.lst", "w");
        char* token = strtok(msg, "\n");
		while(token != NULL){
			if(strlen(token) > 0){
				lines.push_back(token);
			}
			token = strtok(NULL, "\n");
		}

		for(int i = 0; i < lines.size(); i++){
			char* token = strtok(lines[i], " \t");
			std::vector<char*> components;
			while(token != NULL){
				if(strlen(token) > 0){
					components.push_back(token);
				}
				token = strtok(NULL, " \t");
			}
			char* addr = components[0];
			char* name;
			if(strncmp(components[3], "->", 2)){
				name = components[3];
			}
			else{
				name = components[5];
			}

			fprintf(out, "%s\t%s\n", addr, name);
		}
		fclose(out);
    }
}

static void getMainReturnAddr(R2Pipe* r2){
	r2pipe_cmd(r2, "s main");
	char* msg = r2pipe_cmd(r2, "pdf");
	FILE* out;

	if(msg){
		out = fopen("./main_ret.addr", "w");
		char* parsedStrings[2];
		char* token = strtok_r(msg, "\n", parsedStrings);
		while(token != NULL){
			if(strstr(token, "ret") != NULL){
				char* addr = strtok_r(token, " \t", parsedStrings + 1);
				while(strstr(addr, "0x") == NULL)
					addr = strtok_r(NULL, " \t", parsedStrings + 1);
				char* opcode = strtok_r(NULL, " \t", parsedStrings + 1);
				printf("OPCODE: %s\n", opcode);
				if(!strcmp(opcode, "c3")){
					fprintf(out, "%s\n", addr);
					fclose(out);
					return;
				}
			}
			token = strtok_r(NULL, "\n", parsedStrings);
		}
		fclose(out);
	}
}

char* build_cmd(const char* prefix, char* arg){
	size_t len = strlen(prefix) + strlen(arg) + 1;
	char* cmd = (char*) malloc(sizeof(char) * len);
	snprintf(cmd, len, "%s%s", prefix, arg);
	return cmd;
}

char* append_args(char* cmd, char** args){
	char** args_index = args;
	bool requiresQuotes = false;
	size_t len = strlen(cmd) + 1;
	while(*args_index != NULL){
		len += strlen(*args_index) + 1;
		if(strstr(*args_index, " ") != NULL){
			len += 2;
			requiresQuotes = true;
		}
		++args_index;
	}
	char* ret = (char*) malloc(sizeof(char) * len);
	snprintf(ret, len, "%s", cmd);
	args_index = args;
	while(*args_index != NULL){
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
	char** args = argv;
	if(argc < 2){
		fprintf(stderr, "Specify program to launch\n");
		return 2;
	}
	const char* radare_prefix = "radare2 -q0 ";
	char* cmd = build_cmd(radare_prefix, argv[1]);
	printf("CMD RADARE: %s\n", cmd);
	
	R2Pipe *r2 = r2pipe_open (cmd);
	if (r2) {
		free(cmd);
		r2cmd (r2, "aaa");
		storeFuncs (r2);
		getMainReturnAddr(r2);
		r2pipe_close (r2);

		#ifdef DEBUG
			const char* pin_prefix = "/opt/pin/pin -t debug/MemTrace.so -- ";
		#else
			const char* pin_prefix = "/opt/pin/pin -t obj-intel64/MemTrace.so -- ";
		#endif
		char* tmp = build_cmd(pin_prefix, argv[1]);
		cmd = append_args(tmp, &argv[2]);
		free(tmp);

		system(cmd);
		system("rm funcs.lst; rm main_ret.addr");
		return 0;
	}
	return 1;
}