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
        AddressInfo ServerSocket;
        int ServerFD;
        unique_ptr<atomic<bool>> SenderRunning;
        thread *SenderThread;
        queue<ServerAction> OutgoingRequests;
        queue<ClientAction> IngoingResponses;
        Account TargetClient;

        ServerConnection(string&& Address);
        ~ServerConnection();


        bool Connect(bool Register, unsigned long long int id=-1, const string& key="", const string &DisplayName="");
    private:
        mutex m_OutgoingRequests;
        mutex m_IngoingResponses;
        unique_ptr<atomic<bool>> PoppedEmpty;
        bool Initilized;
        bool Setup(const string& Address);

        void PushReq(const ServerAction& req);
        ServerAction PopReq();

        void PushResp(ClientAction resp);
        ClientAction PopResp();
    };

} // client_side

#endif //CHAT2_SERVERCONNECTION_H
