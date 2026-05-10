#include <catch2/catch_test_macros.hpp>

// Файл содержит модульные тесты для вспомогательных функций работы с запросами и JSON.
#include "request_utils.hpp"

#include <algorithm>

// Проверяем корректность преобразования символов в нижний регистр
TEST_CASE("request_utils::to_lower handles mixed ASCII input", "[request_utils]")
{
    REQUIRE(request_utils::to_lower("AbC123!") == "abc123!");
    REQUIRE(request_utils::to_lower("ALREADY lower") == "already lower");
}

TEST_CASE("request_utils::trim removes leading and trailing whitespace", "[request_utils]")
{
    REQUIRE(request_utils::trim("  hello  ") == "hello");
    REQUIRE(request_utils::trim("\n\t spaced \r\n") == "spaced");
}

// Проверяем экранирование специальных и управляющих символов в JSON-строках
TEST_CASE("request_utils::json_escape escapes control characters", "[request_utils]")
{
    REQUIRE(request_utils::json_escape("a\"b\\c\n") == "a\\\"b\\\\c\\n");
    REQUIRE(request_utils::json_escape("line\rtab\t") == "line\\rtab\\t");
}

// Проверяем восстановление исходных символов из экранированных последовательностей
TEST_CASE("request_utils::json_unescape decodes escaped JSON sequences", "[request_utils]")
{
    REQUIRE(request_utils::json_unescape("a\\\"b\\\\c\\n") == "a\"b\\c\n");
    REQUIRE(request_utils::json_unescape("x\\ry\\tz") == "x\ry\tz");
}

TEST_CASE("request_utils::json_get_string extracts escaped values", "[request_utils]")
{
    const std::string body = R"({"name":"Alice \"The One\"","city":"Minsk","empty":""})";
    REQUIRE(request_utils::json_get_string(body, "name") == "Alice \"The One\"");
    REQUIRE(request_utils::json_get_string(body, "city") == "Minsk");
    REQUIRE(request_utils::json_get_string(body, "empty") == "");
    REQUIRE(request_utils::json_get_string(body, "missing").empty());
}

TEST_CASE("request_utils::parse_query handles multiple and missing params", "[request_utils]")
{
    const auto query = request_utils::parse_query("/api/messages?with=bob&limit=10&empty=");
    REQUIRE(query.at("with") == "bob");
    REQUIRE(query.at("limit") == "10");
    REQUIRE(query.at("empty") == "");
    REQUIRE(request_utils::parse_query("/api/messages").empty());
}

TEST_CASE("request_utils::path_only strips the query string", "[request_utils]")
{
    REQUIRE(request_utils::path_only("/api/messages?with=bob") == "/api/messages");
    REQUIRE(request_utils::path_only("/health") == "/health");
}

TEST_CASE("request_utils::now_iso8601_utc returns UTC timestamp format", "[request_utils]")
{
    const std::string timestamp = request_utils::now_iso8601_utc();
    REQUIRE(timestamp.size() == 20);
    REQUIRE(timestamp[4] == '-');
    REQUIRE(timestamp[7] == '-');
    REQUIRE(timestamp[10] == 'T');
    REQUIRE(timestamp[13] == ':');
    REQUIRE(timestamp[16] == ':');
    REQUIRE(timestamp.back() == 'Z');
}

TEST_CASE("request_utils::random_token produces requested hex length", "[request_utils]")
{
    const std::string token = request_utils::random_token(48);
    REQUIRE(token.size() == 48);
    REQUIRE(std::all_of(token.begin(), token.end(), [](unsigned char c)
                        { return std::isxdigit(c) != 0; }));
}