#include "client.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeServer {
    int calls;
    int login_status;
    int challenge_calls;
    int login_calls;
    int sync_calls;
    bool reject_sync_once;
} FakeServer;

static bool digest(String body, Slice output)
{
    assert(body.length > 100);
    assert(output.length >= 65);
    memset(output.data, 'b', 64);
    ((char *)output.data)[64] = 0;
    return true;
}

static bool sign_message(void *context, String message, Slice output)
{
    assert(context == (void *)0x1);
    assert(message.length > 100);
    assert(output.length >= 4841);
    memset(output.data, 'd', 4840);
    ((char *)output.data)[4840] = 0;
    return true;
}

static bool send_request(void *context, HttpRequest request,
                         Slice output, int32_t *status)
{
    FakeServer *server = context;
    ++server->calls;
    assert(output.length >= 256);
    assert(request.accept.length == strlen("application/json"));
    if (server->calls == 1) {
        assert(request.method.length == 3);
        assert(memcmp(request.method.data, "GET", 3) == 0);
        const char *path = "/api/v1/sync/challenge?user_id=";
        assert(strstr(request.url.data, path) != NULL);
        char nonce[80];
        snprintf(nonce, sizeof nonce, "{\"nonce\":\"%064d\"}", 0);
        memset(nonce + 10, 'c', 64);
        strcpy(output.data, nonce);
        *status = 200;
        return true;
    }
    assert(server->calls == 2);
    assert(request.method.length == 4);
    assert(memcmp(request.method.data, "POST", 4) == 0);
    assert(strstr(request.url.data, "/api/v1/sync/login") != NULL);
    assert(request.body.length > 100);
    assert(request.headers.length == 2);
    const HttpHeader *headers = request.headers.data;
    assert(headers[0].name.length == strlen("X-Daochi-User"));
    assert(headers[0].value.length == 64);
    assert(headers[1].name.length == strlen("X-Daochi-Signature"));
    assert(headers[1].value.length == 4840);
    *status = server->login_status;
    if (*status == 200) {
        strcpy(output.data,
            "{\"auth_token\":\"token\",\"expires_in_seconds\":1800,"
            "\"server_time\":1700000000}");
    } else {
        strcpy(output.data, "{}");
    }
    return true;
}

static bool send_bearer(void *context, HttpRequest request,
                        Slice output, int32_t *status)
{
    FakeServer *server = context;
    if (request.method.length == 3 &&
        memcmp(request.method.data, "GET", 3) == 0) {
        ++server->challenge_calls;
        char nonce[80];
        snprintf(nonce, sizeof nonce, "{\"nonce\":\"%064d\"}", 0);
        memset(nonce + 10, 'c', 64);
        strcpy(output.data, nonce);
        *status = 200;
        return true;
    }
    assert(request.method.length == 4);
    assert(memcmp(request.method.data, "POST", 4) == 0);
    if (strstr(request.url.data, "/api/v1/sync/login") != NULL) {
        ++server->login_calls;
        strcpy(output.data,
            "{\"auth_token\":\"token\",\"expires_in_seconds\":1800,"
            "\"server_time\":1700000000}");
        *status = 200;
        return true;
    }
    assert(strstr(request.url.data, "/api/v1/sync") != NULL);
    assert(request.token.length == 5);
    assert(memcmp(request.token.data, "token", 5) == 0);
    assert(request.headers.length == 2);
    const HttpHeader *headers = request.headers.data;
    assert(headers[0].value.length == 64);
    assert(headers[1].value.length == strlen("client-1"));
    assert(request.body.length == 2);
    assert(memcmp(request.body.data, "{}", 2) == 0);
    ++server->sync_calls;
    if (server->reject_sync_once) {
        server->reject_sync_once = false;
        strcpy(output.data, "{}");
        *status = 401;
    } else {
        strcpy(output.data, "{\"server_version\":3}");
        *status = 200;
    }
    return true;
}

int main(void)
{
    char account_id[65];
    memset(account_id, 'a', 64);
    account_id[64] = 0;
    FakeServer server = {.login_status = 200};
    Client client = {0};
    client.server_url = StringLiteral("http://127.0.0.1:8080");
    client.identity.account_id = StringView(account_id, 64);
    client.identity.public_key = StringLiteral("public-key");
    client.identity.client_id = StringLiteral("client-1");
    client.identity.signing_context = (void *)0x1;
    client.digest = digest;
    client.signer = sign_message;
    client.send = send_request;
    client.transport_context = &server;
    Session session = {0};
    AuthResult result = Login(client, &session);
    assert(result == AuthResult_AUTH_OK);
    assert(server.calls == 2);
    assert(strcmp((char *)session.token, "token") == 0);

    server.calls = 0;
    server.login_status = 401;
    result = Login(client, &session);
    assert(result == AuthResult_AUTH_FAILED);
    assert(session.token[0] == 0);

    server.calls = 0;
    client.server_url = StringLiteral("http://public.example.org");
    result = Login(client, &session);
    assert(result == AuthResult_AUTH_INVALID_URL);
    assert(server.calls == 0);

    memset(&server, 0, sizeof server);
    client.server_url = StringLiteral("http://127.0.0.1:8080");
    client.send = send_bearer;
    session = (Session){0};
    char output[256];
    int32_t status = 0;
    result = BearerRequest(client, &session, 1000, StringLiteral("POST"),
        StringLiteral("/api/v1/sync"), StringLiteral("{}"),
        (Slice){.data = output, .length = sizeof output}, &status);
    assert(result == AuthResult_AUTH_OK && status == 200);
    assert(server.challenge_calls == 1 && server.login_calls == 1);
    assert(server.sync_calls == 1);
    assert(session.expires_at_local == 2770);
    assert(strcmp(output, "{\"server_version\":3}") == 0);

    server.reject_sync_once = true;
    result = BearerRequest(client, &session, 1001, StringLiteral("POST"),
        StringLiteral("/api/v1/sync"), StringLiteral("{}"),
        (Slice){.data = output, .length = sizeof output}, &status);
    assert(result == AuthResult_AUTH_OK && status == 200);
    assert(server.challenge_calls == 2 && server.login_calls == 2);
    assert(server.sync_calls == 3);

    result = BearerRequest(client, &session, 1002, StringLiteral("POST\r\n"),
        StringLiteral("/api/v1/sync"), StringLiteral("{}"),
        (Slice){.data = output, .length = sizeof output}, &status);
    assert(result == AuthResult_AUTH_PAYLOAD_FAILED);
    assert(server.sync_calls == 3);
    puts("Daochi Ziran client login passed");
    return 0;
}
