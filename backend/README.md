# anonim backend (C++ + CMake)

Minimal backend prototype for a secure text chat.

## Key goals implemented

- User authentication (register + login with token session).
- Message routing between users.
- Message history storage on server side (in-memory).
- E2EE-compatible transport: server stores and forwards ciphertext only.

## Build

```powershell
cd backend
cmake --build build-ninja
cmake --build build-ninja --config Release
```

## Run

```powershell
./build/anonim_server 8080
```

On Windows with Visual Studio generator, executable path is usually:

## API

### Health

- `GET /health`

### Register

- `POST /api/register`
- JSON body:
  - `username`
  - `passwordHash`
  - `publicKey`

### Login

- `POST /api/login`
- JSON body:
  - `username`
  - `passwordHash`
- Response includes `token`.

### Get user public key

- `GET /api/users/{username}/public-key`

### Send encrypted message

- `POST /api/messages`
- Header: `Authorization: Bearer <token>`
- JSON body:
  - `to`
  - `ciphertext`
  - `nonce`
  - `encryption` (optional, algorithm info)

### Get message history with peer

- `GET /api/messages?with=<peerUsername>`
- Header: `Authorization: Bearer <token>`

## E2EE note

This backend never decrypts message content. It receives and stores only encrypted payload (`ciphertext`) and metadata required for routing.
