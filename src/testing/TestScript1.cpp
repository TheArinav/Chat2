#include <thread>
#include <iostream>

#include "TestScript1.h"

#include "../classes/client_side/ServerConnection.h"
#include "../classes/server_side/Server.h"

using namespace std;
using namespace classes::server_side;
using namespace classes::client_side;

namespace testing::Test1 {
    void StartTest() {
        auto *serverT = new thread([=]() -> void {
            sleep(1);
            auto serv = Server("TestServ");
            serv.Start();
        });
        auto *clientT = new thread([=]() -> void {
            sleep(5);
            auto client = ServerConnection("127.0.0.1");
            client.Connect(true, -1, "myKey123", "TestClient");
        });
        serverT->detach();
        //clientT->detach();
        while (1);
    }
}