# smq
Simple interface for POSIX mq, and a server client type implementation that uses POSIX mq.

# Notice

This is under active development.
What is missing?
1) Error handling on numerous occasions (too many clients, message que full and etc)
2) A good way to form a message (smq_message)
3) Lacking in testing

# Building and Running Tests

```bash
curl -Lo test/nob.h https://raw.githubusercontent.com/tsoding/nob.h/refs/heads/main/nob.h
gcc -o test-build test/test-build.c
./test-build
```
