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

#if __STDC_VERSION__ > 201112L || __STDC_NO_ATOMICS__ == 0
#define SMQ_HAS_ATOMICS
#endif

#ifdef SMQ_HAS_ATOMICS
#include <stdatomic.h>
#endif// SMQ_HAS_ATOMICS

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
    long timeout_ms;
    unsigned int priority;
} smq_channel_transmission_options;

#define SMQ_STATUS_REQUEST 0x0F
#define SMQ_STATUS_RESPONSE 0xF0

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

typedef struct smq_server_t smq_server;
typedef struct smq_server_listener_t smq_server_listener;

struct smq_server_listener_t
{
    smq_channel channel;
    void (*handler)(smq_message *request, smq_message *response);
    struct smq_server_listener *next;
    smq_server *parent_server;
    pthread_t thread;
#ifdef SMQ_HAS_ATOMICS
    atomic_bool is_listening;
#else
    bool is_listening;
#endif
};

struct smq_server_t
{
    smq_server_listener *listeners;
    char name[255];
#ifdef SMQ_HAS_ATOMICS
    atomic_bool running;
#else
    bool running;
#endif
};

typedef struct
{
    smq_channel channel;
    uint16_t id;
} smq_client;

static inline int smq_channel_create(smq_channel *channel);
static inline void smq_channel_close(const smq_channel *channel);
static inline void smq_channel_destroy(const smq_channel *channel);

#define smq_channel_listen(channel, data, size, ...) \
    __smq_channel_listen(channel, data, size, (smq_channel_transmission_options){ __VA_ARGS__ })
static inline int __smq_channel_listen(const smq_channel *channel, char *data, const size_t size, smq_channel_transmission_options options);
static inline int smq_channel_blocking_listen(const smq_channel *channel, char *data, const size_t size);
static inline int smq_channel_timed_listen(const smq_channel *channel, char *data, const size_t size, long timeout);
#define smq_channel_send(channel, data, size, ...) \
    __smq_channel_send(channel, data, size, (smq_channel_transmission_options){ __VA_ARGS__ })
static inline int __smq_channel_send(const smq_channel *channel, const char *data, const size_t size, const smq_channel_transmission_options options);
static inline int smq_channel_blocking_send(const smq_channel *channel, const char *data, const size_t size, int priority);
static inline int smq_channel_timed_send(const smq_channel *channel, const char *data, const size_t size, int priority, long timeout);

static inline int smq_client_create(smq_client *client, uint16_t id, const char *path);
#define smq_client_request(client, request, response, ...) \
    __smq_client_request(client, request, response, (smq_channel_transmission_options){ __VA_ARGS__ })
static inline int __smq_client_request(const smq_client *client, smq_message *request, smq_message *response, smq_channel_transmission_options options);
static inline int smq_client_blocking_request(const smq_client *client, smq_message *request, smq_message *response, const int priority);
static inline int smq_client_timed_request(const smq_client *client, smq_message *request, smq_message *response, const int priority, const long timeout_ms);
static inline void smq_client_destroy(const smq_client *client);

static inline void smq_server_create(smq_server *server, const char *name);
static inline int smq_server_add_listener(smq_server *server, const char *path, void (*handler)(smq_message *request, smq_message *response));
static inline bool smq_server_is_running(smq_server *server);
static inline void smq_server_start(smq_server *server);
static inline int smq_server_start_non_blocking(pthread_t *thread, smq_server *server);
static inline bool smq_server_ready(smq_server *server, long timeout_ms);
static inline void smq_server_stop(smq_server *server);
static inline void smq_server_destroy(smq_server *server);

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
#include <sched.h>
#include <pthread.h>

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
    return options.timeout_ms > 0 ? smq_channel_timed_send(channel, data, size, options.priority, options.timeout_ms) : smq_channel_blocking_send(channel, data, size, options.priority);
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
    return options.timeout_ms > 0 ? smq_channel_timed_listen(channel, data, size, options.timeout_ms) : smq_channel_blocking_listen(channel, data, size);
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
    return options.timeout_ms > 0 ? smq_client_timed_request(client, request, response, options.priority, options.timeout_ms) : smq_client_blocking_request(client, request, response, options.priority);
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
#ifdef SMQ_HAS_ATOMICS
    atomic_init(&server->running, false);
#else
    server->running = false;
#endif
}

static inline smq_server_listener **smq_server_get_last_listener(smq_server_listener **listeners)
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
        .thread = 0,
        .parent_server = server,
        .is_listening = false
    };
    memcpy(&(*new_listener)->channel.path, server->name, strlen(server->name));
    memcpy(&(*new_listener)->channel.path[strlen(server->name)], path, strlen(path) + 1);
    return smq_channel_create(&(*new_listener)->channel);
}


static inline bool __smq_server_listener_is_ready(smq_server_listener *listener)
{
    bool res = false;
#ifdef SMQ_HAS_ATOMICS
    res = atomic_load(&listener->is_listening);
#else
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    res = listener->is_listening;
    pthread_mutex_unlock(&mutex);
#endif
    return res;
}

static inline void __smq_server_listener_modify_readiness(smq_server_listener *listener, bool new_state)
{
#ifdef SMQ_HAS_ATOMICS
    atomic_store(&listener->is_listening, new_state);
#else
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    listener->is_listening = new_state;
    pthread_mutex_unlock(&mutex);
#endif
}

static inline void *__smq_listener_proc(void *listener_)
{
    static const long timeout_ms = 700;
    int listen_res = 0;
    int send_res = 0;
    smq_message *msgresp = malloc(sizeof(*msgresp));
    smq_message *msgrecv = malloc(sizeof(*msgrecv));
    smq_server_listener *listener = (smq_server_listener *)listener_;
    __smq_server_listener_modify_readiness(listener, true);
    while (smq_server_is_running(listener->parent_server)) {
        if ((listen_res = smq_channel_timed_listen(&listener->channel, (char *)msgrecv, sizeof(*msgrecv), timeout_ms)) < 0) {
            switch (listen_res) {
            case -ETIMEDOUT: {
                continue;
            }
            }
        }
        if (msgrecv->header.isresponse == SMQ_STATUS_REQUEST) {
            listener->handler(msgrecv, msgresp);
            msgresp->header.isresponse = SMQ_STATUS_RESPONSE;
            while ((send_res = smq_channel_timed_send(&listener->channel, (char *)msgresp, sizeof(*msgresp), 0, timeout_ms)) < 0) {
                switch (send_res) {
                case -ETIMEDOUT: {
                    continue;
                }
                }
            }
            memset(msgrecv, 0x00, sizeof(*msgrecv));
            memset(msgresp, 0x00, sizeof(*msgresp));
            continue;
        }
        while ((send_res = smq_channel_timed_send(&listener->channel, (char *)msgrecv, sizeof(*msgrecv), 0, timeout_ms)) < 0) {
            switch (send_res) {
            case -ETIMEDOUT: {
                continue;
            }
            }
        }
    }
    free(msgresp);
    free(msgrecv);
    __smq_server_listener_modify_readiness(listener, false);
    return NULL;
}

static inline void __smq_server_modify_running_state(smq_server *server, bool new_state)
{
#ifdef SMQ_HAS_ATOMICS
    atomic_store(&server->running, new_state);
#else
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    server->running = new_state;
    pthread_mutex_unlock(&mutex);
#endif
}

static inline bool smq_server_is_running(smq_server *server)
{
    bool res = false;
#ifdef SMQ_HAS_ATOMICS
    res = atomic_load(&server->running);
#else
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);
    res = server->running;
    pthread_mutex_unlock(&mutex);
#endif
    return res;
}

static inline int smq_server_spawn_subprocess(smq_server_listener *listener)
{
    return pthread_create(&listener->thread, NULL, __smq_listener_proc, (void *)listener);
}

static inline void *__smq_server_run(void *server)
{
    smq_server_start((smq_server *)server);
}

static inline int smq_server_start_non_blocking(pthread_t *thread, smq_server *server)
{
    return pthread_create(thread, NULL, __smq_server_run, (void *)server);
}

static inline void smq_server_start(smq_server *server)
{
    if (server->listeners == NULL) return;
    smq_server_listener *first_listener = (smq_server_listener *)server->listeners;
    if (smq_server_is_running(server) == true) {
        return;
    }
    __smq_server_modify_running_state(server, true);
    for (smq_server_listener *lsner = (smq_server_listener *)first_listener->next; lsner != NULL; lsner = (smq_server_listener *)lsner->next) {
        if (smq_server_spawn_subprocess(lsner) != 0) {
            puts("smq_server_start unable to spawn thread.");
            return;
        }
    }
    (void)__smq_listener_proc((void *)first_listener);
}

static inline bool smq_server_ready(smq_server *server, long timeout_ms)
{
    if (server->listeners == NULL) {
        return false;
    }
    long abs_timeout = smq_timestamp_ms() + timeout_ms;

    for (smq_server_listener *lsner = server->listeners; lsner != NULL; lsner = (smq_server_listener *)lsner->next) {
        long time_now = smq_timestamp_ms();
        while (!__smq_server_listener_is_ready(lsner) || abs_timeout > time_now) {
            time_now = smq_timestamp_ms();
            sched_yield();
        }
        if (time_now > abs_timeout) {
            return false;
        }
    }
    return true;
}

static inline void smq_server_stop(smq_server *server)
{
    __smq_server_modify_running_state(server, false);
    if (server->listeners == NULL) return;
    if (server->listeners->next == NULL) {
        while (__smq_server_listener_is_ready(server->listeners)) {
            sched_yield();
        }
        return;
    }

    for (smq_server_listener *lsner = (smq_server_listener *)server->listeners->next; lsner != NULL; lsner = (smq_server_listener *)lsner->next) {
        if (pthread_join(lsner->thread, NULL) != 0) {
            puts("smq_server_stop phtread unable to join");
            return;
        }
    }
}

static inline void smq_server_destroy(smq_server *server)
{
    smq_server_listener *lsner = server->listeners;
    smq_server_stop(server);
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
