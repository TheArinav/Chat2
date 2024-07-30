// main.cpp
#include <iostream>
#include <csignal>
#include <memory>
#include "src/terminal/Terminal.h"

using namespace std;
using namespace classes::server_side;
using namespace terminal;

void signalHandler(int signum) {
    cout << "\n\nInterrupt signal (" << signum << ") received.\n";

    if (Terminal::CurrentServer) {
        Terminal::CurrentServer->Stop();
    }

    // Exit the program
    exit(signum);
}

int main() {

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    Terminal::StartTerminal();
    return 0;
}
