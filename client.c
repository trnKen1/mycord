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
    struct sockaddr_in server; //struct contains sin: AF_INET family,int port,IPv4 addr,zero[8](padding)
    bool quiet;
    int socket_fd;
    bool running;
    char username[32];
} settings_t;

static char* COLOR_RED = "\033[31m"; //use for highlighting a username mention
static char* COLOR_GRAY = "\033[90m";
static char* COLOR_RESET = "\033[0m";
static settings_t settings = {0};

int process_args(int argc, char *argv[]) {

	//hold default values into local variables while checking for valid args
	char* ip_address = "127.0.0.1";
	char* domain_name = NULL;
    bool domain_used = false;// Flag whether domain used or not - decides if we ignore --ip
	int port_num = 0; //temp port num, must check validity
	settings.server.sin_family = AF_INET; //we are using IPv4, nothing else
	settings.server.sin_port = htons(8080); //default port 8080, but convert with htons()

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
			port_num = atoi(argv[++a]); //convert to int for range checking
			if (port_num <= 0 || port_num > 65535) { //invalid port range
				fprintf(stderr, "Error: Invalid port number %d. Must be between 1 and 65535.\n", port_num);
				return 1;
			}
				settings.server.sin_port = htons(port_num);
				//prefix ++a takes incremented arg a, assigns, then next iter will check for --flags if valid
				//atoi() converts text from args into int to store into port num
				//htons() then converts that atoi int to network format
		}
		else if(strcmp(arg,"--ip") == 0){
			if (domain_used) { //check if usable in the first place. --domain and --ip can't be specified at the same time
                fprintf(stderr, "Error: Cannot use both --ip and --domain.\n");
                return 1;
            }
  			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --ip flag requires an argument.\n");
                return 1;
			}
			//use ip address argument if no error passed
			ip_address = argv[++a];
			domain_name = NULL; //ip specified, domain isn't & "switched off"
		}
		else if( strcmp(arg,"--domain") == 0){
			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --domain flag requires an argument.\n");
                return 1;
            }
			domain_name = argv[++a]; //pointer to first char
			domain_used = true; //since domain is specified, we will lookup ip address for it
			ip_address = NULL; // "switch" ip_address off
		}
		else if( strcmp(arg,"--quiet") == 0){
			//if not quieted, default is false
			settings.quiet = true;
		}
		else {
			fprintf(stderr, "Error: Unkown argument%s\n", arg);
			return 1;
		}
	}//for loop bracket
}

int get_username() {
	//FILE *popen(const char *command, const char *mode);
	FILE* fp = popen("whoami", "r"); //create a child process and read command, set read

	if(fp == NULL){ //error check
		perror("popen failed");
		return -1;
	}

	char buffer[256];//last byte includes null terminator
	//char *fgets(char *str, int n, FILE *stream);
	if(fgets(buffer, sizeof(buffer), fp) != NULL){ //also get size of buffer, but since it's char, usually 1 byte
		size_t len = strcspn(buffer, '\n'); //define a length before the newline that we don't want to indclude, strcspn finds first position of target character
		buffer[len] = '\0';
		strncpy(settings.username, buffer, sizeof(settings.username) - 1); //strncpy safer alternative to strcpy, sizeof would be 31
		settings.username[sizeof(settings.username)] = '\0'; //make sure you null terminate last char in case it gets overwritten with any other value
	}
	pclose(fp); //close file process after use
	//no need to check status, WIFEXITED, WIFSIGNALED, WEXITSTATUS/WTERMSIG,
	//overkill when whoami is gaurunteed to work
	return 0;
}

ssize_t perform_full_read(void *buf, size_t n) {
	// read() is faster than buffered read, a lot of reading
	size_t count = 0; //count how much read so far

	// read up to n with current count so far, count tracks progress
	while(count < n){
		//ssize_t read() returns num of bytes read, non neg integer
		//bc read returns how many bytes are read so far, add it to count
		count += read(settings.socket_fd, buf + count, n - count); //subtract count from n to remove already ready byte count
		//count will keep adding up

		//hit EOF
		if(count <= 0){
			return count;
		}
	}
	return count;
}

void* receive_messages_thread(void* arg) {
    // while some condition(s) are true
		// read message from the server (ensure no short reads)
		// check the message type
            // for message types, print the message and do highlight parsing (if not quiet)
            // for system types, print the message in gray with username SYSTEM
            // for disconnect types, print the reason in red with username DISCONNECT and exit
            // for anything else, print an error
	//int i = 0;
	//while(arg[i] != NULL){
		//read from server

		//check message type
	//	if(get_msg or smth(arg[i])){

	//	}
	//}
	//i++;
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
	struct sigaction s_act = {0}; //defaults 0
	//s_act.sa_handler =
	//TODO: check for more s_act initializations to use

	//set initial settings
	settings_t settings = { //struct settings will be modified if any args get processed
	.server = NULL,
	.quiet = false,
	.socket_fd = -1,
	.running = false,
	.username = ""
	}

	// parse arguments may change settings struct fields
	// function still does something when checked in if statement, so check err
	if(process_args(argc, argv) != 0){
		return 1; //process_args return 1 if failed
	}
    // get username, assigns settings.username within func
	get_username();


	//CLIENT TCP WORKFLOW - CONNECTING FROM CLIENT SIDE
	//Step 1. create a socket, the file descriptor for the network comms
	struct sockadder_in net4 = settings.server.sin_adder;
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0); //set up a socket (file descriptor) to connect to for later network communication
	//IPv4, TCP = AF_INET, SOCK_STREAM, default protocol is 0
	if (socket_fd == -1){
		return 1;
	}

	// 2. connect to server - need IP addy and the port (formatted)
	connect(settings.socket_fd, settings.server, sizeof(settings.server));
	//TODO: error check?

    // 3. create and send login message

    // 4. create and start receive messages thread

    // while some condition(s) are true
        // read a line from STDIN
        // do some error checking (handle EOF, EINTR, etc.)
        // send message to the server

    // wait for the thread / clean up

    // cleanup and return
}
