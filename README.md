# anonim

## Краткая суть проекта

`anonim` — прототип защищенного текстового чата.

- Бэкенд на C++ хранит историю сообщений, выполняет аутентификацию и маршрутизацию.
- Клиент — веб-приложение (Vue + Vite), отвечает за интерфейс и взаимодействие с API.
- Модель безопасности ориентирована на E2EE: сервер получает и хранит только шифртекст и метаданные, без расшифровки содержимого сообщений.

## Сборка (backend)

### Вариант для текущего окружения Windows (MSYS2 g++ + Ninja)

```powershell
cd backend
cmake -S . -B build-ninja -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build-ninja
```

### Универсальный вариант CMake

```powershell
cd backend
cmake -S . -B build
cmake --build build --config Release
```

## Пример запуска

```powershell
cd backend
./build-ninja/anonim_server.exe 8090
```

Проверка доступности API:

```powershell
Invoke-WebRequest -UseBasicParsing http://127.0.0.1:8090/health
```

Ожидаемое тело ответа:

```json
{ "status": "ok", "service": "anonim-backend" }
```

## Использованные сторонние библиотеки и инструменты

### Backend

- CMake (система сборки)
- Ninja (генератор сборки, в текущем окружении)
- WinSock2 (`ws2_32`) для сетевого взаимодействия на Windows

Примечание: сам сервер реализован в основном на стандартной библиотеке C++20, без внешних C++ фреймворков.

### Frontend

- Vue 3
- Vite
- `@vitejs/plugin-vue`
- TypeScript
- `vue-tsc`

