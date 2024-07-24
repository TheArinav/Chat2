#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sstream>

#include "ServerConnection.h"

namespace classes::client_side {
    const char *port = "3490";

    ServerConnection::ServerConnection(string &&Address) {
        ServerFD = -1;
        Initilized = true;
        SenderRunning = make_unique<atomic<bool>>();
        SenderRunning->store(false);
        SenderRunning->store(true);
        PoppedEmpty = make_unique<atomic<bool>>();  // Initialize PoppedEmpty
        PoppedEmpty->store(false);  // Set an initial value
        if (!Setup(Address)) {
            std::cerr << "Failed to establish connection to the server!\n";
            Initilized = false;
        }
    }

    ServerConnection::~ServerConnection() {
        SenderRunning->store(false);
        if (SenderThread->joinable())
            SenderThread->join();
        delete SenderThread;
        if (ServerFD != -1)
            close(ServerFD);
        freeaddrinfo(&ServerSocket);
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
            auto regAct = ServerAction(ServerActionType::RegisterClient, ServerSocket, ss.str());
            string s_regAct = regAct.Serialize();
            send(ServerFD, s_regAct.c_str(), s_regAct.size(), 0);

            sleep(5);

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
                                      ServerSocket,
                                      ss.str());
        string s_logginAct = logginAct.Serialize();
        send(ServerFD, s_logginAct.c_str(), s_logginAct.size(), 0);

        sleep(5);

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
        getline(ss,RespMsg);
        cout << RespMsg << "\n\tServer Name= '" << ServName << "'\n";

        SenderThread = new thread([this]() -> void {
            while (SenderRunning->load()) {
                auto curReq = PopReq();
                if (!PoppedEmpty->load()) {
                    string s_req = curReq.Serialize();
                    send(ServerFD, s_req.c_str(), s_req.size(), 0);
                    sleep(5);
                }
                char buffer[1024];
                ssize_t bytesReceived = recv(ServerFD, &buffer, sizeof buffer, 0);
                if (bytesReceived < 0) {
                    cerr << "Received response was nullptr\n";
                    if (SenderRunning->load())
                        continue;
                    break;
                }

                buffer[bytesReceived] = '\0';
                auto tmp = string(buffer);
                auto response = ClientAction::Deserialize(tmp);

                // Temporary:
                cout << string(buffer) << "\n";
            }
        });

        sleep(1);
        SenderThread->detach();
        // WIP
        return true;
    }

    void ServerConnection::PushReq(const ServerAction &req) {
        {
            lock_guard<mutex> guard(m_OutgoingRequests);
            OutgoingRequests.push((ServerAction&&)req);
        }
    }

    ServerAction ServerConnection::PopReq() {
        {
            lock_guard<mutex> guard(m_OutgoingRequests);
            PoppedEmpty->store(false);
            if (OutgoingRequests.empty())
                return {};
            PoppedEmpty->store(true);
            auto tmp = move(OutgoingRequests.front());
            OutgoingRequests.pop();
            return tmp;
        }
    }

    void ServerConnection::PushResp(ClientAction resp) {
        {
            lock_guard<mutex> guard(m_IngoingResponses);
            IngoingResponses.push(move(resp));
        }
    }

    ClientAction ServerConnection::PopResp() {
        {
            lock_guard<mutex> guard(m_IngoingResponses);
            PoppedEmpty->store(false);
            if (IngoingResponses.empty())
                return {};
            PoppedEmpty->store(true);
            auto tmp = move(IngoingResponses.front());
            IngoingResponses.pop();
            return tmp;
        }
    }

} // namespace classes::client_side
