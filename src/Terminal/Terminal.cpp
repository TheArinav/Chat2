#include "Terminal.h"
#include "InstructionInterpreter.h"
#include <mutex>
#include <iostream>

using namespace terminal::InstructionInterpreter;

namespace terminal {
    stack<Context> Terminal::ContextStack = {};
    unsigned long long Terminal::curRoomID=-1;
    shared_ptr<Server> Terminal::CurrentServer = nullptr;
    shared_ptr<ServerConnection> Terminal::ServerConn = nullptr;
    vector<Account> Terminal::Accounts = {};
    mutex Terminal::ServerMutex = {};
    bool Terminal::ServerBuilt= false;

    void Terminal::PrintLogo() {
        std::cout << R"(
   ___________________________________________________
  / _________________________________________________ \
 | |        _____  ._.             ._.      ._.      | |
 | |       / ___ \ | |             | |      | |      | |
 | |      | /   \/ | |__.   .___.  | |__.   | |      | |
 | |      | |      | '_. \ /  ^  \ |  __|   | |      | |
 | |      | \___/\ | | | | | (_) | |  |__.  '-'      | |
 | |       \_____/ |_| |_| \___^._\ \____|  [=]      | |
 | |_________________________________________________| |
 | |                                                 | |
 | |   Welcome to the "Chat!" App.                   | |
 | |_________________________________________________| |
 | |                 (c)                             | |
 | |   Copyright 2024   Ariel Ayalon.                | |
 | |_________________________________________________| |
 | |                                                 | |
 | |   Please select an action. For help, type 'h'.  | |
 | |_________________________________________________| |
  \___________________________________________________/

)";
    }

    void Terminal::clearTerminal() {
        system("clear");
    }

    void Terminal::PushContext(Context cntxt) {
        if (ContextStack.top() == cntxt)
            return;
        ContextStack.push(cntxt);
    }

    void Terminal::PopContext() {
        if (ContextStack.size() <= 1)
            return;
        if (ContextStack.top() == Context::CLIENT_LOGGED_IN_ROOM)
            curRoomID=-1;
        ContextStack.pop();
    }

    string Terminal::GetContext() {
        switch (ContextStack.top()) {

            default: {
                return "[ILLEGAL_CONTEXT]";
            }
            case Context::NONE: {
                return "[No Context]";
            }
            case Context::SERVER: {
                return "[Server]";
            }
            case Context::CLIENT_LOGGED_OUT: {
                return "[Client, logged-out]";
            }
            case Context::CLIENT_LOGGED_IN: {
                return "[Client, logged-in]";
            }
            case Context::CLIENT_LOGGED_IN_ROOM: {
                return "[Client, logged-in, inside chatroom]";
            }
        }
    }

    void Terminal::HandleInstruction(const Instruction &toHandle) {
        auto&& curName = (string&&)toHandle.Type->ShortForm ;

        if(!VerifyContext(toHandle)){
            cout << "This instruction can't be performed under the current context. Instruction aborted;" << endl;
            return;
        }

        if (curName == "scx")
            cout << "Current Context is: " << GetContext() << endl;
        else if(curName == "s"){
            auto reqContext = any_cast<string>(toHandle.Params[0].Value);
            if(reqContext=="server")
                ContextStack.push(Context::SERVER);
            else if (reqContext=="client")
                ContextStack.push(Context::CLIENT_LOGGED_OUT);
            else
                cerr << "Something went wrong in the 'start' instruction";
            cout << "Successfully started '" << reqContext << "'" << endl;
        }
        else if (curName == "ecx"){
            if(ContextStack.top()==Context::NONE) {
                cout << "You can not exit a [NONE] context. Instruction aborted;" << endl;
                return;
            }
            auto prevC = ContextStack.top();
            if(prevC==Context::CLIENT_LOGGED_IN){
                bool flag=false;
                do {
                    cout << "You are currently in a [Logged In] context. Do you wish to log out? [Y/n]" << endl;
                    string sIn;
                    cin >> sIn;
                    if (sIn == "Y" || sIn == "y")
                        HandleInstruction(InstructionInterpreter::Parse("sd"));
                    else if (sIn != "N" || sIn != "n")
                    {
                        cout << "Invalid response, try again." << endl;
                        flag = true;
                        continue;
                    }
                    flag=false;
                }while (flag);
            }else if (prevC==Context::SERVER && ServerBuilt){
                bool flag = false;
                do{
                    cout << "To exit this context, you have to shutdown the server. Would you like to proceed? [Y/n]" << endl;
                    string sIn;
                    cin >> sIn;
                    if (sIn == "Y" || sIn == "y")
                    {
                        CurrentServer->Stop();
                        CurrentServer.reset();
                        ServerBuilt=false;
                    }
                    else if (sIn != "N" || sIn != "n")
                    {
                        cout << "Invalid response, try again." << endl;
                        flag = true;
                        continue;
                    }
                    flag=false;
                } while (flag);
            }
            cout << "You have left the context: " << GetContext() << endl;
            PopContext();
        }
        else if (curName == "ss"){
            auto&& name = any_cast<string>(toHandle.Params[0].Value);
            auto c_name = string(name);
            CurrentServer = make_shared<Server>((string&&)name);
            CurrentServer->Start();
            ServerBuilt = true;
            cout << "Created and started server '" << c_name << "'" << endl;
            string addr;
            {
                lock_guard<mutex> guard(ServerMutex);
                addr = CurrentServer->IPSTR;
            }
            cout << "Server address is: '" << addr << "'" << endl;
        }
        else if (curName=="sd"){
            if(!ServerBuilt){
                cout << "No server to shutdown. Instruction aborted;" << endl;
                return;
            }
            CurrentServer->Stop();
            CurrentServer.reset();
            ServerBuilt= false;
        }
        else if (curName=="reg"){
            string key;
            string dispName;
            string addr;
            for (auto cur :toHandle.Params){
                if(cur.Type->ShortForm=="-sa")
                    addr = any_cast<string>(cur.Value);
                else if(cur.Type->ShortForm=="-cdn")
                    dispName = any_cast<string>(cur.Value);
                else
                    key = any_cast<string>(cur.Value);
            }
            if(!ServerConn)
                ServerConn= make_shared<ServerConnection>(addr);
            auto Acc = ServerConn->Register(key,dispName);
            Accounts.push_back(Acc);
            cout << "Created account with assigned id= '" << Accounts[Accounts.size()-1].ID << "'" << endl;
        }
        else if(curName == "li"){
            unsigned long long id;
            string addr;
            string key;
            for (auto cur :toHandle.Params){
                if(cur.Type->ShortForm=="-i")
                    id = any_cast<unsigned long long>(cur.Value);
                else if(cur.Type->ShortForm=="-key")
                    key = any_cast<string>(cur.Value);
                else
                    addr = any_cast<string>(cur.Value);
            }
            if(!ServerConn)
                ServerConn= make_shared<ServerConnection>(addr);
            if(ServerConn->Connect(false, id,key)){
                PushContext(Context::CLIENT_LOGGED_IN);
            }else{
                cerr << "Login failed." << endl;
            }
        }else if (curName == "lo"){
            if (!ServerConn)
                return;
            if(Accounts.empty())
                return;
            stringstream ss;
            ss << Accounts[0].ID << " " << Accounts[0].ConnectionKey;
            auto resp = ServerConn->Request(ServerAction(classes::general::ServerActionType::LogoutClient,{},
                                             ss.str()));
            cout << resp.Serialize() << endl;
            PopContext();
        } else if (curName == "mcr"){
            if (!ServerConn)
                return;
            if(Accounts.empty())
                return;
            stringstream ss;
            string roomName;
            vector<unsigned long long> ids;
            for(auto cur: toHandle.Params)
                if(cur.Type->LongForm=="--name")
                    roomName=any_cast<string>(cur.Value);
                else
                    ids = any_cast<vector<unsigned long long>>(cur.Value);
            ss << Accounts[0].ID << " " << Accounts[0].ConnectionKey << " " << roomName;
            auto roomCreateInst = ServerAction(classes::general::ServerActionType::CreateChatroom,
                                               {},
                                               ss.str());
            auto resp = ServerConn->Request((ServerAction&&)roomCreateInst);
            unsigned long long roomID;
            string msg;
            ss=stringstream(resp.Data);
            ss >> roomID;
            getline(ss,msg);
            cout << msg << endl;
            Accounts[0].ChatRooms.emplace(roomName,roomID);
            for(auto curAdd:ids){
                ss = {};
                ss << Accounts[0].ID << " "
                << Accounts[0].ConnectionKey << " "
                << roomID << " "
                << curAdd;
                auto joinInst = ServerAction(classes::general::ServerActionType::AddChatRoomMember,
                                             {},
                                             ss.str());
                if(ServerConn->Request((ServerAction&&)joinInst).ActionType==classes::general::ClientActionType::InformActionFailure) {
                    cerr << "Failed to add client (id=" << curAdd << ")" << endl;
                    break;
                }
            }
        } else if (curName == "ccr"){
            if (toHandle.Params[0].Type->LongForm=="--roomName") {
                auto name = any_cast<string>(toHandle.Params[0].Value);
                if(Accounts[0].ChatRooms.find(name)==Accounts[0].ChatRooms.end()){
                    cerr << "No room by the name '" << name << "' was found on this account";
                    return;
                }
                curRoomID = Accounts[0].ChatRooms[name];
            }
            else
                curRoomID= any_cast<unsigned long long>(toHandle.Params[0].Value);
            if(ContextStack.top()!= Context::CLIENT_LOGGED_IN_ROOM)
                PushContext(Context::CLIENT_LOGGED_IN_ROOM);
            string name;
            for (auto& kvp : Accounts[0].ChatRooms)
                if (kvp.second==curRoomID)
                    name= kvp.first;
            cout << "Successfully entered room '" <<
            name << "#" << curRoomID << "'" << endl;
        } else if (curName == "msg"){
            if(curRoomID==-1)
                return;
            stringstream ss;
            ss << Accounts[0].ID << " " << Accounts[0].ConnectionKey
            << " " << curRoomID << " " << any_cast<string>(toHandle.Params[0].Value);
            auto resp = ServerConn->Request(ServerAction(classes::general::ServerActionType::SendMessage,
                                                         {},
                                                         ss.str()));
            cout << resp.Data << endl;
        } else if(curName == "sl"){
            if(!ServerBuilt)
                return;
            int i=1;
            if(CurrentServer->ServerLog.empty())
                cout<< "Server log is empty." << endl;
            else
                cout << "Printing server log:" << endl;
            for(auto& cur: CurrentServer->ServerLog)
                cout << "\tServer Log[" << i++ << "]= " << cur << endl;

        }
    }

    void Terminal::StartTerminal() {
        clearTerminal();
        PrintLogo();
        InstructionInterpreter::Setup();
        ContextStack.push(Context::NONE);
        std::string input;
        while (true) {
            std::cout << "  '---[Enter Instruction]---> ";
            std::getline(std::cin, input);
            if (input == "exit" || input == "q") break;
            else if (input == "cls" || input == "clear") {
                clearTerminal();
                PrintLogo();
                continue;
            }

            try {
                auto inst = InstructionInterpreter::Parse(input);
                HandleInstruction(inst);
            } catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << "\n";
            }
        }
    }

    bool Terminal::VerifyContext(const Instruction& check) {
        auto&& req = check.Type->ValidContext;
        auto&& cur = ContextStack.top();

        if(req==Context::ANY)
            return true;
        if(cur==req)
            return true;
        if(req == Context::CLIENT_LOGGED_IN && cur == Context::CLIENT_LOGGED_IN_ROOM)
            return true;
        if(req == Context::CLIENT_LOGGED_IN_NO_ROOM && cur == Context::CLIENT_LOGGED_IN)
            return true;

        return false;
    }
}