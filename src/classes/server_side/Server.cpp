#include "Server.h"

#include <utility>
#include <iostream>
#include <arpa/inet.h>
#include <sstream>
#include <fcntl.h>
#include <sys/select.h>

#include "ClientConnection.h"
#include "RegisteredClient.h"

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

    Server::~Server() {
        if (ServerFD > 0) {
            shutdown(ServerFD, SHUT_RDWR);
        }
        Running->store(false);
        if (ListenerThread && ListenerThread->joinable()) {
            ListenerThread->join();
            delete ListenerThread;
        }
        if (EnactRespondThread && EnactRespondThread->joinable()) {
            EnactRespondThread->join();
            delete EnactRespondThread;
        }
        std::cout << "\n\nExecution ended. Printing server log:" << "\n";
        int i = 1;
        while (!ServerLog.empty()) {
            std::cout << "\tServer Log[" << i++ << "]= " << ServerLog[0] << "\n";
            ServerLog.erase(ServerLog.begin());
        }
    }


    Server::Server(string &&name) :
            ServerName(move(name)) {
        Setup();
    }

    int make_socket_non_blocking(int sfd) {
        int flags = fcntl(sfd, F_GETFL, 0);
        if (flags == -1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
            return -1;
        }

        flags |= O_NONBLOCK;
        if (fcntl(sfd, F_SETFL, flags) == -1) {
            perror("fcntl");
            exit(EXIT_FAILURE);
            return -1;
        }

        return 0;
    }

    void Server::Start() {
        Running->store(true);
        ListenerThread = new thread([this]() {
            std::string err = "";
            if (listen(ServerFD, 10) == -1) {
                cerr << "Listen() failure, cause:\n\t" << strerror(errno) << "\n";
                return;
            }

            while (Running->load()) {
                SocketAddressStorage addr{};
                socklen_t sinSize = sizeof addr;
                int newFD = accept(ServerFD, (SocketAddress *) &addr, &sinSize);
                if (newFD == -1) {
                    if (Running == nullptr || !Running->load())
                        break;
                    continue;
                }

                char s[INET6_ADDRSTRLEN];
                inet_ntop(addr.ss_family, get_in_addr((SocketAddress *) &addr), s, sizeof s);

                // Setup connection to client:
                unique_ptr<ClientConnection> conn = make_unique<ClientConnection>();
                conn->FileDescriptor = newFD;
                memcpy(&conn->Address, &addr, sizeof(conn->Address));
                auto tmpClient = std::make_shared<RegisteredClient>((string) "Guest");
                tmpClient->LinkClientConnection(move(conn));

                auto f = [this](const shared_ptr<RegisteredClient> &client, int fd, shared_ptr<atomic<bool>> stop) -> void {
                    if (make_socket_non_blocking(fd) == -1) {
                        close(fd);
                        return;
                    }

                    char buffer[1024] = {0};


                    fd_set read_fds;
                    FD_ZERO(&read_fds);
                    FD_SET(fd, &read_fds);

                    struct timeval timeout{};
                    timeout.tv_sec = 5;  // Set timeout to 5 seconds
                    timeout.tv_usec = 0;

                    auto resp = client->GetResponse();
                    {
                        lock_guard<mutex> guard(*client->m_AwaitingResponses);
                        if (!client->PoppedEmptyFlag) {
                            string sendData = resp.Serialize();
                            send(fd, sendData.c_str(), sendData.size(), 0);
                            return;
                        }
                        if (resp.IsLast) {
                            client->Connection->Stop();
                            client->Connection = nullptr;
                        }
                    }

                    int activity = select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
                    if (activity < 0) {
                        perror("select");
                        close(fd);
                        return;
                    } else if (activity == 0) {
                        // Timeout: No data to read, continue to the next iteration
                        return;
                    }

                    if (FD_ISSET(fd, &read_fds)) {
                        ssize_t valread = recv(fd, (char *) &buffer, sizeof(buffer), 0);
                        if (valread < 0) {
                            perror("recv");
                            close(fd);

                            stop->store(true);

                            return;
                        } else if (valread == 0) {
                            // Connection closed
                            close(fd);

                            stop->store(true);

                            return;
                        }

                        string data(buffer, valread);
                        auto action = make_shared<ServerAction>(ServerAction::Deserialize(data));
                        {
                            lock_guard<mutex> guard(*client->m_AwaitingResponses);
                            PushAction(client, action);
                        }

                    }
                };
                tmpClient->Connection->Start(f);
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
        Running->store(false);
    }

    void Server::Setup() {
        Running = make_shared<atomic<bool>>();
        EnqueuedActions = {};
        ListenerThread = nullptr;
        Running->store(false);
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

            // Successfully bound the socket
            memcpy(&AddrStore, p->ai_addr, p->ai_addrlen);
            break;
        }

        if (p == nullptr) {
            freeaddrinfo(servInf);
            throw std::runtime_error("Failed to bind any address!");
        }

        freeaddrinfo(servInf);
        //endregion

        SocketAddressStorage boundAddr{};
        socklen_t addrLen = sizeof(boundAddr);
        if (getsockname(ServerFD, (struct sockaddr*)&boundAddr, &addrLen) == -1) {
            perror("getsockname");
            close(ServerFD);
            throw std::runtime_error("getsockname() failed!");
        }

        char ipstr[INET6_ADDRSTRLEN];
        void *addr;
        if (boundAddr.ss_family == AF_INET) { // IPv4
            auto *ipv4 = (struct sockaddr_in *)&boundAddr;
            addr = &(ipv4->sin_addr);
        } else { // IPv6
            auto *ipv6 = (struct sockaddr_in6 *)&boundAddr;
            addr = &(ipv6->sin6_addr);
        }
        inet_ntop(boundAddr.ss_family, addr, ipstr, sizeof ipstr);
        IPSTR=ipstr;
    }

    void Server::PushAction(shared_ptr<RegisteredClient> client, shared_ptr<ServerAction> act) {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            EnqueuedActions.push(tuple<shared_ptr<RegisteredClient>, shared_ptr<ServerAction>>(client, act));
        }
    }

    tuple<shared_ptr<RegisteredClient>, shared_ptr<ServerAction>> Server::NextAction() {
        {
            //Critical Section
            lock_guard<mutex> guard(m_EnqueuedActions);
            auto tmp = EnqueuedActions.front();
            EnqueuedActions.pop();
            return tmp;
        }
    }

    void Server::EnactRespond() {
        while (Running->load()) {
            shared_ptr<ServerAction> currentAct;
            shared_ptr<RegisteredClient> currentRequester = nullptr;

            if (EnqueuedActions.empty()) {
                continue; // Skips to the next iteration of the while loop if the queue is empty
            } else {
                auto [r, a] = NextAction();
                currentRequester = r;
                currentAct = a;
            }

            if (currentRequester == nullptr) {
                continue;
            }

            stringstream logSS{};
            stringstream ss(currentAct->Data);
            unsigned long long id;
            string key;

            switch (currentAct->ActionType) {
                case ServerActionType::SendMessage: {
                    unsigned long long rID;
                    string msg;
                    ss >> id >> key >> rID;
                    getline(ss,msg);
                    if (VerifyIdentity(id, key)) {
                        bool found = false;
                        ChatroomHost *room = nullptr;
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for (auto &curR: Rooms) {
                                if (curR.RoomID == rID) {
                                    room = &curR;
                                    for (auto &curMem: curR.Members) {
                                        if (curMem->ClientID == id) {
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        if (!room) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection->Address,
                                                                        "Cannot find requested room."));
                            continue;
                        }
                        if (!found) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection->Address,
                                                                        "You can't send a message to a chat room you are not a member of."));
                            continue;
                        }
                        {
                            lock_guard<mutex> guard(m_Clients);
                            for (auto &curMem: room->Members) {
                                stringstream msgSS{};
                                msgSS << id << " " << msg;
                                curMem->PushResponse(ClientAction(ClientActionType::MessageReceived,
                                                                  {},
                                                                  msgSS.str()));
                            }
                        }
                        currentRequester->PushResponse(ClientAction(general::ClientActionType::InformActionSuccess,
                                                                    {},
                                                                    "Message sent"));
                        logSS << "Message sent in room: '"
                              << room->DisplayName
                              << "#"
                              << room->RoomID
                              << "'. Message content:'"
                              << msg
                              << "' Message sender ID: '"
                              << id
                              << "'";
                        ServerLog.emplace_back(logSS.str());
                        room->PushMessage(id, msg);
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Invalid credentials, failed to send message."));
                    }
                    break;
                }
                case ServerActionType::RegisterClient: {
                    string dispName;
                    ss >> dispName >> key;
                    auto newCl = make_shared<RegisteredClient>(dispName);
                    newCl->LoginKey = key;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        Clients.push_back(move(newCl));
                        newCl = Clients[Clients.size() - 1];

                        logSS << "Created client: '" << newCl->DisplayName << "#" << newCl->ClientID << "'";
                        ServerLog.emplace_back(logSS.str());
                    }
                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                                currentRequester->Connection->Address,
                                                                to_string(newCl->ClientID)));
                    break;
                }
                case ServerActionType::LoginClient: {
                    ss >> id >> key;
                    RegisteredClient *client = nullptr;
                    {
                        lock_guard<mutex> guard(m_Clients);

                        for (auto &c: Clients) {
                            if (c->ClientID == id) {
                                if (c->LoginKey == key) {
                                    client = c.get();
                                    break;
                                } else {
                                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                                currentRequester->Connection->Address,
                                                                                "Invalid credentials, Login failed"));
                                    continue;
                                }
                            }
                        }

                        if (!client) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection->Address,
                                                                        "Invalid credentials, Login failed"));
                            continue;
                        }

                        if (currentRequester->IsConnected) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection->Address,
                                                                        "Nothing to do, you are already logged in"));
                            continue;
                        }
                    }
                    client->LinkClientConnection(move(currentRequester->Connection));
                    client->IsConnected = true;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        // Remove Guest Client
                        auto it = find_if(Clients.begin(), Clients.end(),
                                          [currentRequester](shared_ptr<RegisteredClient> &c) {
                                              return c.get() == currentRequester.get();
                                          });

                        if (it != Clients.end()) {
                            Clients.erase(it);
                        }
                        client->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                          client->Connection->Address,
                                                          ServerName + " You were logged in successfully"));
                        logSS << "Client: '" << client->DisplayName << "#" << client->ClientID << "' has logged in";
                        ServerLog.emplace_back(logSS.str());
                    }

                    break;
                }
                case ServerActionType::LogoutClient: {
                    ss >> id >> key;
                    {
                        lock_guard<mutex> guard(m_Clients);
                        RegisteredClient *client = nullptr;

                        for (auto &c: Clients) {
                            if (c->ClientID == id) {
                                if (c->LoginKey == key) {
                                    client = c.get();
                                    break;
                                } else {
                                    currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                                currentRequester->Connection->Address,
                                                                                "Invalid credentials, Logout failed"));
                                    continue;
                                }
                            }
                        }
                        client->IsConnected = false;
                        client->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                          client->Connection->Address,
                                                          "You were successfully logged out",
                                                          true));
                        logSS << "Client: '" << client->DisplayName << "#" << client->ClientID << "' has logged out";
                        ServerLog.emplace_back(logSS.str());
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
                                if (curCl->ClientID == id)
                                    admin = curCl.get();
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
                            ServerLog.emplace_back(logSS.str());
                        }

                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionSuccess,
                                                                    currentRequester->Connection->Address,
                                                                    to_string(newCR.RoomID) +
                                                                    " Chat room was created$"));
                        currentRequester->PushResponse(ClientAction(ClientActionType::JoinedChatroom,
                                                                    currentRequester->Connection->Address,
                                                                    to_string(newCR.RoomID) + " " + newCR.DisplayName));
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
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
                                                                        currentRequester->Connection->Address,
                                                                        "Failed to find requested room"));
                            continue;
                        }
                        if (room->Admin->ClientID != id) {
                            currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                        currentRequester->Connection->Address,
                                                                        "You must be the room's admin in order to delete it"));
                            continue;
                        }
                        auto it = find_if(Rooms.begin(), Rooms.end(),
                                          [rID](const ChatroomHost &cr) -> bool {
                                              return cr.RoomID == rID;
                                          });
                        if (it != Rooms.end()) {
                            for (auto &curMem: room->Members)
                                curMem->PushResponse(ClientAction(ClientActionType::LeftChatroom,
                                                                  curMem->Connection->Address,
                                                                  to_string(room->RoomID) +
                                                                  " This room was deleted by the admin."));
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
                                                                    currentRequester->Connection->Address,
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
                            if (curCl->ClientID == newMemberID) {
                                newMember = curCl.get();
                                break;
                            }
                        }
                    }

                    if (!room) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Chatroom not found"));
                        continue;
                    }

                    if (room->Admin->ClientID != id) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Only the admin can add members"));
                        continue;
                    }

                    if (!newMember) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "New member not found"));
                        continue;
                    }

                    room->Members.push_back(newMember);
                    newMember->PushResponse(ClientAction(ClientActionType::JoinedChatroom,
                                                         {},
                                                         to_string(room->RoomID) + " " + room->DisplayName));
                    stringstream joinMSG;
                    joinMSG << id << key << rID << "Admin added a member to this room: '"
                            << newMember->DisplayName << "#" << newMember->ClientID << "'";
                    PushAction(currentRequester, make_shared<ServerAction>(general::ServerActionType::SendMessage,
                                                                           currentRequester->Connection->Address,
                                                                           joinMSG.str()));
                    currentRequester->PushResponse(ClientAction(general::ClientActionType::InformActionSuccess,
                                                                {},
                                                                ""));
                    logSS << "Client: '"
                          << newMember->DisplayName
                          << "#"
                          << newMember->ClientID
                          << "' was added to chatroom: '"
                          << room->DisplayName
                          << "#"
                          << room->RoomID
                          << "'";
                    ServerLog.emplace_back(logSS.str());
                    break;
                }
                case ServerActionType::RemoveChatroomMember: {
                    unsigned long long rID, memberID;
                    ss >> id >> key >> rID >> memberID;
                    if (!VerifyIdentity(id, key)) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
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
                            if (curCl->ClientID == memberID) {
                                member = curCl.get();
                                break;
                            }
                        }
                    }

                    if (!room) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Chatroom not found"));
                        continue;
                    }

                    if (room->Admin->ClientID != id && memberID != id) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Only the admin or the member themselves can remove members"));
                        continue;
                    }

                    if (!member) {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
                                                                    "Member not found"));
                        continue;
                    }

                    auto it = find_if(room->Members.begin(), room->Members.end(),
                                      [memberID](RegisteredClient *m) {
                                          return m->ClientID == memberID;
                                      });

                    if (it != room->Members.end()) {
                        room->Members.erase(it);
                        member->PushResponse(ClientAction(ClientActionType::LeftChatroom,
                                                          member->Connection->Address,
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
                        ServerLog.emplace_back(logSS.str());
                    } else {
                        currentRequester->PushResponse(ClientAction(ClientActionType::InformActionFailure,
                                                                    currentRequester->Connection->Address,
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
                if (c->ClientID == id) {
                    if (c->LoginKey == key) {
                        return c->IsConnected;
                    } else {
                        return false;
                    }
                }
            }
            return false;
        }
    }
}
