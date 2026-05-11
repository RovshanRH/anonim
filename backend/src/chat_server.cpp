#include "chat_server.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

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

namespace
{
    // Builder pattern: constructs consistent HTTP responses step by step via a fluent API.
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

    // Strategy pattern: each validator encapsulates one interchangeable message-validation rule.
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

    // Command pattern: each route is represented as an executable command with its own match predicate.
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

    std::string to_lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string trim(const std::string &value)
    {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
        {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return value.substr(start, end - start);
    }

    std::string json_escape(const std::string &input)
    {
        std::ostringstream oss;
        for (char c : input)
        {
            switch (c)
            {
            case '\\':
                oss << "\\\\";
                break;
            case '"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
            }
        }
        return oss.str();
    }

    std::string json_unescape(std::string input)
    {
        std::string out;
        out.reserve(input.size());

        for (std::size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] == '\\' && i + 1 < input.size())
            {
                const char n = input[i + 1];
                if (n == 'n')
                {
                    out.push_back('\n');
                    ++i;
                    continue;
                }
                if (n == 'r')
                {
                    out.push_back('\r');
                    ++i;
                    continue;
                }
                if (n == 't')
                {
                    out.push_back('\t');
                    ++i;
                    continue;
                }
                if (n == '\\' || n == '"' || n == '/')
                {
                    out.push_back(n);
                    ++i;
                    continue;
                }
            }

            out.push_back(input[i]);
        }

        return out;
    }

    std::string json_get_string(const std::string &body, const std::string &key)
    {
        const std::regex pattern("\"" + key + "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\"");
        std::smatch match;
        if (!std::regex_search(body, match, pattern))
        {
            return {};
        }

        if (match.size() < 2)
        {
            return {};
        }

        return json_unescape(match[1].str());
    }

    std::unordered_map<std::string, std::string> parse_query(const std::string &target)
    {
        std::unordered_map<std::string, std::string> query;
        const std::size_t pos = target.find('?');
        if (pos == std::string::npos || pos + 1 >= target.size())
        {
            return query;
        }

        const std::string query_part = target.substr(pos + 1);
        std::size_t start = 0;
        while (start < query_part.size())
        {
            const std::size_t amp = query_part.find('&', start);
            const std::string piece = query_part.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            const std::size_t eq = piece.find('=');
            if (eq != std::string::npos)
            {
                query[piece.substr(0, eq)] = piece.substr(eq + 1);
            }
            else
            {
                query[piece] = "";
            }

            if (amp == std::string::npos)
            {
                break;
            }
            start = amp + 1;
        }

        return query;
    }

    std::string path_only(const std::string &target)
    {
        const std::size_t pos = target.find('?');
        if (pos == std::string::npos)
        {
            return target;
        }
        return target.substr(0, pos);
    }

    std::string now_iso8601_utc()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::tm tm_utc{};
#ifdef _WIN32
        gmtime_s(&tm_utc, &time_t_now);
#else
        gmtime_r(&time_t_now, &tm_utc);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    std::string random_token(std::size_t size = 32)
    {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 15);

        std::string token;
        token.reserve(size);

        for (std::size_t i = 0; i < size; ++i)
        {
            const int value = dist(rng);
            token.push_back(static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10))));
        }

        return token;
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
        const std::string bad = make_http_response(400, "Bad Request", "{\"error\":\"Malformed request\"}");
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
        const std::string bad = make_http_response(400, "Bad Request", "{\"error\":\"Missing request line\"}");
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
        const std::string bad = make_http_response(400, "Bad Request", "{\"error\":\"Invalid request line\"}");
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

        const std::string key = to_lower(trim(header_line.substr(0, colon)));
        const std::string value = trim(header_line.substr(colon + 1));
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

    const std::vector<RouteCommand> commands = {
        RouteCommand{
            [&](const std::string &route_path)
            { return method == "GET" && route_path == "/health"; },
            [&]()
            { return handle_health(); }},
        RouteCommand{
            [&](const std::string &route_path)
            { return method == "POST" && route_path == "/api/register"; },
            [&]()
            { return handle_register(body); }},
        RouteCommand{
            [&](const std::string &route_path)
            { return method == "POST" && route_path == "/api/login"; },
            [&]()
            { return handle_login(body); }},
        RouteCommand{
            [&](const std::string &route_path)
            {
                return method == "GET" && route_path.rfind("/api/users/", 0) == 0 &&
                       route_path.size() > std::string("/api/users//public-key").size() &&
                       route_path.find("/public-key") == route_path.size() - std::string("/public-key").size();
            },
            [&]()
            { return handle_get_public_key(path); }},
        RouteCommand{
            [&](const std::string &route_path)
            { return method == "POST" && route_path == "/api/messages"; },
            [&]()
            { return handle_post_message(headers, body); }},
        RouteCommand{
            [&](const std::string &route_path)
            { return method == "GET" && route_path == "/api/messages"; },
            [&]()
            { return handle_get_messages(headers, target); }}};

    for (const RouteCommand &command : commands)
    {
        if (command.match(path))
        {
            return command.execute();
        }
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

    const MessageValidationContext validation_context{
        to,
        message_type,
        ciphertext,
        nonce,
        key_ciphertext,
        key_nonce,
        key_encryption,
        file_name,
        file_mime,
        file_size};

    const auto validation_strategies = make_message_validation_strategies();
    for (const auto &strategy : validation_strategies)
    {
        const ValidationError error = strategy->validate(validation_context);
        if (error.has_error)
        {
            const std::string body_json = "{\"error\":\"" + json_escape(error.message) + "\"}";
            return make_http_response(400, "Bad Request", body_json);
        }
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
    // Builder usage: one place defines how a complete HTTP response should be assembled.
    return HttpResponseBuilder{}
        .status(status_code, status_text)
        .json_body(json_body)
        .allow_cors(true)
        .build();
}
