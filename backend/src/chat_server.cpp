#include "chat_server.hpp"
#include "request_utils.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace request_utils;

namespace
{
    // Собирает HTTP-ответы в одном месте.
    class HttpResponseBuilder
    {
    public:
        HttpResponseBuilder &status(int code, std::string text)
        {
            status_code_ = code;
            status_text_ = std::move(text);
            return *this;
        }

        HttpResponseBuilder &json_body(std::string body)
        {
            json_body_ = std::move(body);
            return *this;
        }

        HttpResponseBuilder &allow_cors(bool enabled)
        {
            allow_cors_ = enabled;
            return *this;
        }

        std::string build() const
        {
            std::ostringstream response;
            response << "HTTP/1.1 " << status_code_ << ' ' << status_text_ << "\r\n"
                     << "Content-Type: application/json\r\n"
                     << "Content-Length: " << json_body_.size() << "\r\n"
                     << "Connection: close\r\n";

            if (allow_cors_)
            {
                response << "Access-Control-Allow-Origin: *\r\n"
                         << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token\r\n"
                         << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
            }

            response << "\r\n" << json_body_;
            return response.str();
        }

    private:
        int status_code_ = 200;
        std::string status_text_ = "OK";
        std::string json_body_ = "{}";
        bool allow_cors_ = true;
    };

    struct ValidationError
    {
        bool has_error = false;
        std::string message;
    };

    struct MessageValidationContext
    {
        std::string to;
        std::string message_type;
        std::string ciphertext;
        std::string nonce;
        std::string key_ciphertext;
        std::string key_nonce;
        std::string key_encryption;
        std::string file_name;
        std::string file_mime;
        std::string file_size;
    };

    // Отдельные правила проверки сообщения.
    class MessageValidationStrategy
    {
    public:
        virtual ~MessageValidationStrategy() = default;
        virtual ValidationError validate(const MessageValidationContext &context) const = 0;
    };

    class RequiredFieldsValidationStrategy final : public MessageValidationStrategy
    {
    public:
        ValidationError validate(const MessageValidationContext &context) const override
        {
            if (context.to.empty() || context.ciphertext.empty() || context.nonce.empty())
            {
                return ValidationError{true, "to, ciphertext and nonce are required"};
            }
            return ValidationError{};
        }
    };

    class MessageTypeValidationStrategy final : public MessageValidationStrategy
    {
    public:
        ValidationError validate(const MessageValidationContext &context) const override
        {
            if (!context.message_type.empty() && context.message_type != "text")
            {
                return ValidationError{true, "Only text messages are supported"};
            }
            return ValidationError{};
        }
    };

    class UnsupportedPayloadValidationStrategy final : public MessageValidationStrategy
    {
    public:
        ValidationError validate(const MessageValidationContext &context) const override
        {
            if (!context.key_ciphertext.empty() || !context.key_nonce.empty() || !context.key_encryption.empty() ||
                !context.file_name.empty() || !context.file_mime.empty() || !context.file_size.empty())
            {
                return ValidationError{true, "Media/file payloads are not supported"};
            }
            return ValidationError{};
        }
    };

    std::vector<std::unique_ptr<MessageValidationStrategy>> make_message_validation_strategies()
    {
        std::vector<std::unique_ptr<MessageValidationStrategy>> strategies;
        strategies.emplace_back(std::make_unique<RequiredFieldsValidationStrategy>());
        strategies.emplace_back(std::make_unique<MessageTypeValidationStrategy>());
        strategies.emplace_back(std::make_unique<UnsupportedPayloadValidationStrategy>());
        return strategies;
    }

    // Каждому роуту соответствует своя команда.
    struct RouteCommand
    {
        std::function<bool(const std::string &path)> match;
        std::function<std::string()> execute;
    };

#ifdef _WIN32
    using SocketType = SOCKET;
    constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
    using SocketType = int;
    constexpr SocketType kInvalidSocket = -1;
#endif

    void close_socket(SocketType socket_fd)
    {
#ifdef _WIN32
        closesocket(socket_fd);
#else
        close(socket_fd);
#endif
    }

    bool send_all(SocketType socket_fd, const std::string &data)
    {
        std::size_t total_sent = 0;
        while (total_sent < data.size())
        {
            const char *ptr = data.c_str() + total_sent;
            const int remaining = static_cast<int>(data.size() - total_sent);
#ifdef _WIN32
            const int sent = send(socket_fd, ptr, remaining, 0);
#else
            const int sent = static_cast<int>(send(socket_fd, ptr, static_cast<std::size_t>(remaining), 0));
#endif
            if (sent <= 0)
            {
                return false;
            }

            total_sent += static_cast<std::size_t>(sent);
        }

        return true;
    }

    bool recv_http_request(SocketType socket_fd, std::string &request_out)
    {
        request_out.clear();
        std::array<char, 4096> buffer{};
        std::size_t expected_size = 0;

        while (true)
        {
#ifdef _WIN32
            const int received = recv(socket_fd, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
            const int received = static_cast<int>(recv(socket_fd, buffer.data(), buffer.size(), 0));
#endif
            if (received <= 0)
            {
                break;
            }

            request_out.append(buffer.data(), static_cast<std::size_t>(received));

            if (expected_size == 0)
            {
                const std::size_t header_end = request_out.find("\r\n\r\n");
                if (header_end != std::string::npos)
                {
                    expected_size = header_end + 4;
                    const std::string header_block = request_out.substr(0, header_end);
                    const std::regex content_length_regex("\\r\\nContent-Length\\s*:\\s*(\\d+)", std::regex_constants::icase);
                    std::smatch match;
                    if (std::regex_search(header_block, match, content_length_regex) && match.size() >= 2)
                    {
                        expected_size += static_cast<std::size_t>(std::stoul(match[1].str()));
                    }
                }
            }

            if (expected_size > 0 && request_out.size() >= expected_size)
            {
                return true;
            }
        }

        return !request_out.empty();
    }

} // namespace

ChatServer::ChatServer(int port)
    : port_(port),
      listen_socket_(static_cast<int>(kInvalidSocket)),
      running_(false),
      next_message_id_(1) {}

ChatServer::~ChatServer()
{
    stop();
}

bool ChatServer::start()
{
    if (running_.load())
    {
        return true;
    }

#ifdef _WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return false;
    }
#endif

    const SocketType socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == kInvalidSocket)
    {
        std::cerr << "Failed to create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    int yes = 1;
#ifdef _WIN32
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes));
#else
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(port_));

    if (bind(socket_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
    {
        std::cerr << "Bind failed\n";
        close_socket(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    if (listen(socket_fd, 16) != 0)
    {
        std::cerr << "Listen failed\n";
        close_socket(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    listen_socket_ = static_cast<int>(socket_fd);
    running_.store(true);
    std::thread([this]()
                { accept_loop(); })
        .detach();
    return true;
}

void ChatServer::stop()
{
    if (!running_.exchange(false))
    {
        return;
    }

    if (listen_socket_ != static_cast<int>(kInvalidSocket))
    {
        close_socket(static_cast<SocketType>(listen_socket_));
        listen_socket_ = static_cast<int>(kInvalidSocket);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

void ChatServer::accept_loop()
{
    while (running_.load())
    {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif

        const SocketType client_socket = accept(
            static_cast<SocketType>(listen_socket_),
            reinterpret_cast<sockaddr *>(&client_addr),
            &client_len);

        if (client_socket == kInvalidSocket)
        {
            if (running_.load())
            {
                std::cerr << "Accept failed\n";
            }
            continue;
        }

        std::thread([this, client_socket]()
                    { handle_client(static_cast<int>(client_socket)); })
            .detach();
    }
}

void ChatServer::handle_client(int client_socket)
{
    std::string raw_request;
    if (!recv_http_request(static_cast<SocketType>(client_socket), raw_request))
    {
        close_socket(static_cast<SocketType>(client_socket));
        return;
    }

    const std::size_t header_end = raw_request.find("\r\n\r\n");
    if (header_end == std::string::npos)
    {
        const std::string bad = request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Malformed request\"}");
        send_all(static_cast<SocketType>(client_socket), bad);
        close_socket(static_cast<SocketType>(client_socket));
        return;
    }

    const std::string headers_part = raw_request.substr(0, header_end);
    const std::string body = raw_request.substr(header_end + 4);

    std::istringstream stream(headers_part);
    std::string request_line;
    if (!std::getline(stream, request_line))
    {
        const std::string bad = request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Missing request line\"}");
        send_all(static_cast<SocketType>(client_socket), bad);
        close_socket(static_cast<SocketType>(client_socket));
        return;
    }

    if (!request_line.empty() && request_line.back() == '\r')
    {
        request_line.pop_back();
    }

    std::istringstream request_line_stream(request_line);
    std::string method;
    std::string target;
    std::string version;
    request_line_stream >> method >> target >> version;

    if (method.empty() || target.empty())
    {
        const std::string bad = request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Invalid request line\"}");
        send_all(static_cast<SocketType>(client_socket), bad);
        close_socket(static_cast<SocketType>(client_socket));
        return;
    }

    std::unordered_map<std::string, std::string> headers;
    std::string header_line;
    while (std::getline(stream, header_line))
    {
        if (!header_line.empty() && header_line.back() == '\r')
        {
            header_line.pop_back();
        }

        if (header_line.empty())
        {
            continue;
        }

        const std::size_t colon = header_line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        const std::string key = request_utils::to_lower(request_utils::trim(header_line.substr(0, colon)));
        const std::string value = request_utils::trim(header_line.substr(colon + 1));
        headers[key] = value;
    }

    const std::string response = route_request(method, target, headers, body);
    send_all(static_cast<SocketType>(client_socket), response);
    close_socket(static_cast<SocketType>(client_socket));
}

std::string ChatServer::route_request(
    const std::string &method,
    const std::string &target,
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &body)
{
    const std::string path = path_only(target);

    if (method == "OPTIONS")
    {
        return make_http_response(200, "OK", "{}");
    }

    if (method == "GET" && path == "/health")
    {
        return handle_health();
    }

    if (method == "POST" && path == "/api/register")
    {
        return handle_register(body);
    }

    if (method == "POST" && path == "/api/login")
    {
        return handle_login(body);
    }

    if (method == "GET" && path.rfind("/api/users/", 0) == 0 && path.size() > std::string("/api/users//public-key").size() &&
        path.find("/public-key") == path.size() - std::string("/public-key").size())
    {
        return handle_get_public_key(path);
    }

    if (method == "POST" && path == "/api/messages")
    {
        return handle_post_message(headers, body);
    }

    if (method == "GET" && path == "/api/messages")
    {
        return handle_get_messages(headers, target);
    }

    return make_http_response(404, "Not Found", "{\"error\":\"Route not found\"}");
}

std::string ChatServer::handle_health() const
{
    return make_http_response(200, "OK", "{\"status\":\"ok\",\"service\":\"anonim-backend\"}");
}

std::string ChatServer::handle_register(const std::string &body)
{
    const std::string username = trim(json_get_string(body, "username"));
    const std::string password_hash = trim(json_get_string(body, "passwordHash"));
    const std::string public_key = trim(json_get_string(body, "publicKey"));

    if (username.empty() || password_hash.empty() || public_key.empty())
    {
        return make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"username, passwordHash and publicKey are required\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (users_.find(username) != users_.end())
    {
        return make_http_response(409, "Conflict", "{\"error\":\"User already exists\"}");
    }

    users_[username] = User{username, password_hash, public_key};
    return make_http_response(201, "Created", "{\"status\":\"registered\"}");
}

std::string ChatServer::handle_login(const std::string &body)
{
    const std::string username = trim(json_get_string(body, "username"));
    const std::string password_hash = trim(json_get_string(body, "passwordHash"));

    if (username.empty() || password_hash.empty())
    {
        return make_http_response(400, "Bad Request", "{\"error\":\"username and passwordHash are required\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    const auto it = users_.find(username);
    if (it == users_.end() || it->second.password_hash != password_hash)
    {
        return make_http_response(401, "Unauthorized", "{\"error\":\"Invalid credentials\"}");
    }

    const std::string token = random_token();
    sessions_[token] = username;

    std::ostringstream json;
    json << "{\"token\":\"" << token << "\",\"username\":\"" << json_escape(username) << "\"}";
    return make_http_response(200, "OK", json.str());
}

std::string ChatServer::handle_get_public_key(const std::string &target)
{
    const std::string prefix = "/api/users/";
    const std::string suffix = "/public-key";

    if (target.size() <= prefix.size() + suffix.size())
    {
        return make_http_response(400, "Bad Request", "{\"error\":\"Invalid user path\"}");
    }

    const std::string username = target.substr(prefix.size(), target.size() - prefix.size() - suffix.size());
    if (username.empty())
    {
        return make_http_response(400, "Bad Request", "{\"error\":\"Invalid user path\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    const auto it = users_.find(username);
    if (it == users_.end())
    {
        return make_http_response(404, "Not Found", "{\"error\":\"User not found\"}");
    }

    std::ostringstream json;
    json << "{\"username\":\"" << json_escape(it->second.username)
         << "\",\"publicKey\":\"" << json_escape(it->second.public_key) << "\"}";
    return make_http_response(200, "OK", json.str());
}

std::string ChatServer::handle_post_message(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &body)
{
    const std::string sender = authenticate(headers);
    if (sender.empty())
    {
        return make_http_response(401, "Unauthorized", "{\"error\":\"Missing or invalid bearer token\"}");
    }

    const std::string to = trim(json_get_string(body, "to"));
    const std::string message_type = to_lower(trim(json_get_string(body, "messageType")));
    const std::string ciphertext = json_get_string(body, "ciphertext");
    const std::string nonce = json_get_string(body, "nonce");
    std::string encryption = trim(json_get_string(body, "encryption"));
    const std::string key_ciphertext = trim(json_get_string(body, "keyCiphertext"));
    const std::string key_nonce = trim(json_get_string(body, "keyNonce"));
    const std::string key_encryption = trim(json_get_string(body, "keyEncryption"));
    const std::string file_name = trim(json_get_string(body, "fileName"));
    const std::string file_mime = trim(json_get_string(body, "fileMime"));
    const std::string file_size = trim(json_get_string(body, "fileSize"));

    if (to.empty() || ciphertext.empty() || nonce.empty())
    {
        return make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"to, ciphertext and nonce are required\"}");
    }

    if (!message_type.empty() && message_type != "text")
    {
        return make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"Only text messages are supported\"}");
    }

    if (!key_ciphertext.empty() || !key_nonce.empty() || !key_encryption.empty() || !file_name.empty() ||
        !file_mime.empty() || !file_size.empty())
    {
        return make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"Media/file payloads are not supported\"}");
    }

    if (encryption.empty())
    {
        encryption = "unknown";
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (users_.find(to) == users_.end())
    {
        return make_http_response(404, "Not Found", "{\"error\":\"Recipient not found\"}");
    }

    Message msg{
        next_message_id_++,
        sender,
        to,
        ciphertext,
        nonce,
        encryption,
        now_iso8601_utc()};

    messages_.push_back(msg);

    std::ostringstream json;
    json << "{\"status\":\"accepted\",\"messageId\":" << msg.id
         << ",\"routed\":true,\"storedCiphertextOnly\":true}";

    return make_http_response(202, "Accepted", json.str());
}

std::string ChatServer::handle_get_messages(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &target)
{
    const std::string requester = authenticate(headers);
    if (requester.empty())
    {
        return make_http_response(401, "Unauthorized", "{\"error\":\"Missing or invalid bearer token\"}");
    }

    const auto query = parse_query(target);
    const auto it_peer = query.find("with");
    if (it_peer == query.end() || trim(it_peer->second).empty())
    {
        return make_http_response(400, "Bad Request", "{\"error\":\"Query parameter with is required\"}");
    }

    const std::string peer = trim(it_peer->second);

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (users_.find(peer) == users_.end())
    {
        return make_http_response(404, "Not Found", "{\"error\":\"Peer user not found\"}");
    }

    std::ostringstream json;
    json << "{\"with\":\"" << json_escape(peer) << "\",\"messages\":[";

    bool first = true;
    for (const Message &msg : messages_)
    {
        const bool matches = (msg.from == requester && msg.to == peer) || (msg.from == peer && msg.to == requester);
        if (!matches)
        {
            continue;
        }

        if (!first)
        {
            json << ',';
        }
        first = false;

        json << "{\"id\":" << msg.id
             << ",\"from\":\"" << json_escape(msg.from)
             << "\",\"to\":\"" << json_escape(msg.to)
             << "\",\"ciphertext\":\"" << json_escape(msg.ciphertext)
             << "\",\"nonce\":\"" << json_escape(msg.nonce)
             << "\",\"encryption\":\"" << json_escape(msg.encryption)
             << "\",\"timestamp\":\"" << json_escape(msg.timestamp)
             << "\"}";
    }

    json << "]}";
    return make_http_response(200, "OK", json.str());
}

std::string ChatServer::authenticate(const std::unordered_map<std::string, std::string> &headers)
{
    std::string token;

    const auto it_auth = headers.find("authorization");
    if (it_auth != headers.end())
    {
        const std::string auth = trim(it_auth->second);
        const std::string bearer_prefix = "bearer ";
        if (auth.size() > bearer_prefix.size() &&
            to_lower(auth.substr(0, bearer_prefix.size())) == bearer_prefix)
        {
            token = trim(auth.substr(bearer_prefix.size()));
        }
        else
        {
            token = trim(auth);
        }
    }

    if (token.empty())
    {
        const auto it_fallback = headers.find("x-auth-token");
        if (it_fallback != headers.end())
        {
            token = trim(it_fallback->second);
        }
    }

    if (token.empty())
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    const auto token_it = sessions_.find(token);
    if (token_it == sessions_.end())
    {
        return {};
    }

    return token_it->second;
}
std::string ChatServer::make_http_response(int status_code, const std::string &status_text, const std::string &json_body)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << json_body.size() << "\r\n"
             << "Connection: close\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token\r\n"
             << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
             << "\r\n"
             << json_body;

    return response.str();
}
