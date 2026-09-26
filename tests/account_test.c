#include "account.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeServer {
    int requests;
    int delete_status;
} FakeServer;

static bool digest(String body, Slice output)
{
    assert(body.length > 100 && output.length >= 65);
    memset(output.data, 'f', 64);
    ((char *)output.data)[64] = 0;
    return true;
}

static bool signer(void *context, String message, Slice output)
{
    assert(context == (void *)0x1);
    const char prefix[] = "daochi-sync-v1\nPOST\n/api/v1/account/delete\n";
    assert(message.length == sizeof prefix - 1 + 64 + 1 + 64 + 1);
    assert(memcmp(message.data, prefix, sizeof prefix - 1) == 0);
    assert(message.data[sizeof prefix - 1 + 64] == '\n');
    assert(output.length >= 4841);
    memset(output.data, 'd', 4840);
    ((char *)output.data)[4840] = 0;
    return true;
}

static bool send_request(void *context, HttpRequest request,
                         Slice output, int32_t *status)
{
    FakeServer *server = context;
    ++server->requests;
    if (server->requests == 1) {
        assert(strstr(request.url.data, "/api/v1/sync/challenge?") != NULL);
        strcpy(output.data, "{\"nonce\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"}");
        *status = 200;
    } else {
        assert(server->requests == 2);
        assert(strstr(request.url.data, "/api/v1/account/delete") != NULL);
        assert(request.body.length > 100);
        assert(strstr(request.body.data, "\"public_key\":\"public-key\"") != NULL);
        assert(request.headers.length == 2);
        const HttpHeader *headers = request.headers.data;
        assert(headers[1].value.length == 4840);
        strcpy(output.data, "{}");
        *status = server->delete_status;
    }
    return true;
}

int main(void)
{
    char account[65];
    memset(account, 'a', 64);
    account[64] = 0;
    FakeServer server = {.delete_status = 200};
    Client client = {0};
    client.server_url = StringLiteral("http://127.0.0.1:8080");
    client.identity.account_id = StringView(account, 64);
    client.identity.public_key = StringLiteral("public-key");
    client.identity.signing_context = (void *)0x1;
    client.digest = digest;
    client.signer = signer;
    client.send = send_request;
    client.transport_context = &server;
    assert(DeleteAccount(client) == AuthResult_AUTH_OK);
    assert(server.requests == 2);

    server.requests = 0;
    server.delete_status = 500;
    assert(DeleteAccount(client) == AuthResult_AUTH_REQUEST_FAILED);
    server.requests = 0;
    server.delete_status = 403;
    assert(DeleteAccount(client) == AuthResult_AUTH_FAILED);
    client.server_url = StringLiteral("http://public.example.org");
    server.requests = 0;
    assert(DeleteAccount(client) == AuthResult_AUTH_INVALID_URL);
    assert(server.requests == 0);
    puts("Daochi Ziran signed account deletion passed");
    return 0;
}
