#include <catch2/catch_test_macros.hpp>

#include "scenario_helpers.hpp"

using namespace scenario_helpers;

// Модульные тесты для ChatServer: проверяют основные API-эндпоинты и поведение авторизации.

TEST_CASE("ChatServer starts and stops cleanly", "[chat_server]")
{
    ChatServer server(0);
    REQUIRE(server.start());
    server.stop();
    REQUIRE(server.start());
    server.stop();
}

TEST_CASE("ChatServer health endpoint responds with OK", "[chat_server]")
{
    auto server = make_server();
    const std::string response = ChatServerTestAccess::handle_health(server);
    REQUIRE(status_code(response) == 200);
    REQUIRE(body_from_response(response).find("\"service\":\"anonim-backend\"") != std::string::npos);
}

TEST_CASE("ChatServer register handles success and validation errors", "[chat_server]")
{
    auto server = make_server();

    // Проверяем валидацию полей и обработку дублирующей регистрации
    const std::string invalid = ChatServerTestAccess::handle_register(server, R"({"username":""})");
    REQUIRE(status_code(invalid) == 400);

    const std::string created = ChatServerTestAccess::handle_register(server, R"({"username":"alice","passwordHash":"hash","publicKey":"pub"})");
    REQUIRE(status_code(created) == 201);
    REQUIRE(ChatServerTestAccess::users(server).contains("alice"));

    const std::string duplicate = ChatServerTestAccess::handle_register(server, R"({"username":"alice","passwordHash":"hash","publicKey":"pub"})");
    REQUIRE(status_code(duplicate) == 409);
}

TEST_CASE("ChatServer login authenticates and stores session token", "[chat_server]")
{
    auto server = make_server();
    register_user(server, "alice");

    // Проверяем неуспешную и успешную авторизацию, а также генерацию токена
    const std::string invalid = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"wrong"})");
    REQUIRE(status_code(invalid) == 401);

    const std::string valid = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
    REQUIRE(status_code(valid) == 200);
    const std::string token = extract_token(valid);
    REQUIRE_FALSE(token.empty());
    REQUIRE(ChatServerTestAccess::authenticate(server, {{"authorization", "Bearer " + token}}) == "alice");
}

TEST_CASE("ChatServer public key lookup returns existing user data", "[chat_server]")
{
    auto server = make_server();
    register_user(server, "alice", "hash", "public-key-1");

    // Запрос публичного ключа зарегистрированного и незарегистрированного пользователя
    const std::string ok = ChatServerTestAccess::handle_get_public_key(server, "/api/users/alice/public-key");
    REQUIRE(status_code(ok) == 200);
    REQUIRE(body_from_response(ok).find("public-key-1") != std::string::npos);

    const std::string not_found = ChatServerTestAccess::handle_get_public_key(server, "/api/users/bob/public-key");
    REQUIRE(status_code(not_found) == 404);
}

TEST_CASE("ChatServer post message validates auth and stores accepted message", "[chat_server]")
{
    auto server = make_server();
    register_user(server, "alice");
    register_user(server, "bob");

    // Проверяем, что без токена запрос отклоняется, а с токеном — принимается
    const std::string unauthorized = ChatServerTestAccess::handle_post_message(server, {}, R"({"to":"bob","ciphertext":"abc","nonce":"n"})");
    REQUIRE(status_code(unauthorized) == 401);

    const std::string login = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
    const std::string token = extract_token(login);

    const std::string accepted = ChatServerTestAccess::handle_post_message(
        server,
        {{"authorization", "Bearer " + token}},
        R"({"to":"bob","messageType":"text","ciphertext":"cipher","nonce":"nonce","encryption":"ecdh"})");

    REQUIRE(status_code(accepted) == 202);
    REQUIRE(ChatServerTestAccess::messages(server).size() == 1);
    REQUIRE(ChatServerTestAccess::messages(server).front().from == "alice");
    REQUIRE(ChatServerTestAccess::messages(server).front().to == "bob");
}

TEST_CASE("ChatServer get messages returns only conversation messages", "[chat_server]")
{
    auto server = make_server();
    register_user(server, "alice");
    register_user(server, "bob");
    register_user(server, "carol");

    const std::string alice_login = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
    const std::string bob_login = ChatServerTestAccess::handle_login(server, R"({"username":"bob","passwordHash":"hash"})");
    const std::string alice_token = extract_token(alice_login);
    const std::string bob_token = extract_token(bob_login);

    REQUIRE(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + alice_token}}, R"({"to":"bob","messageType":"text","ciphertext":"cipher-1","nonce":"nonce-1","encryption":"ecdh"})").find("202 Accepted") != std::string::npos);
    REQUIRE(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + bob_token}}, R"({"to":"alice","messageType":"text","ciphertext":"cipher-2","nonce":"nonce-2","encryption":"ecdh"})").find("202 Accepted") != std::string::npos);
    REQUIRE(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + alice_token}}, R"({"to":"carol","messageType":"text","ciphertext":"cipher-3","nonce":"nonce-3","encryption":"ecdh"})").find("202 Accepted") != std::string::npos);

    // Проверяем фильтрацию сообщений по участнику беседы и обязательность параметра
    const std::string unauthorized = ChatServerTestAccess::handle_get_messages(server, {}, "/api/messages?with=bob");
    REQUIRE(status_code(unauthorized) == 401);

    const std::string missing_query = ChatServerTestAccess::handle_get_messages(server, {{"authorization", "Bearer " + alice_token}}, "/api/messages");
    REQUIRE(status_code(missing_query) == 400);

    const std::string ok = ChatServerTestAccess::handle_get_messages(server, {{"authorization", "Bearer " + alice_token}}, "/api/messages?with=bob");
    REQUIRE(status_code(ok) == 200);
    const std::string body = body_from_response(ok);
    REQUIRE(body.find("cipher-1") != std::string::npos);
    REQUIRE(body.find("cipher-2") != std::string::npos);
    REQUIRE(body.find("cipher-3") == std::string::npos);
}

TEST_CASE("ChatServer route_request handles OPTIONS and unknown routes", "[chat_server]")
{
    auto server = make_server();
    const std::string options = ChatServerTestAccess::route_request(server, "OPTIONS", "/any", {}, "");
    REQUIRE(status_code(options) == 200);

    const std::string not_found = ChatServerTestAccess::route_request(server, "GET", "/unknown", {}, "");
    REQUIRE(status_code(not_found) == 404);
}