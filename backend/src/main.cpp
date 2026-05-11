#include "chat_server.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
    std::atomic<bool> g_running{true};

    void handle_signal(int)
    {
        g_running.store(false);
    }

    bool is_stdin_interactive()
    {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) != 0;
#else
        return isatty(fileno(stdin)) != 0;
#endif
    }
} // namespace

int main(int argc, char *argv[])
{
    int port = 8080;

    // Порт можно переопределить аргументом командной строки.
    if (argc > 1)
    {
        try
        {
            port = std::stoi(argv[1]);
        }
        catch (const std::exception &)
        {
            std::cerr << "Invalid port argument. Usage: anonim_server [port]\n";
            return EXIT_FAILURE;
        }
    }

    ChatServer server(port);
    if (!server.start())
    {
        std::cerr << "Failed to start server on port " << port << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "anonim_server is running on port " << port << "\n";

    // Ждём Ctrl+C или Enter в интерактивном режиме.
    std::signal(SIGINT, handle_signal);
#ifdef SIGTERM
    std::signal(SIGTERM, handle_signal);
#endif

    if (is_stdin_interactive())
    {
        std::cout << "Press Enter to stop...\n";
        std::string line;
        std::getline(std::cin, line);
    }
    else
    {
        while (g_running.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    server.stop();
    std::cout << "Server stopped.\n";
    return EXIT_SUCCESS;
}
