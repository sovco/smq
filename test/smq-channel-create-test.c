#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stf/stf.h>
#define SMQ_IMPL
#include <smq/smq.h>

// For this to work run these comamnds
// sudo mkdir /dev/mqueue
// mount -t mqueue none /dev/mqueue

bool file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

STF_TEST_CASE(smq_channel, create_with_valid_data_then_destroy)
{
    smq_channel channel = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/channel"
    };
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "failed to get file descriptor");
    smq_channel_destroy(&channel);
}

STF_TEST_CASE(smq_channel, create_with_valid_data_track_mqueue_file)
{
    smq_channel channel = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "failed to get file descriptor");
    STF_EXPECT(file_exists("/dev/mqueue/test"));
    smq_channel_destroy(&channel);
    STF_EXPECT(!file_exists("/dev/mqueue/test"));
}

STF_TEST_CASE(smq_channel, create_with_invalid_path)
{
    smq_channel channel = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "test"
    };
    STF_EXPECT(smq_channel_create(&channel) == -1, .failure_msg = "failed to get file descriptor");
    STF_EXPECT(!file_exists("/dev/mqueue/test"));
    smq_channel_destroy(&channel);
}

STF_TEST_CASE(smq_channel, create_when_exists)
{
    smq_channel channel_a = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    smq_channel channel_b = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    memcpy(&channel_a, &channel_b, sizeof(smq_channel));
    STF_EXPECT(smq_channel_create(&channel_a) != -1, .failure_msg = "failed to get file descriptor channel_a");
    STF_EXPECT(smq_channel_create(&channel_b) != -1, .failure_msg = "failed to get file descriptor channel_b");
    smq_channel_destroy(&channel_a);
    smq_channel_destroy(&channel_b);
}

STF_TEST_CASE(smq_channel_create, when_not_exists_and_no_create)
{
    smq_channel channel = {
        .maxmsgsize = 8192,
        .maxmsgcount = 10,
        .desc = -1,
        .mode = 0666,
        .oflag = O_RDWR | O_CREAT,
        .path = "/test"
    };
    STF_EXPECT(smq_channel_create(&channel) != -1);
    STF_EXPECT(smq_channel_create(&channel) != -1, .failure_msg = "failed to get file descriptor");
    smq_channel_destroy(&channel);
}

int main(void)
{
    return STF_RUN_TESTS();
}
