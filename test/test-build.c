#define NOB_IMPLEMENTATION
#include "nob.h"

static inline bool file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };

    nob_cmd_append(&cmd, "mkdir", "-p", "build/deps/stf");
    if (!nob_cmd_run(&cmd)) return 1;
    if (!file_exists("build/deps/stf/stf.h")) {
        nob_cmd_append(&cmd, "curl", "-Lo", "build/deps/stf/stf.h", "https://raw.githubusercontent.com/sovco/stf/refs/heads/master/include/stf/stf.h");
        if (!nob_cmd_run(&cmd)) return 1;
    }
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Wpedantic", "-D_POSIX_C_SOURCE=200112L", "-std=c11", "-o", "build/smq-utils-test", "-lpthread", "-lrt", "-Iinclude", "-Ibuild/deps", "test/smq-utils-test.c");
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Wpedantic", "-D_POSIX_C_SOURCE=200112L", "-std=c11", "-Wpedantic", "-ggdb3", "-o", "build/smq-server-client-test", "-lpthread", "-lrt", "-Iinclude", "-Ibuild/deps", "test/smq-server-client-test.c");
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Wpedantic", "-D_POSIX_C_SOURCE=200112L", "-std=c11", "-Wpedantic", "-o", "build/smq-channel-create-test", "-lpthread", "-lrt", "-Iinclude", "-Ibuild/deps", "test/smq-channel-create-test.c");
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-Wpedantic", "-D_POSIX_C_SOURCE=200112L", "-std=c11", "-Wpedantic", "-o", "build/smq-channel-listen-send", "-lpthread", "-lrt", "-Iinclude", "-Ibuild/deps", "test/smq-channel-listen-send.c");
    if (!nob_cmd_run(&cmd)) return 1;
    nob_cmd_append(&cmd, "parallel", "--keep-order", ":::", "./build/smq-utils-test", "./build/smq-channel-create-test", "./build/smq-channel-listen-send", "./build/smq-server-client-test");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}
