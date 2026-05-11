# anonim

## Краткое описание проекта

`anonim` — прототип защищенного текстового чата с разделением на backend и frontend:

- `backend` (C++20 + CMake) отвечает за регистрацию, вход, выдачу публичных ключей, хранение и выдачу истории сообщений.
- `frontend/anonim-web` (Vue 3 + TypeScript + Vite) отвечает за интерфейс, управление локальными ключами и взаимодействие с API.

Проект построен по модели E2EE: сервер не расшифровывает сообщения и хранит только шифртекст и служебные метаданные.

## Основные преимущества

- Разделение ответственности: backend занимается API и хранением, frontend — криптографией и UX.
- E2EE-ориентированная архитектура: приватные ключи остаются на клиенте.
- Простой локальный запуск: отдельные команды для backend и frontend или запуск через Docker Compose.
- Покрытие backend-тестами: unit-тесты на Catch2 и отдельные сценарные проверки ключевых потоков.

## Механизм шифрования

- На клиенте генерируется пара ключей ECDH P-256 при регистрации.
- На сервер отправляется только публичный ключ пользователя.
- Для каждого собеседника клиент вычисляет общий секрет (ECDH) и получает ключ AES-GCM 256.
- Каждое сообщение шифруется на клиенте (AES-GCM с уникальным nonce).
- Сервер получает и хранит `ciphertext`, `nonce` и метаданные маршрутизации без доступа к plaintext.

## Third-party технологии

### Backend

- CMake
- Ninja (в текущем рабочем окружении)
- Catch2 v3 (unit-тесты)
- WinSock2 (`ws2_32`) на Windows

### Frontend

- Vue 3
- Vite
- TypeScript
- `@vitejs/plugin-vue`
- `vue-tsc`

### Инфраструктура

- Docker
- Docker Compose

## Инструкции по локальному запуску

### 1) Запуск backend (Windows, MSYS2 g++ + Ninja)

```powershell
cd backend
cmake -S . -B build-ninja -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build-ninja
./build-ninja/anonim_server.exe 8090
```

Проверка health-эндпоинта:

```powershell
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:8090/health
```

Ожидаемый ответ:

```json
{ "status": "ok", "service": "anonim-backend" }
```

### 2) Запуск frontend

```powershell
cd frontend/anonim-web
npm install
npm run dev
```

По умолчанию frontend доступен по адресу `http://127.0.0.1:5173`.

### 3) Запуск через Docker Compose (backend + frontend)

```powershell
docker compose up --build
```

После запуска:

- Frontend: `http://127.0.0.1:5173`
- Backend API: `http://127.0.0.1:8090`

Остановка:

```powershell
docker compose down
```

## Инструкции по тестированию

### Backend unit-тесты (Catch2 + CTest)

```powershell
cd backend
cmake -S . -B build-ninja -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

### Backend сценарные проверки

После сборки доступны исполняемые сценарии в `backend/build-ninja/tests/scenarios`:

- `register_flow`
- `login_flow`
- `public_key_flow`
- `message_send_flow`
- `conversation_poll_flow`

Пример запуска одного сценария:

```powershell
cd backend/build-ninja/tests/scenarios
./register_flow.exe
```

### Frontend проверка сборки

```powershell
cd frontend/anonim-web
npm run build
```
