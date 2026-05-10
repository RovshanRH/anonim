#include "scenario_helpers.hpp"

#include <iostream>

// Сценарий: проверка получения сообщений только для указанной беседы
int main()
{
    try
    {
        // Создаём тестовый сервер и регистрируем трёх пользователей
        auto server = scenario_helpers::make_server();
        scenario_helpers::register_user(server, "alice");
        scenario_helpers::register_user(server, "bob");
        scenario_helpers::register_user(server, "carol");

        const std::string alice_login = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
        const std::string bob_login = ChatServerTestAccess::handle_login(server, R"({"username":"bob","passwordHash":"hash"})");
        const std::string alice_token = scenario_helpers::extract_token(alice_login);
        const std::string bob_token = scenario_helpers::extract_token(bob_login);

        scenario_helpers::require_ok(!alice_token.empty() && !bob_token.empty(), "tokens must not be empty");

        scenario_helpers::require_ok(
            scenario_helpers::status_code(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + alice_token}}, R"({"to":"bob","messageType":"text","ciphertext":"cipher-1","nonce":"nonce-1","encryption":"ecdh"})")) == 202,
            "alice->bob message should be accepted");
        scenario_helpers::require_ok(
            scenario_helpers::status_code(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + bob_token}}, R"({"to":"alice","messageType":"text","ciphertext":"cipher-2","nonce":"nonce-2","encryption":"ecdh"})")) == 202,
            "bob->alice message should be accepted");
        scenario_helpers::require_ok(
            scenario_helpers::status_code(ChatServerTestAccess::handle_post_message(server, {{"authorization", "Bearer " + alice_token}}, R"({"to":"carol","messageType":"text","ciphertext":"cipher-3","nonce":"nonce-3","encryption":"ecdh"})")) == 202,
            "alice->carol message should be accepted");

        const std::string ok = ChatServerTestAccess::handle_get_messages(server, {{"authorization", "Bearer " + alice_token}}, "/api/messages?with=bob");
        scenario_helpers::require_ok(scenario_helpers::status_code(ok) == 200, "conversation poll should return 200");
        const std::string body = scenario_helpers::body_from_response(ok);
        scenario_helpers::require_ok(body.find("cipher-1") != std::string::npos, "expected message missing");
        scenario_helpers::require_ok(body.find("cipher-2") != std::string::npos, "expected reply missing");
        scenario_helpers::require_ok(body.find("cipher-3") == std::string::npos, "unrelated message leaked into poll");

        std::cout << "conversation_poll_flow: OK\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "conversation_poll_flow: FAILED: " << error.what() << '\n';
        return 1;
    }
}