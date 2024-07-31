#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

#include "ServerConnection.h"
#include "../../terminal/Terminal.h"

using namespace terminal;

namespace classes::client_side {
    const char *port = "3490";

    ServerConnection::ServerConnection(const string &Address) {
        ServerFD = -1;
        Initilized = true;
        Receiver = nullptr;
        Expecting = ExpectStatus::None;
        Connected = make_unique<atomic<bool>>();
        Connected->store(false);
        OutgoingRequests = {};
        IngoingResponses = {};
        PoppedEmpty = make_unique<atomic<bool>>(false);  // Initialize PoppedEmpty
        PoppedEmpty->store(false);  // Set an initial value
        if (!Setup(Address)) {
            std::cerr << "Failed to establish connection to the server!\n";
            Initilized = false;
        }
    }

    ServerConnection::~ServerConnection() {
        Connected->store(false);
        if (ResponseProcessor && ResponseProcessor->joinable())
            ResponseProcessor->join();
        delete ResponseProcessor;
        if (Receiver && Receiver->joinable())
            Receiver->join();
        delete Receiver;
        if (ServerFD != -1)
            close(ServerFD);
        freeaddrinfo(&*ServerSocket);
    }

    bool ServerConnection::Setup(const string &Address) {
        TargetClient = {};
        AddressInfo hints, *res, *p;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(Address.c_str(), port, &hints, &res);
        if (status != 0) {
            return false;
        }

        for (p = res; p != nullptr; p = p->ai_next) {
            ServerFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (ServerFD == -1)
                continue;
            if (connect(ServerFD, p->ai_addr, p->ai_addrlen) == -1) {
                close(ServerFD);
                cerr << strerror(errno) << "\n";
                continue;
            }
            break;
        }
        if (p == nullptr)
            return false;

        return true;
    }

    bool ServerConnection::Connect(bool Register, unsigned long long int id, const string &key, const string &DisplayName) {
        if (!Initilized)
            return false;
        if (Register) {
            if (DisplayName.empty())
                return false;
            stringstream ss{};
            ss << DisplayName << " " << key;
            auto regAct = ServerAction(ServerActionType::RegisterClient, {}, ss.str());
            string s_regAct = regAct.Serialize();
            send(ServerFD, s_regAct.c_str(), s_regAct.size(), 0);


            char buffer[1024];
            ssize_t bytesReceived = recv(ServerFD, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived < 0)
                return false;

            buffer[bytesReceived] = '\0';
            string s_resp(buffer);

            auto response = ClientAction::Deserialize(s_resp);
            ss = stringstream(response.Data);
            ss >> id;
        }

        TargetClient.ID = id;
        TargetClient.ConnectionKey = key;

        stringstream ss;
        ss << id << " " << key;
        auto logginAct = ServerAction(ServerActionType::LoginClient,
                                      {},
                                      ss.str());
        string s_logginAct = logginAct.Serialize();
        send(ServerFD, s_logginAct.c_str(), s_logginAct.size(), 0);

        char buffer[1024];
        ssize_t bytesReceived = recv(ServerFD, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived < 0)
            return false;

        buffer[bytesReceived] = '\0';
        string s_resp(buffer);

        auto response = ClientAction::Deserialize(s_resp);
        if (response.ActionType == ClientActionType::InformActionFailure)
            return false;
        ss = stringstream(response.Data);
        string ServName, RespMsg;
        ss >> ServName;
        getline(ss, RespMsg);
        cout << RespMsg << "\n\tServer Name= '" << ServName << "'\n";
        Connected->store(true);
        Receiver = new thread([this]()->void{
            while(Connected->load()){
                char buffer[1024];
                ssize_t bytesReceived = recv(ServerFD, buffer, sizeof(buffer) - 1, 0);
                if (bytesReceived < 0)
                    return;

                buffer[bytesReceived] = '\0';
                string r_next;
                char *deli;
                if ((deli=strchr(buffer,'$'))!= nullptr) {
                    r_next = string(deli);
                    *deli='\0';
                }
                string s_resp(buffer);

                auto response = ClientAction::Deserialize(s_resp);
                if(response.ActionType != general::ClientActionType::MessageReceived)
                    PushResp(std::move(response));
                else
                    PushMess(std::move(response));

                if (!r_next.empty()) {
                    response = ClientAction::Deserialize(r_next);
                    if(response.ActionType != general::ClientActionType::MessageReceived)
                        PushResp(std::move(response));
                    else
                        PushMess(std::move(response));
                }
            }
        });

        ResponseProcessor = new thread([this]()->void {
            while (Connected->load()) {
                auto cont = Terminal::GetContext();
                auto validContext = (cont == terminal::Context::CLIENT_LOGGED_IN || cont == terminal::Context::CLIENT_LOGGED_IN_ROOM);
                if (validContext) {
                    auto mess = PopMess();
                    if (!PoppedEmpty->load()) {
                        stringstream ss(mess.Data);
                        unsigned long long Sender, Room;
                        string msgContent;
                        ss >> Sender >> Room;
                        getline(ss, msgContent);

                        string RoomName;
                        for (auto &curR: Terminal::Accounts[0].ChatRooms)
                            if (curR.second == Room)
                                RoomName = curR.first;

                        cout << "[INFO]: Received Message from chatroom [" << RoomName
                             << "#" << Room << "] with sender id '" << Sender << "'.\n\tMessage:" << msgContent
                             << endl;
                        Terminal::Accounts[0].Messages.emplace_back(Room, Sender, msgContent);
                    }
                }
                switch (Expecting.load()) {
                    case ExpectStatus::None: {
                        auto resp = PopResp();
                        if (resp.ActionType == general::ClientActionType::InformActionSuccess ||
                            resp.ActionType == general::ClientActionType::InformActionFailure) {
                            PushResp(move(resp));
                            continue;
                        }
                        if (resp.ActionType == general::ClientActionType::JoinedChatroom) {
                            stringstream ss(resp.Data);
                            unsigned long long id;
                            string name;
                            ss >> id;
                            getline(ss, name);
                            terminal::Terminal::Accounts[0].ChatRooms.emplace(name, id);
                            cout << "[INFO]: You were added to the chatroom [" << name << "#" << id << "]" << endl;
                        }
                        break;
                    }
                    default: {
                        Expecting.store(ExpectStatus::None);
                        break;
                    }
                }
            }
        });
        return true;
    }

    Account ServerConnection::Register(const string& key, const string& DisplayName) {
        if (DisplayName.empty())
            return {};
        stringstream ss{};
        ss << DisplayName << " " << key;
        auto regAct = ServerAction(ServerActionType::RegisterClient, {}, ss.str());
        string s_regAct = regAct.Serialize();
        send(ServerFD, s_regAct.c_str(), s_regAct.size(), 0);

        char buffer[1024];
        ssize_t bytesReceived = recv(ServerFD, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived < 0)
            return {};

        buffer[bytesReceived] = '\0';
        string s_resp(buffer);

        auto response = ClientAction::Deserialize(s_resp);
        ss = stringstream(response.Data);
        Account res={};
        ss >> res.ID;
        res.ConnectionKey = key;
        return res;
    }

    ClientAction ServerConnection::Request(ServerAction action, ExpectStatus expect) {
        auto snd = action.Serialize();
        Expecting.store(expect);
        send(ServerFD, snd.c_str(), snd.size(), 0);
        while (IngoingResponses.empty());
        auto response = PopResp();
        return response;
    }

    void ServerConnection::PushReq(const ServerAction& req) {
        {
            lock_guard<mutex> guard(m_OutgoingRequests);
            OutgoingRequests.push(std::move(const_cast<ServerAction&>(req)));
        }
    }

    ServerAction ServerConnection::PopReq() {
        {
            lock_guard<mutex> guard(m_OutgoingRequests);
            PoppedEmpty->store(true);
            if (OutgoingRequests.empty())
                return {};
            PoppedEmpty->store(false);
            auto tmp = std::move(OutgoingRequests.front());
            OutgoingRequests.pop();
            return tmp;
        }
    }

    void ServerConnection::PushResp(ClientAction resp) {
        {
            lock_guard<mutex> guard(m_IngoingResponses);
            IngoingResponses.push(std::move(resp));
        }
    }

    ClientAction ServerConnection::PopResp() {
        {
            lock_guard<mutex> guard(m_IngoingResponses);
            PoppedEmpty->store(true);
            if (IngoingResponses.empty())
                return {};
            PoppedEmpty->store(false);
            auto tmp = std::move(IngoingResponses.front());
            IngoingResponses.pop();
            return tmp;
        }
    }

    void ServerConnection::PushMess(ClientAction act) {
        {
            lock_guard<mutex> guard(m_IngoingMessages);
            IngoingMessages.emplace(std::move(act));
        }
    }

    ClientAction ServerConnection::PopMess() {
        {
            lock_guard<mutex> guard(m_IngoingMessages);
            PoppedEmpty->store(true);
            if (IngoingMessages.empty())
                return {};
            PoppedEmpty->store(false);
            auto tmp = std::move(IngoingMessages.front());
            IngoingMessages.pop();
            return tmp;
        }
    }

} // namespace classes::client_side
