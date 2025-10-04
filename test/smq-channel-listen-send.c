#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stf/stf.h>
#define SMQ_IMPL
#include <smq/smq.h>

void *listen(void *channel)
{
    char *msg = malloc(sizeof(smq_message));
    int listenres = smq_channel_listen((smq_channel *)channel, msg, sizeof(smq_message));
    if (listenres == -1) {
        free(msg);
        return NULL;
    }
    return (void *)msg;
}

STF_TEST_CASE(smq_channel_transmission, listening_gets_expected_message)
{
    pthread_t listener;
    const char *expected_msg = "Bla Bla Bla Bla";
    smq_channel channel = {
        .maxmsgsize = sizeof(smq_message),
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    char *rec = NULL;
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "smq_channel_create() was not able to get descriptor");
    STF_EXPECT(pthread_create(&listener, NULL, listen, (void *)&channel) == 0, .failure_msg = "pthread_create failed to spawn thread.");
    STF_EXPECT(smq_channel_blocking_send(&channel, expected_msg, strlen(expected_msg), 0) == 0, .failure_msg = "smq_channel_send failed to send message");
    pthread_join(listener, (void *)&rec);
    STF_EXPECT(strcmp(rec, expected_msg) == 0, .failure_msg = "expected message did not match");
    smq_channel_destroy(&channel);
}

STF_TEST_CASE(smq_channel_transmission, listening_for_a_whole_smq_message)
{
    pthread_t listener;
    const smq_message expected_msg = { .payload = "payload" };
    smq_channel channel = {
        .maxmsgsize = sizeof(smq_message),
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    char *rec = NULL;
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "smq_channel_create() was not able to get descriptor");
    STF_EXPECT(pthread_create(&listener, NULL, listen, (void *)&channel) == 0, .failure_msg = "pthread_create failed to spawn thread.");
    STF_EXPECT(smq_channel_blocking_send(&channel, (char *)&expected_msg, sizeof(expected_msg), 0) == 0, .failure_msg = "smq_channel_send failed to send message");
    pthread_join(listener, (void *)&rec);
    STF_EXPECT(strcmp(((smq_message *)rec)->payload, expected_msg.payload) == 0, .failure_msg = "expected message did not match");
    smq_channel_destroy(&channel);
    free(rec);
}

STF_TEST_CASE(smq_channel_transmission, timed_send_test)
{
    static const int timeout_ms = 500;
    const smq_message expected_msg = { .payload = "payload" };
    smq_channel channel = {
        .maxmsgsize = sizeof(smq_message),
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "smq_channel_create() was not able to get descriptor");
    STF_EXPECT(smq_channel_timed_send(&channel, (char *)&expected_msg, sizeof(expected_msg), 0, timeout_ms) == 0);
    smq_channel_destroy(&channel);
}

STF_TEST_CASE(smq_channel_transmission, timed_listen_test)
{
    static const int timeout_ms = 500;
    struct timespec time_before_call = { 0 };
    struct timespec time_after_call = { 0 };
    char *rec = malloc(sizeof(smq_message));
    smq_channel channel = {
        .maxmsgsize = sizeof(smq_message),
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "smq_channel_create() was not able to get descriptor");
    time_before_call = smq_time_now();
    STF_EXPECT(smq_channel_timed_listen(&channel, rec, sizeof(smq_message), timeout_ms) == -110);
    time_after_call = smq_time_now();
    STF_EXPECT(smq_timespec_to_timestamp_ms(&time_after_call) - smq_timespec_to_timestamp_ms(&time_before_call) >= timeout_ms);
    smq_channel_destroy(&channel);
}

int main(void)
{
    return STF_RUN_TESTS();
}
