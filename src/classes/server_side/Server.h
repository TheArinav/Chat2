//
// Created by ariel on 7/17/2024.
//

#ifndef CHAT2_SERVER_H
#define CHAT2_SERVER_H

#include <sys/socket.h>
#include <netdb.h>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <map>
#include <tuple>

#include "RegisteredClient.h"
#include "ChatroomHost.h"

typedef addrinfo AddressInfo;

using namespace std;

namespace classes::server_side {
    class Server {
    public:
        string ServerName;
        mutex m_Clients;
        vector<string> ServerLog;
        vector<RegisteredClient> Clients;
        vector<ChatroomHost> Rooms;
        thread *ListenerThread;
        thread *EnactRespondThread;
        unique_ptr<AddressInfo> ServerSocket;
        int ServerFD;


        ~Server();
        explicit Server(string&& name);

        void Start();
        void Stop();

        void PushAction(RegisteredClient *client,ServerAction act);
    private:
        atomic<bool> Running;
        mutex m_EnqueuedActions;
        queue<tuple<RegisteredClient*,ServerAction>> EnqueuedActions;
        void Setup();
        tuple<RegisteredClient*,ServerAction> NextAction();
        void EnactRespond();
        bool VerifyIdentity(unsigned long long id, const string& key);
    };
} // server_side

#endif //CHAT2_SERVER_H
