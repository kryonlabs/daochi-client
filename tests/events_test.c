#include "events.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct FakeReceiver {
    int calls;
    const char *body;
} FakeReceiver;

static bool receive_event(void *context, String url, String token,
                          Slice output, int32_t *status)
{
    FakeReceiver *receiver = context;
    ++receiver->calls;
    assert(url.length == strlen("wss://api.example.org/api/v1/sync/ws"));
    assert(memcmp(url.data, "wss://api.example.org/api/v1/sync/ws",
        url.length) == 0);
    assert(token.length == 5 && memcmp(token.data, "token", 5) == 0);
    assert(output.length > (int64_t)strlen(receiver->body));
    strcpy(output.data, receiver->body);
    *status = 200;
    return true;
}

int main(void)
{
    char account[65], other[65], url[128];
    memset(account, 'a', 64);
    account[64] = 0;
    memset(other, 'b', 64);
    other[64] = 0;
    assert(EventUrl(StringLiteral("https://api.example.org/"),
        (Slice){.data = url, .length = sizeof url}));
    assert(strcmp(url, "wss://api.example.org/api/v1/sync/ws") == 0);
    assert(!EventUrl(StringLiteral("http://public.example.org"),
        (Slice){.data = url, .length = sizeof url}));

    char body[256];
    snprintf(body, sizeof body,
        "{\"type\":\"sync_changed\",\"user_id_hash\":\"%s\",\"server_version\":7}",
        account);
    RemoteEvent event = ParseRemoteEvent(StringView(body, strlen(body)),
        StringView(account, 64));
    assert(event.kind == EventKind_EVENT_CHANGED && event.server_version == 7);
    assert(ParseRemoteEvent(StringView(body, strlen(body)),
        StringView(other, 64)).kind == EventKind_EVENT_INVALID);

    Client client = {0};
    client.server_url = StringLiteral("https://api.example.org");
    client.identity.account_id = StringView(account, 64);
    Session session = {0};
    strcpy((char *)session.token, "token");
    session.expires_at_local = 2000;
    FakeReceiver receiver = {.body = body};
    assert(WaitForEvent(client, &session, 1000, &receiver,
        receive_event, &event) == AuthResult_AUTH_OK);
    assert(receiver.calls == 1);
    assert(event.kind == EventKind_EVENT_CHANGED && event.server_version == 7);
    puts("Daochi Ziran remote events passed");
    return 0;
}
