#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

// Утилиты для простого парсинга/формирования HTTP/JSON-строк и вспомогательных операций.
namespace request_utils
{
    // Преобразование/очистка строк.
    std::string to_lower(std::string value);
    std::string trim(const std::string &value);

    // Простейшие операции над JSON-строками (экранирование/чтение полей).
    std::string json_escape(const std::string &input);
    std::string json_unescape(std::string input);
    std::string json_get_string(const std::string &body, const std::string &key);

    // Разбор query-параметров и пути.
    std::unordered_map<std::string, std::string> parse_query(const std::string &target);
    std::string path_only(const std::string &target);

    // Время, токены и формирование HTTP-ответа.
    std::string now_iso8601_utc();
    std::string random_token(std::size_t size = 32);
    std::string make_http_response(int status_code, const std::string &status_text, const std::string &json_body);
}