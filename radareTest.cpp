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
		char* token = strtok(msg, "\n");
		while(token != NULL){
			if(strstr(token, "ret") != NULL){
				char* addr = strtok(token, " \t");
				addr = strtok(NULL, " \t");
				fprintf(out, "%s\n", addr);
				fclose(out);
				return;
			}
			token = strtok(NULL, "\n");
		}
		fclose(out);
	}
}

int main() {
	R2Pipe *r2 = r2pipe_open ("radare2 -q0 ./easy");
	if (r2) {
		r2cmd (r2, "aaa");
		storeFuncs (r2);
		getMainReturnAddr(r2);
		r2pipe_close (r2);
		system("/opt/pin/pin -t obj-intel64/MemTrace.so -- ./easy");
		system("rm funcs.lst; rm main_ret.addr");
		return 0;
	}
	return 1;
}