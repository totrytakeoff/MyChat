# Service MVP Roadmap

## Guiding Principle

MyChat will progress by completing one service MVP at a time. Each service MVP
must preserve the intended microservice boundary while keeping the feature set
small enough to finish and verify.

The sequence is vertical: finish a narrow, tested slice of one service, then
integrate it with the next service. Avoid expanding many services as empty
shells.

## Phase A: Foundation Baseline

Status: mostly complete.

Scope:

- Root CMake staging.
- vcpkg manifest.
- Docker Compose Redis/PostgreSQL.
- Protobuf regeneration target.
- Gateway startup from config.
- Redis wrapper migration to hiredis.
- Focused Redis/Auth tests.

Exit criteria:

- `gateway_server` builds.
- Redis/PostgreSQL containers are healthy.
- Gateway health endpoint returns `{"status":"ok"}`.
- `RedisHiredisTest` and `AuthTokenTest` pass.

## Phase B: Gateway Service MVP Hardening

Status: in progress.

Scope:

- Keep Gateway as an independent service process.
- Remove temporary/test-only handler paths where they affect production flow.
- Fix refresh-token Redis model:
  - `refresh_token:<token>` stores token metadata;
  - `user:<uid>:refresh_tokens` indexes active tokens;
  - each refresh token key has independent TTL.
- Fix access-token revocation:
  - `revoked_access_token:<jti>` with TTL aligned to access token expiry.
- Keep `/api/v1/health` as a minimal operational endpoint.
- Keep token verification tests focused and deterministic.

Exit criteria:

- Gateway builds without known Gateway/Auth warnings.
- Auth token tests cover per-token Redis storage and revocation TTL behavior.
- Gateway health smoke still passes.
- Gateway runtime contract is documented.

## Phase C: PostgreSQL/ODB Foundation MVP

Status: not started as validated build baseline.

Scope:

- Verify ODB compiler availability.
- Decide how ODB is installed and documented:
  - system package;
  - vcpkg package if feasible;
  - project script check with a clear error if missing.
- Bring `common/database/pgsql` into the staged build.
- Generate ODB mapping files from persistent model headers.
- Add a focused PostgreSQL/ODB integration test using Docker PostgreSQL.
- Keep the existing `PgSqlConnection`/`PgSqlManager` design if it builds and
  behaves correctly; refactor only where tests expose problems.

Exit criteria:

- PostgreSQL/ODB target builds.
- A minimal ODB entity can be persisted and loaded from Docker PostgreSQL.
- PgSQL manager health and transaction tests pass.
- ODB generation command is documented.

## Phase D: User Service MVP

Status: not started.

Scope:

- Create a real `services/user` build target.
- Use ODB-backed user entity and repository.
- Support:
  - register by account/password;
  - login credential check;
  - get user profile by uid/account;
  - update last login time.
- Add password hashing. Do not store plaintext passwords.
- Define service request/response contract through protobuf/gRPC or the
  project-approved service interface.

Exit criteria:

- User Service builds as its own target.
- User Service tests pass against Docker PostgreSQL.
- Register/login/profile workflows pass at service level.
- User Service API contract is documented.

## Phase E: Gateway + User Service Integration

Status: not started.

Scope:

- Wire Gateway auth HTTP routes to User Service:
  - `/api/v1/auth/register`;
  - `/api/v1/auth/login`;
  - `/api/v1/auth/refresh`;
  - `/api/v1/auth/logout`.
- Gateway remains responsible for token issuance/verification, unless the
  service contract deliberately moves token issuance into User Service.
- User Service remains responsible for durable account data.

Exit criteria:

- Gateway endpoint tests pass.
- Login returns access/refresh tokens.
- Refresh returns a new valid access token.
- Logout revokes the relevant tokens.

## Phase F: Message Service MVP

Status: not started.

Scope:

- Create `services/message` target.
- Store direct messages through PostgreSQL/ODB.
- Support:
  - send one-to-one text message;
  - save offline message;
  - query history;
  - mark delivered/read if included in MVP contract.
- Integrate Gateway online delivery path through ConnectionManager/Push path.

Exit criteria:

- Message Service tests pass.
- Gateway can send a message to an online user.
- Offline message can be persisted and later pulled.
- Message schema and protobuf contract are documented.

## Phase G: Friend/Group/Push MVPs

Status: future work.

Scope:

- Friend Service: friend requests, accept/reject, contact list.
- Group Service: group create/join/member list/basic group message metadata.
- Push Service: online fanout and deferred delivery policy.

Exit criteria:

- Each service has its own tests and documented API.
- Each service integrates with Gateway through stable contracts.

## Build/Test Gate

Before moving from one phase to the next:

```bash
docker compose up -d redis postgres
cmake -S . -B /tmp/mychat-dev \
  -DMYCHAT_BUILD_TESTS=ON \
  -DMYCHAT_BUILD_GATEWAY=ON \
  -DMYCHAT_BUILD_SERVICES=ON \
  -DMYCHAT_BUILD_LEGACY_GATEWAY_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/mychat-dev -j2
ctest --test-dir /tmp/mychat-dev --output-on-failure
```

During transition, old suites may remain disabled, but every newly completed
module must have a focused `ctest -R ...` command recorded in the devlog.
