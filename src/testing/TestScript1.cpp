// TestScript1.cpp
#include <thread>
#include <iostream>
#include <atomic>
#include "TestScript1.h"
#include "../classes/client_side/ServerConnection.h"
#include "../classes/server_side/Server.h"

using namespace std;
using namespace classes::server_side;
using namespace classes::client_side;

namespace testing::Test1 {
    std::shared_ptr<Server> ServerInstance = nullptr;

    void StartTest() {
        atomic<bool> ServerBuilt ={}, ExitFlag1= {};
        ExitFlag1.store(false);
        ServerBuilt.store(false);
        auto *serverT = new thread([&ServerBuilt,&ExitFlag1]() -> void {
            ServerInstance = make_shared<Server>("TestServer");
            ServerInstance->Start();
            ServerBuilt.store(true);
            while(!ExitFlag1.load());
            exit(EXIT_SUCCESS);
        });
        auto *clientT = new thread([&ServerBuilt,&ExitFlag1]() -> void {
            while(!ServerBuilt.load());
            auto client = ServerConnection((string&&)"127.0.0.1");
            if(!client.Connect(true, -1, (string&&)"myKey123", (string&&)"TestClient"))
                exit(EXIT_FAILURE);

            cout << "\nClient code reached the end of execution. Ending test.\n";

            ExitFlag1.store(true);
            exit(EXIT_SUCCESS);
        });
        serverT->detach();
        clientT->detach();
        while (1);
    }
}
