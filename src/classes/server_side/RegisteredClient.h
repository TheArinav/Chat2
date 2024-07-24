#ifndef CHAT2_REGISTEREDCLIENT_H
#define CHAT2_REGISTEREDCLIENT_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>

#include "../general/ClientAction.h"
#include "../general/ServerAction.h"

using namespace std;
using namespace classes::general;

namespace classes::server_side {
    class ClientConnection;

    class RegisteredClient : public enable_shared_from_this<RegisteredClient> {
    public:
        unsigned long long ClientID;
        string DisplayName;
        string LoginKey;
        unique_ptr<ClientConnection> Connection;
        bool IsConnected;
        bool PoppedEmptyFlag;

        RegisteredClient();
        explicit RegisteredClient(string);
        RegisteredClient(RegisteredClient&);
        RegisteredClient(RegisteredClient&&) noexcept;
        RegisteredClient& operator=(RegisteredClient&&) noexcept;

        void PushResponse(ClientAction);
        void LinkClientConnection(unique_ptr<ClientConnection> conn);
        ClientAction GetResponse();

        shared_ptr<mutex> m_AwaitingResponses;
    private:
        void Setup();
        static unsigned long long count;
        vector<ClientAction> AwaitingResponses;
    };
}

#endif //CHAT2_REGISTEREDCLIENT_H
