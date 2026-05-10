#include "chat_server.hpp"
#include "request_utils.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
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

namespace
{
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
