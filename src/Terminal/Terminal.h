//
// Created by ariel on 7/11/2024.
//

#ifndef CHATROOMSERVER_TERMINAL_H
#define CHATROOMSERVER_TERMINAL_H

#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <stack>
#include "InstructionInterpreter.h"
#include "../classes/server_side/Server.h"
#include "../classes/client_side/Account.h"
#include "util/SafeVector.h"

namespace classes::client_side{
    class ServerConnection;
}

using namespace std;
using namespace classes::server_side;
using namespace classes::client_side;

namespace terminal{
    class Terminal {
    public:
        static shared_ptr<Server> CurrentServer;
        static shared_ptr<ServerConnection> ServerConn;
        static util::SafeVector<Account> Accounts;

        /**
         * Start the terminal.
         */
        static void StartTerminal();

        static Context GetContext();
    private:
        static stack<Context> ContextStack;
        static unsigned long long curRoomID;
        static mutex ServerMutex;
        static mutex m_Context;
        static bool VerifyContext(const Instruction&);
        static bool ServerBuilt;
        static void PrintLogo();
        static void clearTerminal();
        static void PushContext(Context);
        static void PopContext();
        static string SerializeContext();
        static void HandleInstruction(const Instruction&);
    };
}

#endif //CHATROOMSERVER_TERMINAL_H
