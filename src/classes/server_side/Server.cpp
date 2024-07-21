#include "Server.h"

#include <utility>
#include <iostream>
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
        if (ListenerThread->joinable())
            ListenerThread->join();
        delete ListenerThread;
        if (EnactRespondThread->joinable())
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
                    PushAction(&tmpClient, action);

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
        if (result != 0) {
            std::cerr << "getaddrinfo: " << gai_strerror(result) << std::endl;
            throw std::runtime_error("getaddrinfo() failed!");
        }

        ServerSocket = std::make_unique<AddressInfo>(*servInf);

        //region SocketCreate
        AddressInfo *p;
        std::string err;
        for (p = ServerSocket.get(); p != nullptr; p = p->ai_next) {
            ServerFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (ServerFD == -1) {
                perror("socket");
                continue;
            }

            int yes = 1;
            if (setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                perror("setsockopt");
                close(ServerFD);
                err = "setsockopt() failed!";
                continue;
            }

            // Try to bind the socket
            if (bind(ServerFD, p->ai_addr, p->ai_addrlen) == -1) {
                perror("bind");
                close(ServerFD);
                ServerFD = -1; // Reset ServerFD for next attempt
                continue;
            }

            break; // Successfully bound the socket
        }

        if (p == nullptr) {
            freeaddrinfo(servInf);
            throw std::runtime_error("Failed to bind any address!");
        }

        freeaddrinfo(servInf);
        //endregion
    }


    void Server::PushAction(RegisteredClient *client, ServerAction act) {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            EnqueuedActions.push(tuple(client, act));
        }
    }

    tuple<RegisteredClient *, ServerAction > Server::NextAction() {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            auto tmp = EnqueuedActions.front();
            EnqueuedActions.pop();
            return tmp;
        }
    }

    void Server::EnactRespond() {
        while (Running.load()) {
            ServerAction currentAct = {};
            RegisteredClient *currentRequester = nullptr;

            {
                lock_guard<mutex> guard(m_EnqueuedActions);
                if (EnqueuedActions.empty()) {
                    continue; // Skips to the next iteration of the while loop if the queue is empty
                } else {
                    auto [r,a] = NextAction();
                    currentRequester = r;
                    currentAct = a;
                    EnqueuedActions.pop(); // Remove the processed action from the queue
                }
            }

            if (currentRequester == nullptr) {
                continue;
            }

            StreamableString logSS;
            stringstream ss(currentAct.Data);
            unsigned long long id;
            string key;

            switch (currentAct.ActionType) {
                case ServerActionType::SendMessage: {
                    unsigned long long rID;
                    string msg;
                    ss >> id >> key >> rID >> msg;
                    if (VerifyIdentity(id, key)) {
                        bool found = false;
                        ChatroomHost *room = nullptr;
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
                                                                        "Cannot find requested room."));
                            continue;
                        }
                        if (!found) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "You can't send a message to a chat room you are not a member of."));
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
                        room->PushMessage(id, msg);
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Invalid credentials, failed to send message."));
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
                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                                currentRequester->Connection.Address,
                                                                to_string(newCl.ClientID)));
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
                                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                                currentRequester->Connection.Address,
                                                                                "Invalid credentials, Login failed"));
                                    continue;
                                }
                            }
                        }

                        if (!client) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "Invalid credentials, Login failed"));
                            continue;
                        }

                        if (currentRequester->IsConnected) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "Nothing to do, you are already logged in"));
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
                                                          ServerName + " You were logged in successfully"));
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
                                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                                currentRequester->Connection.Address,
                                                                                "Invalid credentials, Logout failed"));
                                    continue;
                                }
                            }
                        }
                        client->IsConnected = false;
                        client->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                          client->Connection.Address,
                                                          "You were successfully logged out",
                                                          true));
                        logSS << "Client: '" << client->DisplayName << "#" << client->ClientID << "' has logged out";
                        ServerLog.emplace_back(logSS);
                    }
                    break;
                }
                case ServerActionType::CreateChatroom: {
                    string roomName;
                    ss >> id >> key >> roomName;
                    if (VerifyIdentity(id, key)) {
                        RegisteredClient *admin = nullptr;
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for (auto &curCl: Clients)
                                if (curCl.ClientID == id)
                                    admin = &curCl;
                        }
                        ChatroomHost newCR = ChatroomHost(roomName, admin);
                        Rooms.push_back(newCR);
                        {
                            lock_guard<mutex> guard(m_Clients);
                            logSS << "Client: '"
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

                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                                    currentRequester->Connection.Address,
                                                                    to_string(newCR.RoomID) + " Chat room was created"));
                        currentRequester->PushResponse(ClientAction(ClientActionType::JoinedChatroom,
                                                                    currentRequester->Connection.Address,
                                                                    to_string(newCR.RoomID) + " " + newCR.DisplayName));
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Invalid credentials"));
                    }
                    break;
                }
                case ServerActionType::RemoveChatroom: {
                    unsigned long long rID;
                    ChatroomHost *room = nullptr;
                    ss >> id >> key >> rID;
                    if (VerifyIdentity(id, key)) {
                        for (auto &curR: Rooms) {
                            if (curR.RoomID == rID) {
                                room = &curR;
                                break;
                            }
                        }
                        if (!room) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "Failed to find requested room"));
                            continue;
                        }
                        if (room->Admin->ClientID != id) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection.Address,
                                                                        "You must be the room's admin in order to delete it"));
                            continue;
                        }
                        auto it = find_if(Rooms.begin(), Rooms.end(),
                                          [rID](const ChatroomHost &cr) -> bool {
                                              return cr.RoomID == rID;
                                          });
                        if (it != Rooms.end()) {
                            for (auto &curMem: room->Members)
                                curMem.PushResponse(ClientAction(ClientActionType::LeftChatroom,
                                                                 curMem.Connection.Address,
                                                                 to_string(room->RoomID) + " This room was deleted by the admin."));
                            Rooms.erase(it);
                        }
                    }
                    break;
                }
                case ServerActionType::AddChatRoomMember: {
                    unsigned long long rID, newMemberID;
                    ss >> id >> key >> rID >> newMemberID;
                    if (!VerifyIdentity(id, key)) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Invalid credentials"));
                        continue;
                    }

                    ChatroomHost *room = nullptr;
                    RegisteredClient *newMember = nullptr;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        for (auto &curR: Rooms) {
                            if (curR.RoomID == rID) {
                                room = &curR;
                                break;
                            }
                        }
                        for (auto &curCl: Clients) {
                            if (curCl.ClientID == newMemberID) {
                                newMember = &curCl;
                                break;
                            }
                        }
                    }

                    if (!room) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Chatroom not found"));
                        continue;
                    }

                    if (room->Admin->ClientID != id) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Only the admin can add members"));
                        continue;
                    }

                    if (!newMember) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "New member not found"));
                        continue;
                    }

                    room->Members.push_back(*newMember);
                    newMember->PushResponse(ClientAction(ClientActionType::JoinedChatroom,
                                                         newMember->Connection.Address,
                                                         to_string(room->RoomID) + " " + room->DisplayName));
                    StreamableString joinMSG;
                    joinMSG << id << key << rID << "Admin added a member to this room: '"
                    << newMember->DisplayName << "#" << newMember->ClientID << "'";
                    PushAction(currentRequester,ServerAction(general::ServerActionType::SendMessage,
                                            currentRequester->Connection.Address,
                                                             (string)joinMSG));

                    logSS << "Client: '"
                          << newMember->DisplayName
                          << "#"
                          << newMember->ClientID
                          << "' was added to chatroom: '"
                          << room->DisplayName
                          << "#"
                          << room->RoomID
                          << "'";
                    ServerLog.emplace_back(logSS);
                    break;
                }
                case ServerActionType::RemoveChatroomMember: {
                    unsigned long long rID, memberID;
                    ss >> id >> key >> rID >> memberID;
                    if (!VerifyIdentity(id, key)) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Invalid credentials"));
                        continue;
                    }

                    ChatroomHost *room = nullptr;
                    RegisteredClient *member = nullptr;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        for (auto &curR: Rooms) {
                            if (curR.RoomID == rID) {
                                room = &curR;
                                break;
                            }
                        }
                        for (auto &curCl: Clients) {
                            if (curCl.ClientID == memberID) {
                                member = &curCl;
                                break;
                            }
                        }
                    }

                    if (!room) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Chatroom not found"));
                        continue;
                    }

                    if (room->Admin->ClientID != id && memberID != id) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Only the admin or the member themselves can remove members"));
                        continue;
                    }

                    if (!member) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Member not found"));
                        continue;
                    }

                    auto it = find_if(room->Members.begin(), room->Members.end(),
                                      [memberID](const RegisteredClient &m) {
                                          return m.ClientID == memberID;
                                      });

                    if (it != room->Members.end()) {
                        room->Members.erase(it);
                        member->PushResponse(ClientAction(ClientActionType::LeftChatroom,
                                                          member->Connection.Address,
                                                          to_string(room->RoomID) + " " + room->DisplayName));
                        logSS << "Client: '"
                              << member->DisplayName
                              << "#"
                              << member->ClientID
                              << "' was removed from chatroom: '"
                              << room->DisplayName
                              << "#"
                              << room->RoomID
                              << "'";
                        ServerLog.emplace_back(logSS);
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection.Address,
                                                                    "Member not found in the chatroom"));
                    }
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
                        return c.IsConnected;
                    } else {
                        return false;
                    }
                }
            }
            return false;
        }
    }
}