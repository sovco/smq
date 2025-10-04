#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <stf/stf.h>
#include <sched.h>
#define SMQ_IMPL
#include <smq/smq.h>

void handler_hello(smq_message *request, smq_message *response)
{
    (void)request;
    static const char *msg = "Hello!";
    memcpy(response->payload, msg, strlen(msg));
}

void handler_heya(smq_message *request, smq_message *response)
{
    (void)request;
    static const char *msg = "heya!";
    memcpy(response->payload, msg, strlen(msg));
}

void client_request(uint16_t id, const char *path, smq_message *response)
{
    smq_client client = { 0 };
    smq_message client_request = { 0 };
    *response = (smq_message){ 0 };
    smq_client_create(&client, id, path);
    smq_client_request(&client, &client_request, response, .timeout_ms = 1500);
    smq_client_destroy(&client);
}

STF_TEST_CASE(smq_server_client, test_single_client_single_request)
{
    static const char *expected_payload = "Hello!";
    smq_message server_response = { 0 };
    smq_server server = { 0 };
    pthread_t server_handle = 0;
    smq_server_create(&server, "/server");
    STF_EXPECT(smq_server_add_listener(&server, "-hello", handler_hello) == 0);
    STF_EXPECT(server.listeners != NULL, .failure_msg = "server unable to spawn listeners");
    smq_server_start_non_blocking(&server_handle, &server);
    STF_EXPECT(smq_server_ready(&server, 500));
    client_request(1, "/server-hello", &server_response);
    STF_EXPECT(strcmp(expected_payload, server_response.payload) == 0, .failure_msg = "server returned message is not what expected");
    smq_server_destroy(&server);
    STF_EXPECT(pthread_join(server_handle, NULL) == 0);
}

STF_TEST_CASE(smq_server_client, test_two_clients_single_request_two_paths)
{
    static const char *expected_payload_a = "Hello!";
    static const char *expected_payload_b = "heya!";
    pthread_t server_handle = 0;
    smq_message server_response = { 0 };
    smq_server server = { 0 };
    smq_server_create(&server, "/server");
    STF_EXPECT(smq_server_add_listener(&server, "-hello", handler_hello) == 0);
    STF_EXPECT(smq_server_add_listener(&server, "-heya", handler_heya) == 0);
    STF_EXPECT(server.listeners != NULL, .failure_msg = "server unable to spawn listeners");
    smq_server_start_non_blocking(&server_handle, &server);
    client_request(1, "/server-hello", &server_response);
    STF_EXPECT(strcmp(expected_payload_a, server_response.payload) == 0, .failure_msg = "server returned message is not what expected");
    memset(&server_response, 0x00, sizeof(server_response));
    client_request(2, "/server-heya", &server_response);
    STF_EXPECT(strcmp(expected_payload_b, server_response.payload) == 0, .failure_msg = "server returned message is not what expected");
    smq_server_destroy(&server);
    STF_EXPECT(pthread_join(server_handle, NULL) == 0);
}

void *request(void *args)
{
    (void)args;
    static int id = 1;
    smq_client client = { 0 };
    smq_message client_request = { 0 };
    smq_message *server_response = malloc(sizeof(*server_response));
    smq_client_create(&client, id++, "/server-hello");
    smq_client_request(&client, &client_request, server_response);
    smq_client_destroy(&client);
    return (void *)server_response;
}

STF_TEST_CASE(smq_server_client, test_multiple_client_single_request)
{
    static const size_t client_count = 5;
    static const char *expected_payload = "Hello!";
    pthread_t server_handle = 0;
    pthread_t clients[client_count];
    smq_message *server_response = NULL;
    smq_server server = { 0 };
    smq_server_create(&server, "/server");
    smq_server_add_listener(&server, "-hello", handler_hello);
    smq_server_start_non_blocking(&server_handle, &server);
    for (size_t i = 0; i < client_count; i++) {
        if (pthread_create(&clients[i], NULL, request, NULL) != 0) {
            smq_server_destroy(&server);
            STF_EXPECT(false);
        }
    }
    for (size_t j = 0; j < client_count; j++) {
        STF_EXPECT(pthread_join(clients[j], (void **)&server_response) == 0);
        if (server_response != NULL) {
            STF_EXPECT(strcmp(expected_payload, server_response->payload) == 0);
            free(server_response);
            continue;
        }
        STF_EXPECT(false, .failure_msg = "server response was NULL");
    }
    smq_server_destroy(&server);
    STF_EXPECT(pthread_join(server_handle, NULL) == 0);
}

int main(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    return STF_RUN_TESTS();
}
