#ifndef CHAT2_SERVERCONNECTION_H
#define CHAT2_SERVERCONNECTION_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <thread>
#include <queue>
#include <mutex>
#include <memory>
#include <atomic>

#include "Account.h"
#include "../general/ServerAction.h"
#include "../general/ClientAction.h"

typedef addrinfo AddressInfo;

using namespace std;
using namespace classes::general;

namespace classes::client_side {

    class ServerConnection {
    public:
        shared_ptr<AddressInfo> ServerSocket;
        int ServerFD;
        unique_ptr<atomic<bool>> SenderRunning;
        thread *SenderThread;
        queue<ServerAction> OutgoingRequests;
        queue<ClientAction> IngoingResponses;
        Account TargetClient;

        explicit ServerConnection(const string& Address);
        ~ServerConnection();

        Account Register(const string&, const string&);
        bool Connect(bool Register, unsigned long long int id=-1, const string& key="", const string &DisplayName="");

        ClientAction Request(ServerAction action) const;
    private:
        void PushReq(const ServerAction& req);
        ClientAction PopResp();
        mutex m_OutgoingRequests;
        mutex m_IngoingResponses;
        unique_ptr<atomic<bool>> PoppedEmpty;  // Initialize this properly

        bool Initilized;
        bool Setup(const string& Address);

        ServerAction PopReq();
        void PushResp(ClientAction resp);
    };

} // client_side

#endif //CHAT2_SERVERCONNECTION_H
