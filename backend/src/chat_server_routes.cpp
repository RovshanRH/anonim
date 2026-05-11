#include "chat_server.hpp"

#include "request_utils.hpp"

#include <sstream>

std::string ChatServer::route_request(
    const std::string &method,
    const std::string &target,
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &body)
{
    // Выбирает обработчик по методу и пути.
    const std::string path = request_utils::path_only(target);

    if (method == "OPTIONS")
    {
        return request_utils::make_http_response(200, "OK", "{}");
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

    return request_utils::make_http_response(404, "Not Found", "{\"error\":\"Route not found\"}");
}

std::string ChatServer::handle_health() const
{
    // Простая проверка живости сервиса.
    return request_utils::make_http_response(200, "OK", "{\"status\":\"ok\",\"service\":\"anonim-backend\"}");
}

std::string ChatServer::handle_register(const std::string &body)
{
    // Регистрирует пользователя и сохраняет его публичный ключ.
    const std::string username = request_utils::trim(request_utils::json_get_string(body, "username"));
    const std::string password_hash = request_utils::trim(request_utils::json_get_string(body, "passwordHash"));
    const std::string public_key = request_utils::trim(request_utils::json_get_string(body, "publicKey"));

    if (username.empty() || password_hash.empty() || public_key.empty())
    {
        return request_utils::make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"username, passwordHash and publicKey are required\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (users_.find(username) != users_.end())
    {
        return request_utils::make_http_response(409, "Conflict", "{\"error\":\"User already exists\"}");
    }

    users_[username] = User{username, password_hash, public_key};
    return request_utils::make_http_response(201, "Created", "{\"status\":\"registered\"}");
}

std::string ChatServer::handle_login(const std::string &body)
{
    // Проверяет пароль и выдаёт токен сессии.
    const std::string username = request_utils::trim(request_utils::json_get_string(body, "username"));
    const std::string password_hash = request_utils::trim(request_utils::json_get_string(body, "passwordHash"));

    if (username.empty() || password_hash.empty())
    {
        return request_utils::make_http_response(400, "Bad Request", "{\"error\":\"username and passwordHash are required\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    const auto it = users_.find(username);
    if (it == users_.end() || it->second.password_hash != password_hash)
    {
        return request_utils::make_http_response(401, "Unauthorized", "{\"error\":\"Invalid credentials\"}");
    }

    const std::string token = request_utils::random_token();
    sessions_[token] = username;

    std::ostringstream json;
    json << "{\"token\":\"" << token << "\",\"username\":\"" << request_utils::json_escape(username) << "\"}";
    return request_utils::make_http_response(200, "OK", json.str());
}

std::string ChatServer::handle_get_public_key(const std::string &target)
{
    // Отдаёт публичный ключ пользователя по его логину.
    const std::string prefix = "/api/users/";
    const std::string suffix = "/public-key";

    if (target.size() <= prefix.size() + suffix.size())
    {
        return request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Invalid user path\"}");
    }

    const std::string username = target.substr(prefix.size(), target.size() - prefix.size() - suffix.size());
    if (username.empty())
    {
        return request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Invalid user path\"}");
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    const auto it = users_.find(username);
    if (it == users_.end())
    {
        return request_utils::make_http_response(404, "Not Found", "{\"error\":\"User not found\"}");
    }

    std::ostringstream json;
    json << "{\"username\":\"" << request_utils::json_escape(it->second.username)
         << "\",\"publicKey\":\"" << request_utils::json_escape(it->second.public_key) << "\"}";
    return request_utils::make_http_response(200, "OK", json.str());
}

std::string ChatServer::handle_post_message(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &body)
{
    // Принимает только зашифрованные сообщения.
    const std::string sender = authenticate(headers);
    if (sender.empty())
    {
        return request_utils::make_http_response(401, "Unauthorized", "{\"error\":\"Missing or invalid bearer token\"}");
    }

    const std::string to = request_utils::trim(request_utils::json_get_string(body, "to"));
    const std::string message_type = request_utils::to_lower(request_utils::trim(request_utils::json_get_string(body, "messageType")));
    const std::string ciphertext = request_utils::json_get_string(body, "ciphertext");
    const std::string nonce = request_utils::json_get_string(body, "nonce");
    std::string encryption = request_utils::trim(request_utils::json_get_string(body, "encryption"));
    const std::string key_ciphertext = request_utils::trim(request_utils::json_get_string(body, "keyCiphertext"));
    const std::string key_nonce = request_utils::trim(request_utils::json_get_string(body, "keyNonce"));
    const std::string key_encryption = request_utils::trim(request_utils::json_get_string(body, "keyEncryption"));
    const std::string file_name = request_utils::trim(request_utils::json_get_string(body, "fileName"));
    const std::string file_mime = request_utils::trim(request_utils::json_get_string(body, "fileMime"));
    const std::string file_size = request_utils::trim(request_utils::json_get_string(body, "fileSize"));

    if (to.empty() || ciphertext.empty() || nonce.empty())
    {
        return request_utils::make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"to, ciphertext and nonce are required\"}");
    }

    if (!message_type.empty() && message_type != "text")
    {
        return request_utils::make_http_response(
            400,
            "Bad Request",
            "{\"error\":\"Only text messages are supported\"}");
    }

    if (!key_ciphertext.empty() || !key_nonce.empty() || !key_encryption.empty() || !file_name.empty() ||
        !file_mime.empty() || !file_size.empty())
    {
        return request_utils::make_http_response(
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
        return request_utils::make_http_response(404, "Not Found", "{\"error\":\"Recipient not found\"}");
    }

    Message msg{
        next_message_id_++,
        sender,
        to,
        ciphertext,
        nonce,
        encryption,
        request_utils::now_iso8601_utc()};

    messages_.push_back(msg);

    std::ostringstream json;
    json << "{\"status\":\"accepted\",\"messageId\":" << msg.id
         << ",\"routed\":true,\"storedCiphertextOnly\":true}";

    return request_utils::make_http_response(202, "Accepted", json.str());
}

std::string ChatServer::handle_get_messages(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &target)
{
    // Возвращает переписку только между двумя участниками.
    const std::string requester = authenticate(headers);
    if (requester.empty())
    {
        return request_utils::make_http_response(401, "Unauthorized", "{\"error\":\"Missing or invalid bearer token\"}");
    }

    const auto query = request_utils::parse_query(target);
    const auto it_peer = query.find("with");
    if (it_peer == query.end() || request_utils::trim(it_peer->second).empty())
    {
        return request_utils::make_http_response(400, "Bad Request", "{\"error\":\"Query parameter with is required\"}");
    }

    const std::string peer = request_utils::trim(it_peer->second);

    std::lock_guard<std::mutex> lock(data_mutex_);
    if (users_.find(peer) == users_.end())
    {
        return request_utils::make_http_response(404, "Not Found", "{\"error\":\"Peer user not found\"}");
    }

    std::ostringstream json;
    json << "{\"with\":\"" << request_utils::json_escape(peer) << "\",\"messages\":[";

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
             << ",\"from\":\"" << request_utils::json_escape(msg.from)
             << "\",\"to\":\"" << request_utils::json_escape(msg.to)
             << "\",\"ciphertext\":\"" << request_utils::json_escape(msg.ciphertext)
             << "\",\"nonce\":\"" << request_utils::json_escape(msg.nonce)
             << "\",\"encryption\":\"" << request_utils::json_escape(msg.encryption)
             << "\",\"timestamp\":\"" << request_utils::json_escape(msg.timestamp)
             << "\"}";
    }

    json << "]}";
    return request_utils::make_http_response(200, "OK", json.str());
}

std::string ChatServer::authenticate(const std::unordered_map<std::string, std::string> &headers)
{
    // Ищет токен в заголовках и сверяет его сессией.
    std::string token;

    const auto it_auth = headers.find("authorization");
    if (it_auth != headers.end())
    {
        const std::string auth = request_utils::trim(it_auth->second);
        const std::string bearer_prefix = "bearer ";
        if (auth.size() > bearer_prefix.size() &&
            request_utils::to_lower(auth.substr(0, bearer_prefix.size())) == bearer_prefix)
        {
            token = request_utils::trim(auth.substr(bearer_prefix.size()));
        }
        else
        {
            token = request_utils::trim(auth);
        }
    }

    if (token.empty())
    {
        const auto it_fallback = headers.find("x-auth-token");
        if (it_fallback != headers.end())
        {
            token = request_utils::trim(it_fallback->second);
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