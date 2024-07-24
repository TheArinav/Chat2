// main.cpp
#include <iostream>
#include <csignal>
#include <memory>
#include "src/testing/TestScript1.h"

using namespace std;
using namespace classes::server_side;
using namespace testing;

void signalHandler(int signum) {
    std::cout << "\n\nInterrupt signal (" << signum << ") received.\n";

    if (Test1::ServerInstance) {
        Test1::ServerInstance->Stop();
    }

    // Exit the program
    exit(signum);
}

int main() {
    cout << "Starting the test (TestScript1.h):\n\n";
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Test1::StartTest();
    return 0;
}
