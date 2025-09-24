#ifndef SMQ_H
#define SMQ_H

#include <mqueue.h>
#include <stdint.h>

#ifndef SMQ_MAX_MSG_SIZE
#define SMQ_MAX_MSG_SIZE 8192// Get this from /proc/sys/fs/mqueue/msgsize_default
#endif// SMQ_MAX_MSG_SIZE

#ifndef SMQ_MAX_MSG_COUNT
#define SMQ_MAX_MSG_COUNT 10// Get this from /proc/sys/fs/mqueue/msg_default
#endif// SMQ_MAX_MSG_COUNT

#define SMQ_STATUS_REQUEST 0x00
#define SMQ_STATUS_RESPONSE 0xFF

typedef struct
{
    long maxmsgsize;
    long maxmsgcount;
    mqd_t desc;
    mode_t mode;
    int oflag;
    char path[255];
} smq_channel;

typedef struct
{
    long timeoutms;
    unsigned int priority;
    bool blocking;
} smq_channel_transmission_options;

typedef struct
{
    uint16_t clientid;
    uint8_t status;
    uint8_t isresponse;
} smq_msg_header;

#define SMQ_HEADER_SIZE sizeof(((smq_msg_header *)0)->clientid) + sizeof(((smq_msg_header *)0)->status) + sizeof(((smq_msg_header *)0)->isresponse)
#define SMQ_PAYLOAD_SIZE (SMQ_MAX_MSG_SIZE) - (SMQ_HEADER_SIZE)

typedef struct
{
    smq_msg_header header;
    char payload[SMQ_PAYLOAD_SIZE];
} smq_message;

typedef struct
{
    smq_channel channel;
    void (*handler)(smq_message *request, smq_message *response);
    struct smq_server_listener *next;
    pid_t subpid;
} smq_server_listener;

typedef struct
{
    smq_server_listener *listeners;
    char name[255];
} smq_server;

typedef struct
{
    smq_channel channel;
    uint16_t id;
} smq_client;

static inline int smq_channel_create(smq_channel *channel);
static inline void smq_channel_close(const smq_channel *channel);
static inline void smq_channel_destroy(const smq_channel *channel);

#define smq_channel_listen(channel, data, size, ...) \
    __smq_channel_listen(channel, data, size, (smq_channel_transmission_options){ .blocking = true, .timeoutms = 0, .priority = 0, __VA_ARGS__ })
static inline int __smq_channel_listen(const smq_channel *channel, char *data, const size_t size, smq_channel_transmission_options options);
static inline int smq_channel_blocking_listen(const smq_channel *channel, char *data, const size_t size);
static inline int smq_channel_timed_listen(const smq_channel *channel, char *data, const size_t size, long timeout);
#define smq_channel_send(channel, data, size, ...) \
    __smq_channel_send(channel, data, size, (smq_channel_transmission_options){ .blocking = true, .timeoutms = 0, .priority = 0, __VA_ARGS__ })
static inline int __smq_channel_send(const smq_channel *channel, const char *data, const size_t size, const smq_channel_transmission_options options);
static inline int smq_channel_blocking_send(const smq_channel *channel, const char *data, const size_t size, int priority);
static inline int smq_channel_timed_send(const smq_channel *channel, const char *data, const size_t size, int priority, long timeout);

static inline int smq_client_create(smq_client *client, uint16_t id, const char *path);
#define smq_client_request(client, request, response, ...) \
    __smq_client_request(client, request, response, (smq_channel_transmission_options){ .blocking = true, .timeoutms = 0, .priority = 0, __VA_ARGS__ })
static inline int __smq_client_request(const smq_client *client, smq_message *request, smq_message *response, smq_channel_transmission_options options);
static inline int smq_client_blocking_request(const smq_client *client, smq_message *request, smq_message *response, const int priority);
static inline int smq_client_timed_request(const smq_client *client, smq_message *request, smq_message *response, const int priority, const long timeout_ms);
static inline void smq_client_destroy(const smq_client *client);

static inline void smq_server_create(smq_server *server, const char *name);
static inline int smq_server_add_listener(smq_server *server, const char *path, void (*handler)(smq_message *request, smq_message *response));
static inline void smq_server_start(smq_server *server);
static inline void smq_server_destroy(smq_server *server);
static inline void __smq_listener_proc(smq_server_listener *listener);

static inline long smq_timestamp_ms();
static inline long smq_timespec_to_timestamp_ms(struct timespec *time);
static inline void smq_abs_timeout(struct timespec *restrict time, long offset_ms);
static inline struct timespec smq_time_now();

#ifdef SMQ_IMPL

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

static inline int smq_channel_create(smq_channel *channel)
{
    struct mq_attr att = { .mq_msgsize = channel->maxmsgsize, .mq_maxmsg = channel->maxmsgcount };
    channel->desc = mq_open(channel->path, channel->oflag, channel->mode, att);
    if (channel->desc == -1) {
        printf("Error in opening channel: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static inline void smq_channel_close(const smq_channel *channel)
{
    mq_close(channel->desc);
}

static inline void smq_channel_destroy(const smq_channel *channel)
{
    smq_channel_close(channel);
    mq_unlink(channel->path);
}

static inline int __smq_channel_send(const smq_channel *channel, const char *data, const size_t size, smq_channel_transmission_options options)
{
    if (options.blocking) {
        return smq_channel_blocking_send(channel, data, size, options.priority);
    }
    return smq_channel_timed_send(channel, data, size, options.priority, options.timeoutms);
}

static inline int smq_channel_blocking_send(const smq_channel *channel, const char *data, const size_t size, int priority)
{
    int ret = mq_send(channel->desc, (char *)data, size, priority);
    ret == -1 ? ret = -errno : ret;
    return ret;
}

static inline int smq_channel_timed_send(const smq_channel *channel, const char *data, const size_t size, int priority, long timeout)
{
    int ret = -1;
    struct timespec abstimeout = smq_time_now();
    smq_abs_timeout(&abstimeout, timeout);
    ret = mq_timedsend(channel->desc, (char *)data, size, priority, &abstimeout);
    ret == -1 ? ret = -errno : ret;
    return ret;
}

static inline int __smq_channel_listen(const smq_channel *channel, char *data, const size_t size, smq_channel_transmission_options options)
{
    return options.blocking ? smq_channel_blocking_listen(channel, data, size) : smq_channel_timed_listen(channel, data, size, options.timeoutms);
}

static inline int smq_channel_blocking_listen(const smq_channel *channel, char *data, const size_t size)
{
    int ret = (int)mq_receive(channel->desc, (void *)data, size + 1, NULL);
    ret == -1 ? ret = -errno : ret;
    return ret;
}

static inline int smq_channel_timed_listen(const smq_channel *channel, char *data, const size_t size, long timeout)
{
    int ret = -1;
    struct timespec abstimeout = smq_time_now();
    smq_abs_timeout(&abstimeout, timeout);
    ret = (int)mq_timedreceive(channel->desc, (void *)data, size + 1, NULL, &abstimeout);
    ret == -1 ? ret = -errno : ret;
    return ret;
}

static inline int __smq_client_request(const smq_client *client, smq_message *request, smq_message *response, smq_channel_transmission_options options)
{
    if (options.blocking) {
        return smq_client_blocking_request(client, request, response, options.priority);
    }
    return smq_client_timed_request(client, request, response, options.priority, options.timeoutms);
}

static inline int smq_client_blocking_request(const smq_client *client, smq_message *request, smq_message *response, const int priority)
{
    request->header.clientid = client->id;
    request->header.isresponse = SMQ_STATUS_REQUEST;
    if (smq_channel_blocking_send(&client->channel, (char *)request, sizeof(*request), priority) != 0) {
        return -1;
    }
    while (response->header.isresponse != SMQ_STATUS_RESPONSE) {
        if (smq_channel_blocking_listen(&client->channel, (char *)response, sizeof(*response)) > 0) {
            if (response->header.isresponse == SMQ_STATUS_RESPONSE && response->header.clientid == client->id) {
                return 0;
            }
            if (smq_channel_blocking_send(&client->channel, (char *)response, sizeof(*response), priority) != 0) {
                continue;
            }
        }
    }
    return -1;
}

static inline int smq_client_timed_request(const smq_client *client, smq_message *request, smq_message *response, const int priority, const long timeout_ms)
{
    request->header.clientid = client->id;
    request->header.isresponse = SMQ_STATUS_REQUEST;
    if (smq_channel_timed_send(&client->channel, (char *)request, sizeof(*request), priority, timeout_ms) != 0) {
        return -1;
    }
    while (response->header.isresponse != SMQ_STATUS_RESPONSE) {
        if (smq_channel_timed_listen(&client->channel, (char *)response, sizeof(*response), timeout_ms) > 0) {
            if (response->header.isresponse == SMQ_STATUS_RESPONSE && response->header.clientid == client->id) {
                return 0;
            }
            if (smq_channel_timed_send(&client->channel, (char *)response, sizeof(*response), priority, timeout_ms) != 0) {
                continue;
            }
        }
    }
    return -1;
}

static inline int smq_client_create(smq_client *client, uint16_t id, const char *path)
{
    client->id = id;
    client->channel = (smq_channel){
        .maxmsgsize = sizeof(smq_message),
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR
    };

    memcpy(&client->channel.path, path, strlen(path) + 1);
    return smq_channel_create(&client->channel);
}

static inline void smq_client_destroy(const smq_client *client)
{
    smq_channel_close(&client->channel);
}

static inline void smq_server_create(smq_server *server, const char *name)
{
    *server = (smq_server){
        .listeners = NULL
    };
    memcpy(&server->name, name, strlen(name) + 1);
}

smq_server_listener **smq_server_get_last_listener(smq_server_listener **listeners)
{
    smq_server_listener **tmp = listeners;
    while (*tmp != NULL) {
        tmp = (smq_server_listener **)&((*tmp)->next);
    }
    return tmp;
}

static inline int smq_server_add_listener(smq_server *server, const char *path, void (*handler)(smq_message *request, smq_message *response))
{
    smq_server_listener **new_listener = smq_server_get_last_listener(&server->listeners);
    *new_listener = malloc(sizeof(smq_server_listener));
    **new_listener = (smq_server_listener){
        .channel = (smq_channel){
          .maxmsgsize = sizeof(smq_message),
          .maxmsgcount = 10,
          .desc = -1,
          .mode = 0666,
          .oflag = O_RDWR | O_CREAT },
        .handler = handler,
        .next = NULL,
        .subpid = 0
    };
    memcpy(&(*new_listener)->channel.path, server->name, strlen(server->name));
    memcpy(&(*new_listener)->channel.path[strlen(server->name)], path, strlen(path) + 1);
    return smq_channel_create(&(*new_listener)->channel);
}

static inline void __smq_listener_proc(smq_server_listener *listener)
{
    static const long timeout_ms = 1500;
    int listen_res = 0;
    int send_res = 0;
    smq_message msgresp = { 0 };
    smq_message msgrecv = { 0 };

    while (true) {
        if ((listen_res = smq_channel_timed_listen(&listener->channel, (char *)&msgrecv, sizeof(msgrecv), timeout_ms)) < 0) {
            switch (listen_res) {
            case -ETIMEDOUT: {
                continue;
            }
            }
        }
        if (msgrecv.header.isresponse == SMQ_STATUS_REQUEST) {
            listener->handler(&msgrecv, &msgresp);
            msgresp.header.isresponse = SMQ_STATUS_RESPONSE;
            while ((send_res = smq_channel_timed_send(&listener->channel, (char *)&msgresp, sizeof(msgresp), 0, timeout_ms)) < 0) {
                switch (send_res) {
                case -ETIMEDOUT: {
                    continue;
                }
                }
            }
            memset(&msgrecv, 0x00, sizeof(msgrecv));
            memset(&msgresp, 0x00, sizeof(msgresp));
            continue;
        }
        while ((send_res = smq_channel_timed_send(&listener->channel, (char *)&msgrecv, sizeof(msgrecv), 0, timeout_ms)) < 0) {
            switch (send_res) {
            case -ETIMEDOUT: {
                continue;
            }
            }
        }
    }
}

static inline int smq_server_spawn_subprocess(smq_server_listener *listener)
{
    int forkres = -1;
    if ((forkres = fork()) == 0) {
        __smq_listener_proc(listener);
    }
    return forkres;
}

static inline void smq_server_start(smq_server *server)
{
    int subpid = -1;
    for (smq_server_listener *lsner = server->listeners; lsner != NULL; lsner = (smq_server_listener *)lsner->next) {
        if ((subpid = smq_server_spawn_subprocess(lsner)) > 0) {
            lsner->subpid = subpid;
            if (setpgid(subpid, server->listeners->subpid) != 0) {
                puts("smq_server_start() could not set gpid");
                exit(EXIT_FAILURE);
            }
        }
    }
}

static inline void smq_server_destroy(smq_server *server)
{
    smq_server_listener *lsner = server->listeners;
    killpg(server->listeners->subpid, SIGKILL);
    while (lsner != NULL) {
        smq_server_listener *tmp = (smq_server_listener *)lsner->next;
        smq_channel_destroy(&lsner->channel);
        free(lsner);
        lsner = tmp;
    }
}

static const long msins = 1000;
static const long nsinms = 1000000;
static const long nsinsec = 1000000000;

static inline long smq_timespec_to_timestamp_ms(struct timespec *time)
{
    return time->tv_sec * msins + time->tv_nsec / nsinms;
}

static inline long smq_timestamp_ms()
{
    struct timespec time = smq_time_now();
    return smq_timespec_to_timestamp_ms(&time);
}

static inline void smq_abs_timeout(struct timespec *restrict time, long offset_ms)
{
    time->tv_nsec += offset_ms * nsinms;
    time->tv_sec += time->tv_nsec / nsinsec;
    time->tv_nsec -= (time->tv_nsec / nsinsec) * nsinsec;
}

static inline struct timespec smq_time_now()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time;
}

#endif// SMQ_IMPL
#endif// SMQ_H
