#include "url.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char output[256];
    Slice buffer = {.data = output, .length = sizeof output};
    assert(UrlValid(StringLiteral("https://api.example.org")));
    assert(UrlValid(StringLiteral("http://localhost:8080")));
    assert(UrlValid(StringLiteral("127.0.0.1:8080")));
    assert(UrlValid(StringLiteral("http://192.168.1.12")));
    assert(!UrlValid(StringLiteral("http://api.example.org")));
    assert(!UrlValid(StringLiteral("http://localhost.evil")));
    assert(!UrlValid(StringLiteral("http://192.168.1.256")));
    assert(!UrlValid(StringLiteral("https://api.example.org\r\n")));
    assert(NormalizeUrl(StringLiteral("127.0.0.1:8080"), buffer));
    assert(strcmp(output, "http://127.0.0.1:8080") == 0);
    assert(JoinUrl(StringLiteral("https://api.example.org/"),
        StringLiteral("/api/v1/sync"), buffer));
    assert(strcmp(output, "https://api.example.org/api/v1/sync") == 0);
    assert(JoinWebSocketUrl(StringLiteral("https://api.example.org"),
        StringLiteral("/api/v1/sync/ws"), buffer));
    assert(strcmp(output, "wss://api.example.org/api/v1/sync/ws") == 0);
    buffer.length = 4;
    assert(!NormalizeUrl(StringLiteral("localhost"), buffer));
    assert(output[0] == 0);
    puts("Daochi Ziran URL policy passed");
    return 0;
}
