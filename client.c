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

//!!!MESSAGE TYPES THAT EACH INTEGER REFERS TO
typedef enum MessageType { //these are just ints at the end of the day
	LOGIN = 0,
	LOGOUT = 1,
	MESSAGE_SEND = 2,
	MESSAGE_RECV = 10,
	DISCONNECT = 12,
	SYSTEM = 13
} message_type_t;

//It is highly recommended to use a packed struct and implement the memory
//layout as defined above and send/receive messages in struct-sized chunks as
//opposed to sending each field individually.
typedef struct __attribute__((packed)) Message {
	//message_type will ideally take from message_type_t
	unsigned int message_type; //unsigned int, first 4 bytes, the int represents many msg types, defined in enums
	unsigned int timestamp; //u int UNIX timestamp, next 4 bytes
	char* username[32]; //next 32 bytes, null terminated string, ex: @kxt5405 with '\0'
	char* message_str[1024]; //next 1024 bytes is a null term msg
} message_t;


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
static settings_t settings = {0}; //defaulted settings

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
	message_t message;

	// while some condition(s) are true
	while(settings.running == true){
		// read message from the server (ensure no short reads)
		//use perform_full_read(), which uses read() unbuffered, but ensures no cutoffs

		unsigned int message_type = ntohl(message.type); //get message type, formatted
		unsigned int message_time = ntohl(message.timestamp);

		// check the message type
			// for message types, print the message and do highlight parsing (if not quiet)
        if(message_type == MESSAGE_RECV){
			//print message: timestamp, username, content

			//highlighting feature
			if(settings.quiet == false){ //if !quiet and the mention @abc1234 shows


			}
			else{ //should only print that one copy, no repeat messages
				//printf(); //formatted
			}


        }
			// for system types, print the message in gray with username SYSTEM
		else if(message_type == SYSTEM){

        }
			// for disconnect types, print the reason in red with username DISCONNECT and exit
		else if(message_type == DISCONNECT) {

        }
			// for anything else, print an error
	}
	return NULL;
}

void sig_handler(int signal){
	//Set up signal handlers for SIGINT/SIGTERM using sigaction() at the top of the main function
	//On signal, send a LOGOUT message to server before closing connections and cleaning up / exiting.
	//Clean up all resources (threads, sockets, etc.)
}

int main(int argc, char *argv[]) {
    // setup sigactions (ill-advised to use signal for this project, use sigaction with default (0) flags instead)
	struct sigaction s_act = {0}; //defaults 0
	s_act.sa_handler = sig_handler; //all purpose signal handler

	//some initial settings set when args are processed
	settings.socket_fd = -1;

	// parse arguments may change settings struct fields
	// function still does something when checked in if statement, so check err
	if(process_args(argc, argv) != 0){
		return 1; //process_args return 1 if failed
	}
    // get username, assigns settings.username within func
	get_username();

	//CLIENT TCP WORKFLOW - CONNECTING FROM CLIENT SIDE
	//Step 1. get address/create a socket, the file descriptor for the network comms

	settings.socket_fd = socket(AF_INET, SOCK_STREAM, 0); //set up a socket (file descriptor) to connect to for later network communication
	//IPv4, TCP = AF_INET, SOCK_STREAM, default protocol is 0
	//SOCK_STREAM is stream based TCP
	if (socket_fd == -1){
		perror("Error: Socket failed");
		return 1;
	}

	// 2. connect to server - need IP addy and the port (formatted)
	// addr - is a pointer to the address structure
	const struct sockaddr *addr = &settings.server;
	if(connect(settings.socket_fd, addr, sizeof(settings.server)) != 0){ //0 = success, -1 = fail
		perror("Error: Connection failed");
		//cleanup - close socket fd
		close(settings.socket_fd);
		return 1;
	}
	// Connected! The handle is now connected between host and server

    // 3. create and send login message
	// send login message
	message_t login_message = {0};
	login_message.type = htonl(LOGIN); //must convert to network byte order!

	//username of that message, take where the login_message is (addy), then the size
	strncpy(login_message.username, &login_message, 31); //length - 1

	//put message parts into format
	if ( write( settings.socket_fd, &login_message, sizeof(login_message)) <= 0) {
 		perror("Encountered a write error, Login failed");
		return 1;
	}

    // 4. create and start receive messages thread

	// threads allow us to use mult blocking calls at the same time



    // while some condition(s) are true
        // read a line from STDIN
        // do some error checking (handle EOF, EINTR, etc.)
        // send message to the server

    // wait for the thread / clean up

    // cleanup and return
	settings.running = false; //flag is recognized to be off, so everything should actually stop running
	//send signal for logout

	//close from when you connected in step 2
	close(settings.socket_fd);

	return 0; //success
}
