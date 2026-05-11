#include "scenario_helpers.hpp"

#include <iostream>

// Сценарий: регистрация пользователей и проверка ошибок валидации/дубликатов
int main()
{
    try
    {
        // Создаём тестовый сервер в памяти (без реального сетевого сокета)
        auto server = scenario_helpers::make_server();

        const std::string ok = ChatServerTestAccess::handle_register(server, R"({"username":"alice","passwordHash":"hash","publicKey":"pub"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(ok) == 201, "register should return 201");

        const std::string duplicate = ChatServerTestAccess::handle_register(server, R"({"username":"alice","passwordHash":"hash","publicKey":"pub"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(duplicate) == 409, "duplicate registration should return 409");

        const std::string invalid = ChatServerTestAccess::handle_register(server, R"({"username":"","passwordHash":"hash","publicKey":"pub"})");
        scenario_helpers::require_ok(scenario_helpers::status_code(invalid) == 400, "invalid registration should return 400");

        std::cout << "register_flow: OK\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "register_flow: FAILED: " << error.what() << '\n';
        return 1;
    }
}