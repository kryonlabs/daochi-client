# Daochi Client

Daochi Client is the independent Ziran library for applications that sync with
Daochi. It contains no server, mesh node, database, or UI implementation.

The portable modules currently provide:

- canonical challenge, login body, and signed message builders (`wire.zi`);
- HTTPS and local network URL policy (`url.zi`);
- challenge parsing, signed login preparation, and token parsing (`auth.zi`);
- login and bearer requests with one fresh login after a `401` (`client.zi`);
- canonical device registration and protocol v6 transaction messages
  (`transaction.zi`);
- signed device registration and protocol v6 sync requests (`sync.zi`).
- alias and friend requests, actions, lists, and stats (`social.zi`).
- challenge signed account deletion without transmitting a key backup
  (`account.zi`).

The application supplies the account signer and body digest. A host supplies
bounded HTTP requests through `SendRequest`; the host must NUL terminate every
response buffer, including on error. This keeps private keys in the app's key
store and keeps platform transport outside the protocol core. Applications
persist `Session` if they want bearer tokens to survive restart. Account keys
must remain when a bearer token expires.

Add this repository and Ziran's `std` directory to the Ziran module paths, then
import `client` and `sync`. Set `Client.app_id`, supply a `Device` with a
persisted Ed25519 key and signer, and supply a cryptographically random 64-digit
hex nonce callback. `Sync` registers the device and signs the exact sync body
with both the account and device keys. The app owns the payload format and
merges the response. For an Inbe native host, adapt `SendRequest` to Ziran's
curl module. Browser and Android hosts can implement the same boundary.

`sh tests/run.sh` checks the library using the sibling `ziran` checkout. Set
`ZIRAN_DIR` when that checkout is elsewhere.

The client migration is in progress. This library does not yet handle remote
events, device revocation, or platform key provisioning. Apps
must connect their payload builder, response merge, key storage, and transport
to the portable client.
