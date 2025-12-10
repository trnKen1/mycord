#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <stdint.h>

// typedef enum MessageType { ... } message_type_t;
// typedef struct __attribute__((packed)) Message { ... } message_t;
typedef struct Settings {
    struct sockaddr_in server;
    bool quiet;
    int socket_fd;
    bool running;
    char username[32];
} settings_t;

static char* COLOR_RED = "\033[31m";
static char* COLOR_GRAY = "\033[90m";
static char* COLOR_RESET = "\033[0m";
static settings_t settings = {0};

int process_args(int argc, char *argv[]) {

	// ERROR CHECKING
	if(argv == NULL || argc == 0){
		return -1;
	}

	//a for arg indexing, find each argument in argv
	for(int a = 0; a < argc; a++){
		char *arg = argv[a]; //current arg

		// use str functions to parse strings
		if(strcmp(arg, "--help") == 0){ //strmp returns 0 on match
			help_message();
		}
		else if(strcmp(arg,"--port") == 0){ //listen for next arg if port num was provided
			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --port flag requires an argument.\n");
                return 1; //error
            }//must past error to get to valid assignment
			settings.socket_fd = argv[++a]; //prefic ++a takes incremented arg a, assigns, then next iter will check for --flags if valid
		}
		else if( strcmp(arg,"--ip") == 0){
  			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --ip flag requires an argument.\n");
                return 1;
            }
			//use ip address argument if no error passed
			settings.ip = argv[++a];
		}
		else if( strcmp(arg,"--domain") == 0){
			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --domain flag requires an argument.\n");
                return 1;
            }
			domain = argv[++a];
		}
		else if( strcmp(arg,"--quiet") == 0){
			//quiet doesn't require an arg field, just toggle on/off
			//quiet: do not perform alerts or mention highlighting
			if(settings.quiet == false){
				settings.quiet = true;
			}
			else settings.quiet = false;
		}

	}

	return -1;
}

int get_username() {
    //use popen() to read whoami

	return -1;
}

void handle_signal(int signal) {
    //sigint

	//sighup
	//sigterm
	//

	return;
}

ssize_t perform_full_read(void *buf, size_t n) {
    return -1;
}

void* receive_messages_thread(void* arg) {
    // while some condition(s) are true
		// read message from the server (ensure no short reads)
		// check the message type
            // for message types, print the message and do highlight parsing (if not quiet)
            // for system types, print the message in gray with username SYSTEM
            // for disconnect types, print the reason in red with username DISCONNECT and exit
            // for anything else, print an error
	//while(){
		//1
		//2
		//2abcd
	//}

}

void help_message(){
	printf("age: ./client [-h] [--port PORT] [--ip IP] [--domain DOMAIN] [--quiet]");
	printf("mycord client");
	printf("\n	 options:");
	printf("--help                show this help message and exit");
	printf("--port PORT           port to connect to (default: 8080)");
	printf("--ip IP               IP to connect to (default: \"127.0.0.1\")");
	printf("--domain DOMAIN       Domain name to connect to (if domain is specified, IP must not be)");
	printf("--quiet               do not perform alerts or mention highlighting");
	printf("\n	examples:");
	printf("./client --help (prints the above message)");
	printf("./client --port 1738 (connects to a mycord server at 127.0.0.1:1738)" );
	printf("./client --domain example.com (connects to a mycord server at example.com:8080)");
}

int main(int argc, char *argv[]) {
    // setup sigactions (ill-advised to use signal for this project, use sigaction with default (0) flags instead)
	//set initial settings
	settings.quiet;
	settings.socket_fd() = 8080; //default port is 8080
	settings.running;
	// parse arguments

    // get username
	settings.username = getusername();
    // create socket

    // connect to server

    // create and send login message

    // create and start receive messages thread

    // while some condition(s) are true
        // read a line from STDIN
        // do some error checking (handle EOF, EINTR, etc.)
        // send message to the server

    // wait for the thread / clean up

    // cleanup and return
}
