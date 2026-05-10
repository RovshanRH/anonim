#include "request_utils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>

namespace request_utils
{
    std::string to_lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string trim(const std::string &value)
    {
        std::size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
        {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return value.substr(start, end - start);
    }

    std::string json_escape(const std::string &input)
    {
        std::ostringstream oss;
        for (char c : input)
        {
            switch (c)
            {
            case '\\':
                oss << "\\\\";
                break;
            case '"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
            }
        }
        return oss.str();
    }

    std::string json_unescape(std::string input)
    {
        std::string out;
        out.reserve(input.size());

        for (std::size_t i = 0; i < input.size(); ++i)
        {
            if (input[i] == '\\' && i + 1 < input.size())
            {
                const char n = input[i + 1];
                if (n == 'n')
                {
                    out.push_back('\n');
                    ++i;
                    continue;
                }
                if (n == 'r')
                {
                    out.push_back('\r');
                    ++i;
                    continue;
                }
                if (n == 't')
                {
                    out.push_back('\t');
                    ++i;
                    continue;
                }
                if (n == '\\' || n == '"' || n == '/')
                {
                    out.push_back(n);
                    ++i;
                    continue;
                }
            }

            out.push_back(input[i]);
        }

        return out;
    }

    std::string json_get_string(const std::string &body, const std::string &key)
    {
        const std::string needle = "\"" + key + "\"";
        const std::size_t key_pos = body.find(needle);
        if (key_pos == std::string::npos)
        {
            return {};
        }

        std::size_t pos = key_pos + needle.size();
        while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])) != 0)
        {
            ++pos;
        }

        if (pos >= body.size() || body[pos] != ':')
        {
            return {};
        }

        ++pos;
        while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])) != 0)
        {
            ++pos;
        }

        if (pos >= body.size() || body[pos] != '"')
        {
            return {};
        }

        ++pos;
        std::string raw_value;
        bool escaped = false;
        for (; pos < body.size(); ++pos)
        {
            const char current = body[pos];
            if (escaped)
            {
                raw_value.push_back('\\');
                raw_value.push_back(current);
                escaped = false;
                continue;
            }

            if (current == '\\')
            {
                escaped = true;
                continue;
            }

            if (current == '"')
            {
                break;
            }

            raw_value.push_back(current);
        }

        return json_unescape(raw_value);
    }

    std::unordered_map<std::string, std::string> parse_query(const std::string &target)
    {
        std::unordered_map<std::string, std::string> query;
        const std::size_t pos = target.find('?');
        if (pos == std::string::npos || pos + 1 >= target.size())
        {
            return query;
        }

        const std::string query_part = target.substr(pos + 1);
        std::size_t start = 0;
        while (start < query_part.size())
        {
            const std::size_t amp = query_part.find('&', start);
            const std::string piece = query_part.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
            const std::size_t eq = piece.find('=');
            if (eq != std::string::npos)
            {
                query[piece.substr(0, eq)] = piece.substr(eq + 1);
            }
            else
            {
                query[piece] = "";
            }

            if (amp == std::string::npos)
            {
                break;
            }
            start = amp + 1;
        }

        return query;
    }

    std::string path_only(const std::string &target)
    {
        const std::size_t pos = target.find('?');
        if (pos == std::string::npos)
        {
            return target;
        }
        return target.substr(0, pos);
    }

    std::string now_iso8601_utc()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::tm tm_utc{};
#ifdef _WIN32
        gmtime_s(&tm_utc, &time_t_now);
#else
        gmtime_r(&time_t_now, &tm_utc);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    std::string random_token(std::size_t size)
    {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 15);

        std::string token;
        token.reserve(size);

        for (std::size_t i = 0; i < size; ++i)
        {
            const int value = dist(rng);
            token.push_back(static_cast<char>(value < 10 ? ('0' + value) : ('a' + (value - 10))));
        }

        return token;
    }

    std::string make_http_response(int status_code, const std::string &status_text, const std::string &json_body)
    {
        std::ostringstream response;
        response << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n"
                 << "Content-Type: application/json\r\n"
                 << "Content-Length: " << json_body.size() << "\r\n"
                 << "Connection: close\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Access-Control-Allow-Headers: Content-Type, Authorization, X-Auth-Token\r\n"
                 << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 << "\r\n"
                 << json_body;

        return response.str();
    }
}