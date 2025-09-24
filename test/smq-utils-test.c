#include <stdlib.h>
#include <stf/stf.h>
#define SMQ_IMPL
#include <smq/smq.h>

STF_TEST_CASE(smq_utils, timestamp_returns_something)
{
    STF_EXPECT(smq_timestamp_ms() != 0, .failure_msg = "timestamp_ms() returned 0");
}

STF_TEST_CASE(smq_utils, abs_timeout_addition_1000ms_match)
{
    static const long ms_offset = 1000;
    struct timespec now_org = smq_time_now();
    struct timespec now_offset = now_org;
    smq_abs_timeout(&now_offset, ms_offset);
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) > smq_timespec_to_timestamp_ms(&now_org), .failure_msg = "abs timeout with offset is not greater than smq_time_now()");
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) - smq_timespec_to_timestamp_ms(&now_org) == ms_offset, .failure_msg = "difference between abs timeout and smq_time_now() is not as expected");
}

STF_TEST_CASE(smq_utils, abs_timeout_addition_2500ms_match)
{
    static const long ms_offset = 2500;
    struct timespec now_org = smq_time_now();
    struct timespec now_offset = now_org;
    smq_abs_timeout(&now_offset, ms_offset);
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) > smq_timespec_to_timestamp_ms(&now_org), .failure_msg = "abs timeout with offset is not greater than smq_time_now()");
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) - smq_timespec_to_timestamp_ms(&now_org) == ms_offset, .failure_msg = "difference between abs timeout and smq_time_now() is not as expected");
}

STF_TEST_CASE(smq_utils, abs_timeout_addition_1ms_match)
{
    static const long ms_offset = 1;
    struct timespec now_org = smq_time_now();
    struct timespec now_offset = now_org;
    smq_abs_timeout(&now_offset, ms_offset);
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) > smq_timespec_to_timestamp_ms(&now_org), .failure_msg = "abs timeout with offset is not greater than smq_time_now()");
    STF_EXPECT(smq_timespec_to_timestamp_ms(&now_offset) - smq_timespec_to_timestamp_ms(&now_org) == ms_offset, .failure_msg = "difference between abs timeout and smq_time_now() is not as expected");
}

int main(void)
{
    return STF_RUN_TESTS();
}
