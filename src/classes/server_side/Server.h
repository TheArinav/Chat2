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

#include "../general/ServerAction.h"
#include "../general/ClientAction.h"
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
        vector<shared_ptr<RegisteredClient>> Clients;
        vector<ChatroomHost> Rooms;
        thread *ListenerThread;
        thread *EnactRespondThread;
        unique_ptr<AddressInfo> ServerSocket;
        sockaddr_storage AddrStore;
        string IPSTR;
        int ServerFD;

        ~Server();
        explicit Server(string&& name);

        void Start();
        void Stop();

        void PushAction(shared_ptr<RegisteredClient> client,shared_ptr<ServerAction> act);
    private:
        shared_ptr<atomic<bool>> Running;
        mutex m_EnqueuedActions;
        queue<tuple<shared_ptr<RegisteredClient>,shared_ptr<ServerAction>>> EnqueuedActions;
        void Setup();
        tuple<shared_ptr<RegisteredClient>,shared_ptr<ServerAction>> NextAction();
        void EnactRespond();
        bool VerifyIdentity(unsigned long long id, const string& key);
    };
} // namespace classes::server_side

#endif //CHAT2_SERVER_H
