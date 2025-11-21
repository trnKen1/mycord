# mycord Final Programming Assignment

In this assignment, you are going to implement your own client for the `mycord` chat service. You will utilize many aspects of typing, dynamic memory, IO, signals, networking, and mulithreading in C and Linux/Unix. You'll get experience implementing a protocol using sockets on TCP IPv4, with associated error handling and argument parsing.

## Accepting the Assignment

[Click here to accept this assignment](https://classroom.github.com/a/aWwilwLR). Once you accept the invitation, GitHub will create a private repository containing the starter files for the assignment. You will also find an accompanying `README.md`, which is essentially this document you are reading, useful for instructions and further details.

## Submission Details

This assignment has **three parts** graded out of 15 points.
- Part 1 is worth 12.5 points (implementing mycord)
- Part 2 is worth 2.5 points (implementing / improving mycord with GenAI and report)
- Part 3 is worth 5 points (implementing additional features with Gen AI)

Notice the total possible points are 20. Anything over 15 points will be treated as extra credit toward your entire grade. (That is to say, if you do all three parts you will get 5% bonus grade credit (almost half a letter grade!))

## Submission Rules

You must work on and submit Part 1 first before you begin Part 2 or 3. If you submit Part 2 or 3 on canvas before Part 1, your Part 1 score will be a zero. **You MUST commit and submit your version of mycord (Part 1) to Canvas first.** Repeated again:

**YOU MUST IMPLEMENT MYCORD YOURSELF FIRST, COMMIT IT TO GITHUB, AND THEN SUBMIT TO CANVAS BEFORE YOU BEGIN PART 2.** 

If your canvas submission for Part 2 or 3 is BEFORE the date of your Part 1 commit, Part 1 will not be graded!!

You can find more details about Part 2 and 3 below.

# Part 1: Mycord Implementation

In part 1 of this assignment, you will implement by yourself (with no LLM help) the following application.

## Details

You will be implementing a client for the `mycord` service called `client` which has the following help menu:

```
$ ./client --help
usage: ./client [-h] [--port PORT] [--ip IP] [--domain DOMAIN] [--quiet]

mycord client

options:
  --help                show this help message and exit
  --port PORT           port to connect to (default: 8080)
  --ip IP               IP to connect to (default: "127.0.0.1")
  --domain DOMAIN       Domain name to connect to (if domain is specified, IP must not be)
  --quiet               do not perform alerts or mention highlighting

examples:
  ./client --help (prints the above message)
  ./client --port 1738 (connects to a mycord server at 127.0.0.1:1738)
  ./client --domain example.com (connects to a mycord server at example.com:8080)
```

### Argument Details

- **--help**: Display the help message and exit (can be anything, or copied from above).
- **--port**: The port to connect to the mycord server. Default to 8080
- **--ip**: The IPv4 address to connect to the mycord server. Default to 127.0.0.1 (the loopback or localhost address)
- **--domain**: The domain name to connect to the mycord server. Will perform a DNS resolution and find the first IPv4 address to use. If this is specified, then --ip must not be. 
- **--quiet**: When your username is @mentioned, your client will highlight it in red. This disables such feature.

### Mycord Message Protocol

The mycord service exchanges messages in 1,064 byte messages which are broken down as follows:

1. The first 4 bytes represent the message type (unsigned integer)
2. The next 4 bytes represents a UNIX timestamp (unsigned integer)
3. The next 32 bytes represents a null-terminated username string (char[32])
4. The next 1024 bytes represents a null-terminated message string (char[1024])

It is highly recommended to use a packed struct and implement the memory layout as defined above and send/receive messages in struct-sized chunks as opposed to sending each field individually.

### Mycord Message Types

There are 6 message types (3 inbound, 3 outbound) as defined below:

1. `Type 0 LOGIN [OUTBOUND]` 
    - A `LOGIN` message type is an outbound message (only sent to the server) indicating you wish to login. 
    - When sending this message, only the message type and the username field needs to be populated. (leave timestamp and message empty / 0)
      - The username should be non-empty printable ASCII / alphanumeric (comes from whoami or $USER)
        - If the username is invalid you will receive a `DISCONNECT` message
        - If the username is already logged in you will receive a `DISCONNECT` message
    - This message should only be sent once (at client startup)
    - After sending this message, you will start getting past and live messages via `MESSAGE_RECV` messages (see below).
2. `Type 1 LOGOUT [OUTBOUND]`
    - A `LOGOUT` message type is an outbound message (only sent to the server) indicating you wish to logout and close the socket.
    - When sending this message, only the message type needs to be populated. (leave username, timestamp, and message empty / 0)
    - A logout message should be sent when any of the following happens:
        - A local client error (graceful termination)
        - SIGTERM/SIGINT are sent
        - EOF on STDIN (Ctrl+D)
3. `Type 2 MESSAGE_SEND [OUTBOUND]`
    - A `MESSAGE_SEND` message type is an outbound message (only sent to the server) sending your client's message.
    - When sending this message, only the message type and message field needs to be populated. (leave timestamp and username empty / 0)
    - Messages must be a null-terminated non-empty printable ASCII string with no new line terminators between 1 and 1023 characters
        - If the message is invalid you will receive a `DISCONNECT` message
        - If you send more than 5 messages in a second you will receive a `DISCONNECT` message
    - Messages will come from your user's STDIN
4. `Type 10 MESSAGE_RECV [INBOUND]`
    - A `MESSAGE_RECV` message type is an inbound message (only received from the server) recieving a chat message from another user.
    - A type, username, timestamp, and message will be populated
        - Messages may contain @mentions and should be handled
        - Timestamps are UNIX local timestamps
    - Messages should be formatted and displayed to the user in accordance with the `USER` output parameters defined below.
6. `Type 12 DISCONNECT [INBOUND]`
    - A `DISCONNECT` message type is an inbound message (only received from the server) indicating that you made an illegal request and your socket will be closed.
    - A type, username, timestamp, and message will be populated
        - The message is the reason the server disconnected you
        - Timestamps are UNIX local timestamps
    - You should NOT send any more data over the network after a `DISCONNECT`, not even a `LOGOUT` as your socket will be closed by the server.
    - Messages should be formatted and displayed to the user in accordance with the `DISCONNECT` output parameters defined below.
7. `Type 13 SYSTEM [INBOUND]`
    - A `SYSTEM` message type is an inbound message (only received from the server) recieving a system message (user logged in, user logged out, global message, etc...)
    - A type, username, timestamp, and message will be populated
        - The username will always be `SYSTEM`
        - The message is the server message, will never contain a mention
        - Timestamps are UNIX local timestamps
    - Messages should be formatted and displayed to the user in accordance with the `SYSTEM` output parameters defined below.


### Client Output Details

1. `USER`
    - `MESSAGE_RECV` message types should be displayed to the user in the following format:
    
        `[%Y-%m-%d %H:%M:%S] {user}: {message}`

    - An example message with timestamp `1763665259`, username `abc123`, and message `This is an old message` would be printed as:
        
        `[2025-11-20 14:00:59] abc123: This is an old message`

    - Timestamps can be parsed from int to time using the `strftime` function found in `<time.h>`. 
        - The time format is `%Y-%m-%d %H:%M:%S`.
    - User messages may also contain mentions, where your username is prepended with an `@`. For example, if your username was `abc123`, a mention would be the string `@abc123`. All instances of mentions shall be highlighted in red and prepended with the bell ASCII `\a` to give the user a notification of their mention. For example, assuming your username was `abc123`, a message with timestamp `1763665259`, username `xyz987`, and message `This is a mention @abc123 message` would be printed as:
        
        `[2025-11-20 14:00:59] xyz987: This is a mention \a\033[31m@abc123\033[0m message`

2. `DISCONNECT`
    - `DISCONNECT` message types should be displayed in all red highlight to the user in the following format:
    
        `[DISCONNECT] {message/reason}`

    - An example disconnect message with timestamp `1763665259`, username `abc123`, and message `You were kicked` would be printed as:
        
        `\033[31m[DISCONNECT] You were kicked\033[0m`

    - where `\033[31m` is the ANSI red color and `\033[0m` is the reset color
3. `SYSTEM`
    - `SYSTEM` message types should be displayed in all gray highlight to the user in the following format: 
        
        `[SYSTEM] {message}`

    - An example system message with timestamp `1763665259`, username `SYSTEM`, and message `xyz987 logged in` would be printed as:
        
        `\033[90m[SYSTEM] xyz987 logged in\033[0m`

    - where `\033[90m` is the ANSI gray color and `\033[0m` is the reset color


### Program Requirements

Your `client` program should:

1. **Parse arguments**: Accept optional flags for `--help`, `--port`, `--ip`, `--domain`, and `--quiet`
   - If there are any errors parsing arguments, you should print to `STDERR` with the text `Error: <your message>` and exit main with a non-zero exit code.
2. **Get username**: Retrieve the username from the output of `whoami` or `echo $USER`
   - The username will be used for login and must be valid (non-empty, printable ASCII/alphanumeric)
3. **Connect to server**: Establish a TCP IPv4 connection to the specified server (default: 127.0.0.1:8080)
   - Support DNS resolution when `--domain` is specified (use the first IP available)
   - Handle connection errors gracefully with error messages to STDERR
4. **Send LOGIN message**: Immediately after connecting, send a LOGIN message with your username
5. **Receive messages**: After sending a login, you should spawn a thread to continuously receive and process messages from the server
   - Handle MESSAGE_RECV messages (live chat messages)
   - Handle SYSTEM messages (server notifications)
   - Handle DISCONNECT messages (server-initiated disconnection)
   - Format output; Display messages according to the specified format:
      - USER messages: `[YYYY-MM-DD HH:MM:SS] username: message`
         - Highlight @mentions in red and prepend with the bell `\a` (unless `--quiet` is specified)
      - SYSTEM messages: `[SYSTEM] message` (in gray)
      - DISCONNECT messages: `[DISCONNECT] reason` (in red)
6. **Send messages**: In the main thread, and after spawning the recieving thread, continually read lines of user input from STDIN and send MESSAGE_SEND messages to the server
   - Validate messages before sending (non-empty, printable ASCII, 1-1023 characters, no newlines)
   - You can use the `isprint(char c)` function to check if a character is printable ASCII or not
   - Never send invalid messages that would cause a DISCONNECT, tell the user instead via STDERR of their fault.
7. **Handle signals**: Gracefully handle SIGINT/SIGTERM by sending LOGOUT and cleaning up
   - Handle EOF on STDIN (Ctrl+D) by sending LOGOUT and exiting
   - Hint: use a global variable / settings variable to flag when the client should shutdown
8. **Error handling**: All errors should be printed to STDERR with the prefix `Error: `
   - Never receive a DISCONNECT due to invalid user input (validate before sending)

### Implementation Details

You may use any library or function you like to implement this assignment. That said, to guide your approach, the solution only uses these functions and syscalls:
- Dynamic memory functions: `sizeof()`, `free()`
- Socket functions: `socket()`, `connect()`, `read()`, `write()`, `close()`
- Network functions: `inet_pton()`, `gethostbyname()`, `htons()`, `htonl()`, `ntohl()`
- Threading: `pthread_create()`, `pthread_join()`
- Signal handling: `sigaction()`, `raise()`
- Time functions: `localtime()`, `strftime()`
- String functions: `strcmp()`, `strcpy()`, `strlen()`, `strstr()`, `strchr()`, `strerror()`, `isprint()`, `sprintf()`
- Process functions: `popen()`, `pclose()`
- Input/Output: `getline()`, `fprintf()`, `printf()`, `fwrite()`, `feof()`
- State: `exit()`

Feel free to look at the man pages for any of these functions to inspect their function signature and use case.

#### Recommended Implementation Approach

If you are stuck where to begin, follow the below guide to build your implementation:

1. **Parse and verify CLI arguments**
   - Parse arguments into a structure to store connection settings (IP/domain, port, quiet flag)
   - Handle `--help` flag to display usage and exit
   - Resolve DNS (if domain specified)
      - Validate that `--ip` and `--domain` are not both specified
      - Use `gethostbyname()` to resolve domain names to IPv4 addresses
      - Pick the first IP address from the result
- All errors or failures should be printed to STDERR and start with "Error: <message>"

2. **Gather username**
   - Get the logged in username from the `whoami` command or the `$USER` environment variable
   - This will be the user's username for the connection
   - Validate that the username is non-empty and printable ASCII/alphanumeric

3. **Connect to server**
   - Create an IPv4 TCP socket
   - Attempt to connect to the server
   - Handle errors gracefully

4. **Send LOGIN message**
   - After connecting, immediately send a LOGIN message with your username

5. **Spawn receive thread**
   - Create a pthread for receiving/parsing messages from the server
   - Add the associated call to wait for the thread to finish afterward
   - This thread will continuously read messages and parse/handle/output them to STDOUT
      - Read structured messages from the server
      - Handle different message types:
          - **MESSAGE_RECV**: Format as `[YYYY-MM-DD HH:MM:SS] username: message`
             - Parse for @mentions and highlight in red (unless `--quiet`)
             - Prepend bell character (`\a`) before highlighted mentions
          - **SYSTEM**: Format as `[SYSTEM] message` in gray
          - **DISCONNECT**: Format as `[DISCONNECT] reason` in red, then cleanup and exit
      - Continue reading until connection closes or some global condition is not met

6. **Main thread: Handle STDIN**
   - Back in the main thread, after spawning the receive thread but before waiting for the thread to finish...
   - Read lines from STDIN using `getline()`
   - Validate messages before sending:
     - Remove newline characters
     - Check length (1-1023 characters)
     - Check for printable ASCII characters only
   - If validation fails, print error to STDERR and continue (don't send)
   - Send valid messages as MESSAGE_SEND to the server
   - Handle EOF (Ctrl+D) by sending LOGOUT and exiting

7. **Handle signals**
   - Set up signal handlers for SIGINT/SIGTERM using `sigaction()` at the top of the main function
   - On signal, send a LOGOUT message to server before closing connections and cleaning up / exiting.
   - Clean up all resources (threads, sockets, etc.)

8. **Cleanup / Memory Leaks**
    - Close socket file descriptor
    - Free any memory you may have
    - Exit with appropriate return code
    - Feel free to test with Valgrind

### Recommendations, hints, and tips

- **Use pthreads**: The recommended approach is to use pthreads for handling concurrent message receiving and sending. This allows the client to receive messages from the server while simultaneously reading user input from STDIN. You can use forks and multiprocessing, but it is not recommended for the client.

- **Validate user input**: You should never receive a DISCONNECT due to user input. Always validate messages before sending them to the server:
  - Check that the message is non-empty
  - Check that the message length is between 1 and 1023 characters
  - Check that all characters are printable ASCII
  - Remove newline characters before sending
  - If validation fails, print an error to STDERR and continue (do not send the message)

- **Error handling**: All errors should be printed to STDERR and prepended with "Error: ". This includes:
  - Argument parsing errors
  - Connection errors
  - Socket errors
  - Message validation errors
  - Any other runtime errors

- **Username retrieval**: The username will come from the output of `whoami` or `echo $USER`.

- **Graceful Termination**: Pressing Ctrl+D (STDIN EOF) or Ctrl+C (SIGINT) or kill -TERM <client> (SIGTERM) should always terminate the application with no errors shown and the process not hanging.

## Assignment File Structure

### **client.c**

**This is the only file you should be modifying in this assignment.**

For all parts of this assignment, you will be writing your code in this single source file. To give you some guidance, the source file for Part 1 shouldn't be too much longer than 300 lines of code, with comments. 100 of that are "boilerplate" CLI parsing, struct definitions, name/username resolution.

### **server.py**

This is a Python server implementation provided to you for testing your client locally. You can run it with:

```bash
python3 server.py
```

The server will start locally with a port chosen based off of your username. This should avoid the case where multiple people have to fight over port reservations. If you find your port is in use, the type in a manual port as an argument like so: `python3 server.py 1234` 

The server will print to you what it is doing and what its state is. This should help you debug. Feel free to edit the server code to your liking if you want to add more print statements.

When you have tested your implementation on your server, you can connect to the classroom server at `mycord.devic.dev` to chat with others :)

### **messages.log**

This file is generated by the server and is used as a history for the messages that are sent. Feel free to edit as you please. One will be generated for you at startup with dummy data and at least one mention for testing.

### **README.md**

The markdown document you are reading now.

---

## Testing Your Code

**Note: There is no test script for this project. You will need to test your own code.**

When you finish a function or reach a stopping point, you can compile and test your code using the following commands.

### Compiling

In the project directory, run this command in your terminal to compile your code:

```shell
gcc client.c -o client -pthread
```

If your code has no syntax errors, an executable file called `client` will appear in your directory. If there are syntax errors, carefully read the errors or warnings to understand what line of code is causing the syntax error.

**Note:** You must link with the pthread library using the `-pthread` flag.

### Testing / Running

You can test your program yourself by connecting to a server. You have two options:

1. **Test locally using the provided server:**
   ```shell
   # Terminal 1: Start the server
   python3 server.py
   # look at what port it started on (let's assume it's 1234)
   
   # Terminal 2: Run your client on the same machine
   ./client --port 1234
   
   # Terminal 3: Run your client on a different machine
   ./client --domain e5-cse-135-08.cse.psu.edu --port 1234
   ```

2. **Test on the classroom server:**
   ```shell
   ./client --domain mycord.devic.dev
   ```

You should test various scenarios:
- Valid message sending
- Invalid message handling (too long, empty, non-printable characters)
- Signal handling (Ctrl+C, SIGTERM)
- EOF handling (Ctrl+D)
- Multiple clients connecting simultaneously
- Message formatting and mention highlighting
- System messages
- Disconnect handling

---

## Part 1 Rubric

Your grade for part 1 will be broken down into the following categories
- 2pts | Comment Quality
- 1pts | Commits / Evidence of work
- 1pts | Proper CLI handling / parsing / error messages
- 0.5pts | Help Menu
- 1 pts | Capable of LOGIN
- 1 pts | Capable of MESSAGE_SEND
- 1 pts | Proper error handling of MESSAGE_SEND (never receives a disconnect)
- 1 pts | Capable of MESSAGE_RECV
- 0.5 pts | Proper output formats
- 0.5 pts | Proper output highlighting
- 0.5 pts | Proper disconnect handling
- 1 pts | EOF/SIGINT/SIGTERM handling
- 0.5 pts | Sent a message to the classroom server
- 1 pts | Never crashes
- \-\-\-\-\-\-\-\-\-\-\-
- 12.5 | Total



---

# Part 2: AI Assistance

In Part 2 of this assignment, you will use GenAI or an LLM of your choice (ChatGPT, Claude, Copilot, Gemini, DeepSeek, Llama, Grok, ...) to help you with:
   - Code quality and cleanliness
   - Performance optimizations
   - Better error handling
   - Code organization and structure
   - Reimplement the assignment entirely

Using it's suggestions, implement them on top of your Part 1 assignment and submit a commit ID on Canvas. You will then submit a 500 word document about your co-journey (in Canvas). Discuss how well the AI performed, and how fast you were able to complete the project. Did it work the first time? The second? What parts of the commit were from AI, was it useful? Did you understand what it was doing?

## Part 2 Rubric

Your grade for part 2 will be broken down into the following categories
- 1pts | AI-assisted code/optimization work per the rubric from Part 1 (i.e. we will test your AIs code on part 1, a 12.5/12.5 = 1pts)
- 1.5pts | 500 word report about your journey with AI (0.5pts poor report | 1pt average report | 1.5pts excellent report)
- \-\-\-\-\-\-\-\-\-\-\-
- 2.5 | Total

---

# Part 3: TUI Interface (Extra Credit)

After completing Part 1 or Part 2, add a `--tui` flag to your client that enables a Text User Interface (TUI) instead of the standard STDIN/STDOUT interface.

Not sure what a TUI is? Think back to using GDB, VIM, or any other terminal based editor. If you want inspiration you can see what an LLM TUI looks like [here](https://www.youtube.com/watch?v=UAK6dQbnknE).

Your extra credit task will be to, using an LLM, create a TUI for the mycord client with the following requirements:
- Input should always be typed at the bottom of the screen
- When messages arrive, the input field should not be split across two/multiple lines
- The interface should be redrawn to accommodate new messages while keeping input at the bottom
- Messages should scroll up as new ones arrive
- The arrow keys should be used to scroll up and down the history of messages
- TUI is built using ANSI sequences or builtin libraries only

The implementation or style (colors, format, boxes, etc...) is entirely up to you and is a creative element. You will get the full 5% bonus if your TUI satisfies the above requirements. 

## Part 3 Rubric

Your grade for part 3 will be graded into one of the following categories:
- 0 pts | TUI does not work / no commit / input lines are broken by output
- 4 pts | TUI works (no broken input lines, scrolling works)
- 5 pts | TUI is a masterclass, well thought out and easy to use / visually pleasing (colors, styles, etc...)

---

# Turnin

## Part 1 Submission

Go to Canvas, start the quiz, and submit a commit ID as a single line in the submission box for Part 1.

## Part 2 Submission

After completing Part 2 (AI-assisted improvements), submit your updated commit ID to Canvas for Part 2 along with your report.

## Part 3 Submission

After completing Part 3 (TUI interface), submit your final commit ID to Canvas for Part 3.
