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
		return 1;
	}

	//arg indexing, find each argument in argv
	for(int i = 0; i < argc; i++){
		char *arg = argv[i]; //current arg

		// use str functions to parse strings
		if(strcmp(arg, "--help") == 0){ //strmp returns 0 on match
			help_message();
		}
		else if(strcmp(arg,"--port") == 0){ //listen for next arg if port num was provided
			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --port flag requires an argument.\n");
                return 1; //error
			}//must past error to get to valid assignment
			port_num = atoi(argv[++i]); //convert to int for range checking
			if (port_num <= 0 || port_num > 65535) { //invalid port range
				fprintf(stderr, "Error: Invalid port number %d. Must be between 1 and 65535.\n", port_num);
				return 1;
			}
				settings.server.sin_port = htons(port_num);
				//prefix ++i takes incremented arg a, assigns, then next iter will check for --flags if valid
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
			ip_address = argv[++i];
			domain_name = NULL; //ip specified, domain isn't & "switched off"
		}
		else if( strcmp(arg,"--domain") == 0){
			if (i + 1 >= argc || argv[i+1][0] == '-') {
                fprintf(stderr, "Error: --domain flag requires an argument.\n");
                return 1;
            }
			domain_name = argv[++i]; //pointer to first char
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
		if(perform_full_read(&message, sizeof(message)) <= 0){
			if(settings.running == true){
				fprintf(stderr, "Error: Server closed");
				exit(1);
			}
			break;//not running
		}

		unsigned int message_type = ntohl(message.message_type); //get message type, formatted
		unsigned int message_time = ntohl(message.timestamp);

		// check the message type
			// for message types, print the message and do highlight parsing (if not quiet)
        if(message_type == MESSAGE_RECV){
			//print message: timestamp, username, content FORMAT
			time_t time = message_time;
			//get local time with <time.h>
			struct tm* raw_time_info = localtime(&time); //convert time_t to the tm struct in local time zone

			char time_buffer[32];//buffer for the timestamp (to be formatted)
			char mention[32];//mentioned char array
			sprintf(mention, "@%s", settings.username); //print mention string into mention buffer

			//highlighting feature
			if(settings.quiet == false && strstr(message.message_str, mention)){ //if !quiet and the mention @abc1234 shows
				//strstr - first occurence of string
				printf("[%s] %s: ", time_buffer, message.username);

				//highlight word loop
				char* current = message.message_str;
				char* highlighted_word = strstr(current, mention); //where the first occurence of mention would be
				while(highlighted_word != NULL){
					// highlight-current gets us to focus on the mention part
					//1 = size of char
					//current = start of raw message
					fwrite(current, 1, highlighted_word - current, stdout);
					printf("\a%s%s%s", COLOR_RED, mention, COLOR_RESET); //highlight here

					current = highlighted_word + strlen(mention);

					//change 'index' value
					highlighted_word = strstr(current, mention);
				}
				printf("%s\n", current);

			}
			else{ //should only print that one copy, no repeat messages
				//printf(); //formatted
				printf("[%s] %s: %s\n", time_buffer, message.username, message.message_str);
			}
		} //end of MESSAGE_RECV bracket

			// for system types, print the message in gray with username SYSTEM
		else if(message_type == SYSTEM){
			printf("%s[SYSTEM] %s%s\n", COLOR_GRAY, message.message_str, COLOR_RESET);
        }
			// for disconnect types, print the reason in red with username DISCONNECT and exit
		else if(message_type == DISCONNECT) {
			printf("%s[DISCONNECT] %s%s\n", COLOR_RED, message.message_str, COLOR_RESET);
			exit(0);
        }
			// for anything else, print an error
		else {
			fprintf(stderr, "Message receive fail");
		}
	}

	return NULL;
}

void sig_handler(int signal){
	//Set up signal handlers for SIGINT/SIGTERM using sigaction() at the top of the main function
	//On signal, send a LOGOUT message to server before closing connections and cleaning up / exiting.
	//Clean up all resources (threads, sockets, etc.)
	if(settings.running){
		//send logout
		message_t message = {0};
		message.message_type = htonl(LOGOUT);
		write(settings.socket_fd, &message, sizeof(message));
	}
}

int main(int argc, char *argv[]) {
    // setup sigactions (ill-advised to use signal for this project, use sigaction with default (0) flags instead)
	struct sigaction s_act = {0}; //defaults 0
	s_act.sa_handler = sig_handler; //all purpose signal handler
	sigaction(SIGINT, &s_act, NULL);
	sigaction(SIGTERM, &s_act, NULL);

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
	if (settings.socket_fd == -1){
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
	login_message.message_type = htonl(LOGIN); //must convert to network byte order!

	//username of that message, take where the login_message is (addy), then the size
	strncpy(login_message.username, &login_message, 31); //length - 1

	//put message parts into format
	if ( write( settings.socket_fd, &login_message, sizeof(login_message)) <= 0) {
 		perror("Error: Encountered a write error, Login failed");
		return 1;
	}

    // 4. create and start receive messages thread
	// threads allow us to use mult blocking calls at the same time
	settings.running = true;

	pthread_t thread_id;
	pthread_create(&thread_id, NULL, receive_messages_thread, NULL); //0 if success

	char* line = NULL; //each line is going to be read
	size_t len = 0;//length of line
    // while some condition(s) are true
	while(settings.running == true){
        // read a line from STDIN
		ssize_t bytes_read = getline(&line, &len, stdin); //returns num of chars read, translates to bytes
        // do some error checking (handle EOF, EINTR, etc.)
		if(bytes_read == -1){ //EOF
			//stop loop
			break;
		}
		if(line[bytes_read - 1] == '\n'){ //newline handling - we want to remove it
			line[bytes_read - 1] = '\0'; //replace with null term to end it
		}
		//message length is 1024 bytes, check invalid lengths
		if(bytes_read == 0 || bytes_read > 1023){ //cannot send empty message
			fprintf(stderr, "Error: Invalid message length, cannot be 0 or longer than 1024 characters");
			continue; //shouldn't break the whole thing, continue and try a valid message len next time
		}

		bool valid_message = true; //check valid message before sending
		for(int i = 0; i < bytes_read; i++){
			//isprint() checks if character is printable
			if(isprint(line[i]) == false){
				valid_message = false;
			}
		} //for loop bracket

		// SEND message to server
		if(valid_message == true){
			//create new message struct
			message_t msg = {0};
			msg.message_type = htonl(MESSAGE_SEND); // format and specify message type
			strncpy(msg.message_str, line, 1023); //copy contents to message struct's format field
			write(settings.socket_fd, &msg, sizeof(msg)); //write back to socket
		}
		else{ //if we checked invalid length, then the content itself must be unreadable
			fprintf(stderr, "Error: Invalid message may contain invalid characters");
		}

	} //while loop bracket

    // cleanup and return
	settings.running = false; //flag is recognized to be off, so everything should actually stop running

	//send signal for logout
	sig_handler(0); //LOGOUT signal

	//close from when you connected in step 2
	close(settings.socket_fd);
	settings.socket_fd = -1;

	pthread_join(thread_id, NULL); //wait for thread
	free(line);

	return 0; //success
}
