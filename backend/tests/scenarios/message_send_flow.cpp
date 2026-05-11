#include "scenario_helpers.hpp"

#include <iostream>

// Сценарий: отправка сообщения — проверяем авторизацию и сохранение сообщения
int main()
{
    try
    {
        // Создаём тестовый сервер и регистрируем участников диалога
        auto server = scenario_helpers::make_server();
        scenario_helpers::register_user(server, "alice");
        scenario_helpers::register_user(server, "bob");

        const std::string login = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
        const std::string token = scenario_helpers::extract_token(login);
        scenario_helpers::require_ok(!token.empty(), "token should not be empty");

        const std::string unauthorized = ChatServerTestAccess::handle_post_message(server, {}, R"({"to":"bob","ciphertext":"cipher","nonce":"nonce"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(unauthorized) == 401, "unauthorized post should return 401");

        const std::string accepted = ChatServerTestAccess::handle_post_message(
            server,
            {{"authorization", "Bearer " + token}},
            R"({"to":"bob","messageType":"text","ciphertext":"cipher","nonce":"nonce","encryption":"ecdh"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(accepted) == 202, "message send should return 202");
        scenario_helpers::require_ok(ChatServerTestAccess::messages(server).size() == 1, "message should be stored");

        std::cout << "message_send_flow: OK\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "message_send_flow: FAILED: " << error.what() << '\n';
        return 1;
    }
}