#pragma once
#include <string>

// Короткие пояснения:
// `User` и `Message` — структуры для хранения данных пользователей и сообщений.
// `ChatServer` — минимальный HTTP-сервер, хранящий пользователей, сессии и зашифрованные сообщения в памяти.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct User
{
    std::string username;
    std::string password_hash;
    std::string public_key;
};

struct Message
{
    std::uint64_t id;
    std::string from;
    std::string to;
    std::string ciphertext;
    std::string nonce;
    std::string encryption;
    std::string timestamp;
};

class ChatServer
{
public:
    explicit ChatServer(int port);
    ~ChatServer();

    ChatServer(const ChatServer &) = delete;
    ChatServer &operator=(const ChatServer &) = delete;

    // Запустить/остановить сервер (локально, в текущем процессе).
    bool start();
    void stop();

#ifdef ANONIM_TESTING
    friend class ChatServerTestAccess;
#endif

private:
    int port_;
    int listen_socket_;
    std::atomic<bool> running_;

    std::mutex data_mutex_;
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> sessions_;
    std::vector<Message> messages_;
    std::uint64_t next_message_id_;

    void accept_loop();
    void handle_client(int client_socket);

    // Обработчики HTTP-запросов (используются в тестах и сетевом цикле).
    std::string route_request(
        const std::string &method,
        const std::string &target,
        const std::unordered_map<std::string, std::string> &headers,
        const std::string &body);

    std::string handle_health() const;
    std::string handle_register(const std::string &body);
    std::string handle_login(const std::string &body);
    std::string handle_get_public_key(const std::string &target);
    std::string handle_post_message(
        const std::unordered_map<std::string, std::string> &headers,
        const std::string &body);
    std::string handle_get_messages(
        const std::unordered_map<std::string, std::string> &headers,
        const std::string &target);

    std::string authenticate(const std::unordered_map<std::string, std::string> &headers);
    static std::string make_http_response(int status_code, const std::string &status_text, const std::string &json_body);
};
