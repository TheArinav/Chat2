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
#include "../classes/client_side/ServerConnection.h"

using namespace std;
using namespace classes::server_side;
using namespace classes::client_side;

namespace terminal{
    class Terminal {
    public:
        static shared_ptr<Server> CurrentServer;
        static shared_ptr<ServerConnection> ServerConn;
        static vector<Account> Accounts;

        /**
         * Start the terminal.
         */
        static void StartTerminal();
    private:
        static unsigned long long curRoomID;
        static mutex ServerMutex;
        static bool VerifyContext(const Instruction&);
        static stack<Context> ContextStack;
        static bool ServerBuilt;
        static void PrintLogo();
        static void clearTerminal();
        static void PushContext(Context);
        static void PopContext();
        static string GetContext();
        static void HandleInstruction(const Instruction&);
    };
}

#endif //CHATROOMSERVER_TERMINAL_H
