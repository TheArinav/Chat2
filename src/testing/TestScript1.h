// TestScript1.h
#ifndef CHAT2_TESTSCRIPT1_H
#define CHAT2_TESTSCRIPT1_H

#include "../classes/server_side/Server.h"
#include <memory>

namespace testing::Test1 {
    extern std::shared_ptr<classes::server_side::Server> ServerInstance;
    void StartTest();
}

#endif //CHAT2_TESTSCRIPT1_H
