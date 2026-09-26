#include "sync.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeServer {
    int stage;
    int account_signs;
    int device_signs;
    int nonces;
} FakeServer;

static bool digest(String body, Slice output)
{
    assert(body.length > 0 && output.length >= 65);
    memset(output.data, 'f', 64);
    ((char *)output.data)[64] = 0;
    return true;
}

static bool account_sign(void *context, String message, Slice output)
{
    FakeServer *server = context;
    ++server->account_signs;
    if (server->account_signs == 1) {
        assert(message.length > 20);
        assert(memcmp(message.data, "daochi-sync-v1\nPOST\n/api/v1/sync/login\n", 39) == 0);
    } else if (server->account_signs == 2) {
        assert(memcmp(message.data, "daochi-device-registration-v1\n", 30) == 0);
        assert(strstr(message.data, "\ninbe\n") != NULL);
        assert(strstr(message.data, "\n1700000300\n") != NULL);
    } else {
        assert(server->account_signs == 3);
        assert(memcmp(message.data, "daochi-tx-v1\n6\n", 15) == 0);
        assert(strstr(message.data, "\nPOST\n/api/v1/sync\n") != NULL);
        assert(strstr(message.data, "\n1700000300\n") != NULL);
    }
    assert(output.length >= 4841);
    memset(output.data, 'a', 4840);
    ((char *)output.data)[4840] = 0;
    return true;
}

static bool device_sign(void *context, String message, Slice output)
{
    FakeServer *server = context;
    ++server->device_signs;
    assert(server->device_signs == 1);
    assert(memcmp(message.data, "daochi-tx-v1\n6\n", 15) == 0);
    assert(output.length >= 129);
    memset(output.data, 'd', 128);
    ((char *)output.data)[128] = 0;
    return true;
}

static bool nonce(void *context, Slice output)
{
    FakeServer *server = context;
    ++server->nonces;
    assert(server->nonces <= 3 && output.length >= 65);
    memset(output.data, '0' + server->nonces, 64);
    ((char *)output.data)[64] = 0;
    return true;
}

static bool send_request(void *context, HttpRequest request,
                         Slice output, int32_t *status)
{
    FakeServer *server = context;
    ++server->stage;
    assert(request.url.length < 1024);
    assert(request.url.data[request.url.length] == 0);
    switch (server->stage) {
    case 1:
        assert(strstr(request.url.data, "/api/v1/sync/challenge?") != NULL);
        strcpy(output.data, "{\"nonce\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\"}");
        break;
    case 2:
        assert(strstr(request.url.data, "/api/v1/sync/login") != NULL);
        assert(request.headers.length == 2);
        strcpy(output.data, "{\"auth_token\":\"token\",\"expires_in_seconds\":1800,\"server_time\":1700000000}");
        break;
    case 3:
        assert(strstr(request.url.data, "/api/v1/account/devices") != NULL);
        assert(request.token.length == 5);
        assert(strstr(request.body.data, "\"app_id\":\"inbe\"") != NULL);
        assert(strstr(request.body.data, "\"nonce\":\"111111") != NULL);
        assert(strstr(request.body.data, "\"expires_at\":1700000300") != NULL);
        strcpy(output.data, "{}");
        break;
    case 4: {
        assert(strstr(request.url.data, "/api/v1/sync") != NULL);
        assert(request.headers.length == 3);
        const HttpHeader *headers = request.headers.data;
        assert(headers[2].name.length == 11);
        assert(memcmp(headers[2].name.data, "X-Daochi-Tx", 11) == 0);
        assert(strstr(headers[2].value.data, "\"protocol_version\":6") != NULL);
        assert(strstr(headers[2].value.data, "\"tx_id\":\"222222") != NULL);
        assert(strstr(headers[2].value.data, "\"nonce\":\"333333") != NULL);
        assert(strstr(headers[2].value.data, "\"body_sha256\":\"ffffff") != NULL);
        assert(strstr(headers[2].value.data, "\"device_signature\":\"dddddd") != NULL);
        assert(request.body.length == 22);
        assert(memcmp(request.body.data, "{\"protocol_version\":6}", 22) == 0);
        strcpy(output.data, "{\"server_version\":7}");
        break;
    }
    default:
        assert(!"unexpected request");
    }
    *status = 200;
    return true;
}

int main(void)
{
    FakeServer server = {0};
    char account[65], device_key[65];
    memset(account, 'a', 64);
    account[64] = 0;
    memset(device_key, 'b', 64);
    device_key[64] = 0;
    Client client = {0};
    client.server_url = StringLiteral("http://127.0.0.1:8080");
    client.app_id = StringLiteral("inbe");
    client.identity.account_id = StringView(account, 64);
    client.identity.public_key = StringLiteral("public-key");
    client.identity.client_id = StringLiteral("client-1");
    client.identity.signing_context = &server;
    client.digest = digest;
    client.signer = account_sign;
    client.send = send_request;
    client.transport_context = &server;
    Device device = {
        .key_id = StringView(device_key, 64),
        .public_key_hex = StringView(device_key, 64),
        .signing_context = &server,
        .signer = device_sign,
    };
    Session session = {0};
    char response[256];
    int32_t status = 0;
    AuthResult result = Sync(client, &session, device, 1000, &server, nonce,
        StringLiteral("{\"protocol_version\":6}"),
        (Slice){.data = response, .length = sizeof response}, &status);
    assert(result == AuthResult_AUTH_OK);
    assert(status == 200 && server.stage == 4);
    assert(server.account_signs == 3 && server.device_signs == 1);
    assert(server.nonces == 3);
    assert(strcmp(response, "{\"server_version\":7}") == 0);
    puts("Daochi Ziran signed sync passed");
    return 0;
}
