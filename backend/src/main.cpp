#include "chat_server.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    int port = 8080;

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
    std::cout << "Press Enter to stop...\n";
    std::string line;
    std::getline(std::cin, line);

    server.stop();
    std::cout << "Server stopped.\n";
    return EXIT_SUCCESS;
}
