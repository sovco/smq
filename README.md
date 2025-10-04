# smq
Simple interface for POSIX mq, and a server client type implementation that uses POSIX mq.

# Purpose

Ideally this library is supposed to provide simple and fast IPC.

# Notice

This is under active development.
What is missing?
1) Benchmark under different scenarios 
2) Error handling on numerous occasions (too many clients, message que full and etc)
3) Lacking in testing - *next target*

# Basic Usage

This is a single header library include the header smq.h in your source.
You need to link -rt in your build environement (look in to test/test-build.c for examples) and -lpthread.

In order to know how big is your mq message size and mq maximum message count (both are POSIX mq terms, don't confuse with mq_message) you need to define bellow mentioned definitions.
```c
#define SMQ_MAX_MSG_SIZE 8192// Get this from /proc/sys/fs/mqueue/msgsize_default
#define SMQ_MAX_MSG_COUNT 10// Get this from /proc/sys/fs/mqueue/msg_default
#define SMQ_IMPL
#include <smq/smq.h>
```

This library includes a server and client.
Client sends a smq_message (which consists of a header and payload) and expects a response from the server.
smq_message has an attribute .payload, this is where your application specific data will be stored for transmission.
The size of .payload depends on SMQ_MAX_MSG_SIZE value.

This is an example on how to create a server
```c

void handler_hello(smq_message *request, smq_message *response)
{
    (void)request;
    static const char *msg = "Hello!";
    memcpy(response->payload, msg, strlen(msg));
}

void handler_heyo(smq_message *request, smq_message *response)
{
    (void)request;
    static const char *msg = "heya!";
    memcpy(response->payload, msg, strlen(msg));
}

...

smq_server server = { 0 };
smq_server_create(&server, "/test"); // /test is the path and server identifier
smq_server_add_listener(&server, "-hello", handler_hello); // This will create an associated path /test-hello
smq_server_add_listener(&server, "-heyo", handler_heyo); // This will create an associated path /test-heyo
smq_server_start(&server); // Blocking or smq_server_start_non_blocking(pthread_t *thread, smq_server *server) for non blocking
...
if(!smq_server_ready(&server, 500 /*ms timeout*/)) {
    //server was unable to reach ready state
    return;
}
//After this point server is ready for requests
...

smq_server_destroy(&server); //This will close and unlink the mq path 

```
Here is an example on how to create a client to make a request
```c

smq_client client = { 0 };
smq_message *client_request = malloc(sizeof(*client_request));
smq_message *server_response = malloc(sizeof(*server_response));
smq_client_create(&client, 5 /*this is an id for this particular client must be unique*/, "/test-hello");
smq_client_request(&client, client_request, response, .timeoutms = 1500);
smq_client_destroy(&client); // Will close the mq path
```

Note that many functions of the API return 0 on success, if not they return *errno* but with a minus sign for easy checking.

# Building and Running Tests

```bash
curl -Lo test/nob.h https://raw.githubusercontent.com/tsoding/nob.h/refs/heads/main/nob.h
gcc -o test-build test/test-build.c
./test-build
```
