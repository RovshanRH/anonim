#include "scenario_helpers.hpp"

#include <iostream>

// Сценарий: проверка получения публичного ключа зарегистрированного и незарегистрированного пользователя
int main()
{
    try
    {
        // Создаём тестовый сервер и заранее регистрируем пользователя
        auto server = scenario_helpers::make_server();
        scenario_helpers::register_user(server, "alice", "hash", "public-key-1");

        const std::string ok = ChatServerTestAccess::handle_get_public_key(server, "/api/users/alice/public-key");
        scenario_helpers::require_ok(scenario_helpers::status_code(ok) == 200, "public-key lookup should return 200");
        scenario_helpers::require_ok(scenario_helpers::body_from_response(ok).find("public-key-1") != std::string::npos, "public key payload missing");

        const std::string missing = ChatServerTestAccess::handle_get_public_key(server, "/api/users/bob/public-key");
        scenario_helpers::require_ok(scenario_helpers::status_code(missing) == 404, "unknown user should return 404");

        std::cout << "public_key_flow: OK\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "public_key_flow: FAILED: " << error.what() << '\n';
        return 1;
    }
}