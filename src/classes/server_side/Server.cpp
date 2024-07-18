#include "Server.h"

#include <utility>
#include <arpa/inet.h>
#include <sstream>
#include "../general/StreamableString.h"

typedef sockaddr_storage SocketAddressStorage;
typedef sockaddr SocketAddress;

using namespace classes::general;

namespace classes::server_side {
    const char *ServerPort = "3490";

    void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET)
            return &(((struct sockaddr_in *) sa)->sin_addr);
        return &(((struct sockaddr_in6 *) sa)->sin6_addr);
    }

    Server::Server() {
        Setup();
    }

    Server::~Server() {
        if (ServerFD > 0)
            shutdown(ServerFD, SHUT_RDWR);
        Running.store(false);
        if (ListenerThread)
            ListenerThread->join();
        delete ListenerThread;
        if (EnactRespondThread)
            EnactRespondThread->join();
        delete EnactRespondThread;
    }

    Server::Server(string name) :
            ServerName(move(name)) {
        Setup();
    }

    void Server::Start() {
        Running.store(true);
        ListenerThread = new thread([this]() {
            string err;
            if (listen(ServerFD, 10) == -1) {
                return;
            }

            while (Running.load()) {
                SocketAddressStorage addr{};
                socklen_t sinSize = sizeof addr;
                int newFD = accept(ServerFD, (SocketAddress *) &addr, &sinSize);
                if (newFD == -1) {
                    if (!Running.load()) break;
                    continue;
                }

                char s[INET6_ADDRSTRLEN];
                inet_ntop(addr.ss_family, get_in_addr((SocketAddress *) &addr), s, sizeof s);

                // Setup connection to client:
                ClientConnection conn{};
                conn.FileDescriptor = newFD;
                memcpy(&conn.Address, &addr, sizeof(conn.Address));
                auto tmpClient = RegisteredClient("Guest");
                tmpClient.Connection = conn;

                tmpClient.Connection.Start([&tmpClient, this](int fd) {
                    char buffer[1024] = {0};
                    ssize_t valread = recv(tmpClient.Connection.FileDescriptor, &buffer, sizeof buffer, 0);
                    if (valread < 0) {
                        close(tmpClient.Connection.FileDescriptor);
                        return;
                    }
                    string data(buffer, valread);
                    auto action = ServerAction::Deserialize(data);
                    PushAction(&tmpClient, &action);

                    auto resp = tmpClient.GetResponse();
                    if (!tmpClient.PoppedEmptyFlag) {
                        string sendData = resp.Serialize();
                        send(tmpClient.Connection.FileDescriptor, sendData.c_str(), sendData.size(), 0);
                    }
                    if (resp.IsLast) {
                        tmpClient.Connection.Stop();
                        tmpClient.Connection = {};
                    }
                });
                {
                    lock_guard<mutex> guard(m_Clients);
                    Clients.push_back(tmpClient);
                }
            }
            close(ServerFD); // Close the server file descriptor on exit
        });
        ListenerThread->detach();
        EnactRespondThread = new thread([this] { EnactRespond(); });
        EnactRespondThread->detach();
    }

    void Server::Stop() {
        Running.store(false);
    }

    void Server::Setup() {
        ListenerThread = nullptr;
        Running.store(false);
        ServerFD = -1;
        AddressInfo hints{}, *servInf;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        int result = getaddrinfo(nullptr, ServerPort, &hints, &servInf);
        if (result)
            throw runtime_error("getaddrinfo() failed!");
        //region SocketCreate
        AddressInfo *p;
        string err;
        for (p = ServerSocket.get(); p != nullptr; p = p->ai_next) {
            ServerFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (!(ServerFD + 1)) {
                err = "socket() failed!";
                continue;
            }
            int yes = 1;
            if (setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                close(ServerFD);
                err = "setsockopt() failed!";
                break;
            }
            break;
        }
        if (p == nullptr) {
            throw runtime_error(err);
            exit(EXIT_FAILURE);
        }
        //endregion

        //region SocketBind
        for (p = ServerSocket.get(); p != nullptr; p = p->ai_next) {
            if (bind(ServerFD, p->ai_addr, p->ai_addrlen) == -1) {
                close(ServerFD);
                err = "bind() failed!";
                continue;
            }
            break;
        }
        if (p == nullptr) {
            throw runtime_error(err);
            exit(EXIT_FAILURE);
        }
        //endregion
    }

    void Server::PushAction(RegisteredClient *client, ServerAction *act) {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            EnqueuedActions.push(tuple(client, act));
        }
    }

    tuple<RegisteredClient *, ServerAction *> Server::NextAction() {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            auto tmp = EnqueuedActions.front();
            EnqueuedActions.pop();
            return tmp;
        }
    }

    void Server::EnactRespond() {
        while (Running) {
            ServerAction *currentAct = nullptr;
            RegisteredClient *currentRequester = nullptr;

            {
                lock_guard<mutex> guard(m_EnqueuedActions);
                if (EnqueuedActions.empty()) {
                    continue; // Skips to the next iteration of the while loop if the queue is empty
                } else {
                    auto &front = EnqueuedActions.front();
                    currentRequester = get<0>(front);
                    currentAct = get<1>(front);
                    EnqueuedActions.pop(); // Remove the processed action from the queue
                }
            }

            if (currentAct == nullptr || currentRequester == nullptr) {
                continue; // Safeguard against null actions or requesters
            }

            StreamableString logSS;
            stringstream ss(currentAct->Data);
            unsigned long long id;
            string key;

            switch (currentAct->ActionType) {
                case ServerActionType::SendMessage: {
                    unsigned long long rID;
                    string msg;
                    ss >> id >> key >> rID >> msg;
                    if (VerifyIdentity(id, key)) {
                        bool found = false;
                        ChatroomHost *room;
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for (auto &curR: Rooms) {
                                if (curR.RoomID == rID) {
                                    room = &curR;
                                    for (auto &curMem: curR.Members) {
                                        if (curMem.ClientID == id) {
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        if (!room) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "msg'Cannot find requested room.'"));
                            continue;
                        }
                        if (!found) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "msg'You can't send a message to a"
                                                                        "chat room you are not a member of.'"));
                            continue;
                        }
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for (auto &curMem: room->Members) {
                                StreamableString msgSS;
                                msgSS << id << " " << msg;
                                curMem.PushResponse(ClientAction(ClientActionType::MessageReceived,
                                                                 curMem.Connection.Address,
                                                                 msgSS));
                            }
                        }
                        logSS << "Message sent in room: '"
                              << room->DisplayName
                              << "#"
                              << room->RoomID
                              << "'. Message content:'"
                              << msg
                              << "' Message sender ID: '"
                              << id
                              << "'";
                        ServerLog.emplace_back(logSS);
                        room->PushMessage(id,msg);
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "msg'Invalid credentials, failed to"
                                                                    "send message.'"));
                    }
                    break;
                }
                case ServerActionType::RegisterClient: {
                    string dispName;
                    ss >> dispName >> key;
                    auto newCl = RegisteredClient(dispName);
                    newCl.LoginKey = key;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        Clients.push_back(newCl);

                        logSS << "Created client: '" << newCl.DisplayName << "#" << newCl.ClientID << "'";
                        ServerLog.emplace_back(logSS);
                    }
                    break;
                }
                case ServerActionType::LoginClient: {
                    ss >> id >> key;
                    RegisteredClient *client = nullptr;
                    {
                        lock_guard<mutex> guard(m_Clients);


                        for (auto &c: Clients) {
                            if (c.ClientID == id) {
                                if (c.LoginKey == key) {
                                    client = &c;
                                    break;
                                } else {
                                    currentRequester->PushResponse(ClientAction(
                                            ClientActionType::InformActionFailure,
                                            currentRequester->Connection.Address,
                                            "msg'Invalid credentials, Login failed'"));
                                    continue;
                                }
                            }
                        }

                        if (!client) {
                            currentRequester->PushResponse(ClientAction(
                                    ClientActionType::InformActionFailure,
                                    currentRequester->Connection.Address,
                                    "msg'Invalid credentials, Login failed'"));
                            continue;
                        }

                        if (currentRequester->IsConnected) {
                            currentRequester->PushResponse(ClientAction(
                                    ClientActionType::InformActionFailure,
                                    currentRequester->Connection.Address,
                                    "msg'Nothing to do, you are already logged in'"));
                            continue;
                        }

                        client->Connection = currentRequester->Connection;

                        // Remove Guest Client
                        auto it = find_if(Clients.begin(), Clients.end(),
                                          [currentRequester](const RegisteredClient &c) {
                                              return &c == currentRequester;
                                          });

                        if (it != Clients.end()) {
                            Clients.erase(it);
                        }
                        client->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                          client->Connection.Address,
                                                          ServerName + " msg'You were logged in successfully'"));
                        logSS << "Client: '" << client->DisplayName << "#" << client->ClientID << "' has logged in";
                        ServerLog.emplace_back(logSS);
                    }

                    break;
                }
                case ServerActionType::LogoutClient: {
                    ss >> id >> key;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        RegisteredClient *client = nullptr;

                        for (auto &c: Clients) {
                            if (c.ClientID == id) {
                                if (c.LoginKey == key) {
                                    client = &c;
                                    break;
                                } else {
                                    currentRequester->PushResponse(ClientAction(
                                            ClientActionType::InformActionFailure,
                                            currentRequester->Connection.Address,
                                            "msg'Invalid credentials, Logout failed'"));
                                    continue;
                                }
                            }
                        }
                        client->IsConnected = false;
                        client->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                          client->Connection.Address,
                                                          "msg'You were successfully logged out'",
                                                          true));
                        logSS << "Client: '" << client->DisplayName << "#" << client->ClientID << "' has logged out";
                        ServerLog.emplace_back(logSS);
                    }
                    break;
                }
                case ServerActionType::CreateChatroom: {
                    string roomName;
                    ss >> id >> key >> roomName;
                    if(VerifyIdentity(id,key)){
                        RegisteredClient *admin;
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for(auto &curCl: Clients)
                                if(curCl.ClientID==id)
                                    admin=&curCl;
                        }
                        ChatroomHost newCR= ChatroomHost(roomName,admin);
                        Rooms.push_back(newCR);
                        {
                            lock_guard<mutex> guard(m_Clients);
                            logSS
                            << "Client: '"
                            << admin->DisplayName
                            << "#"
                            << admin->ClientID
                            << "' Created the new chatroom: '"
                            << newCR.DisplayName
                            << "#"
                            << newCR.RoomID
                            << "'";
                            ServerLog.emplace_back(logSS);
                        }
                    }
                    break;
                }
                case ServerActionType::RemoveChatroom: {
                    // Handle RemoveChatroom
                    break;
                }
                case ServerActionType::AddChatRoomMember: {
                    // Handle AddChatRoomMember
                    break;
                }
                case ServerActionType::RemoveChatroomMember: {
                    // Handle RemoveChatroomMember
                    break;
                }
            }
        }
    }

    bool Server::VerifyIdentity(unsigned long long int id, const string &key) {
        {
            lock_guard<mutex> guard(m_Clients);
            for (auto &c: Clients) {
                if (c.ClientID == id) {
                    if (c.LoginKey == key) {
                        return true;
                    } else {
                        return false;
                    }
                }
            }
            return false;
        }
    }
}