#pragma once

#include "chat_server.hpp"

#include "request_utils.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

class ChatServerTestAccess
{
public:
    static std::string route_request(ChatServer &server, const std::string &method, const std::string &target,
                                     const std::unordered_map<std::string, std::string> &headers, const std::string &body)
    {
        return server.route_request(method, target, headers, body);
    }

    static std::string handle_health(ChatServer &server) { return server.handle_health(); }
    static std::string handle_register(ChatServer &server, const std::string &body) { return server.handle_register(body); }
    static std::string handle_login(ChatServer &server, const std::string &body) { return server.handle_login(body); }
    static std::string handle_get_public_key(ChatServer &server, const std::string &target) { return server.handle_get_public_key(target); }
    static std::string handle_post_message(ChatServer &server, const std::unordered_map<std::string, std::string> &headers, const std::string &body)
    {
        return server.handle_post_message(headers, body);
    }
    static std::string handle_get_messages(ChatServer &server, const std::unordered_map<std::string, std::string> &headers, const std::string &target)
    {
        return server.handle_get_messages(headers, target);
    }
    static std::string authenticate(ChatServer &server, const std::unordered_map<std::string, std::string> &headers)
    {
        return server.authenticate(headers);
    }

    static auto &users(ChatServer &server) { return server.users_; }
    static auto &sessions(ChatServer &server) { return server.sessions_; }
    static auto &messages(ChatServer &server) { return server.messages_; }
    static std::uint64_t &next_message_id(ChatServer &server) { return server.next_message_id_; }
};