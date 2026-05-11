#pragma once

#include "support/chat_server_test_access.hpp"

#include <stdexcept>
#include <string>

namespace scenario_helpers
{
    inline int status_code(const std::string &response)
    {
        const std::size_t first_space = response.find(' ');
        const std::size_t second_space = response.find(' ', first_space + 1);
        return std::stoi(response.substr(first_space + 1, second_space - first_space - 1));
    }

    inline std::string body_from_response(const std::string &response)
    {
        const std::size_t body_pos = response.find("\r\n\r\n");
        return body_pos == std::string::npos ? std::string{} : response.substr(body_pos + 4);
    }

    inline ChatServer make_server()
    {
        return ChatServer(0);
    }

    inline void require_ok(bool condition, const std::string &message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    inline void register_user(ChatServer &server, const std::string &username, const std::string &password_hash = "hash", const std::string &public_key = "pub")
    {
        const std::string body = "{\"username\":\"" + username + "\",\"passwordHash\":\"" + password_hash + "\",\"publicKey\":\"" + public_key + "\"}";
        require_ok(ChatServerTestAccess::handle_register(server, body).find("201 Created") != std::string::npos, "register_user failed");
    }

    inline std::string extract_token(const std::string &response)
    {
        return request_utils::json_get_string(body_from_response(response), "token");
    }

    inline void assert_token_not_empty(const std::string &response)
    {
        require_ok(!extract_token(response).empty(), "token is empty");
    }
}