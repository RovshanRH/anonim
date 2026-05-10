#include "scenario_helpers.hpp"

#include <iostream>

// Сценарий: логин — проверка неверного и верного пароля, а также валидности токена
int main()
{
    try
    {
        // Тестовый сервер и предварительная регистрация пользователя
        auto server = scenario_helpers::make_server();
        scenario_helpers::register_user(server, "alice");

        const std::string invalid = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"wrong"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(invalid) == 401, "invalid login should return 401");

        const std::string ok = ChatServerTestAccess::handle_login(server, R"({"username":"alice","passwordHash":"hash"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(ok) == 200, "valid login should return 200");
        scenario_helpers::assert_token_not_empty(ok);
        scenario_helpers::require_ok(ChatServerTestAccess::authenticate(server, {{"authorization", "Bearer " + scenario_helpers::extract_token(ok)}}) == "alice", "token auth should resolve alice");

        std::cout << "login_flow: OK\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "login_flow: FAILED: " << error.what() << '\n';
        return 1;
    }
}