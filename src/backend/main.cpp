#include <iostream>
#include <string>
#include <cstdlib>
#include "server.h"

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    } else {
        char* env_p = std::getenv("PORT");
        if (env_p) port = std::atoi(env_p);
    }

    std::cout << "[INFO] Initializing StudyMate 3D Backend...\n";
    studymate::Server server(port, "public", "data");
    
    if (!server.start()) {
        std::cerr << "[FATAL] Failed to start server.\n";
        return 1;
    }

    return 0;
}
