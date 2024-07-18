#ifndef CHAT2_REGISTEREDCLIENT_H
#define CHAT2_REGISTEREDCLIENT_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>

#include "../general/ClientAction.h"
#include "../general/ServerAction.h"
#include "ClientConnection.h"

using namespace std;
using namespace classes::general;

namespace classes::server_side {
    class RegisteredClient {
    public:
        unsigned long long ClientID;
        string DisplayName;
        string LoginKey;
        ClientConnection Connection;
        bool IsConnected;
        bool PoppedEmptyFlag;

        RegisteredClient();
        explicit RegisteredClient(string);
        RegisteredClient(const RegisteredClient&);
        RegisteredClient &operator=(const RegisteredClient &other);
        RegisteredClient(RegisteredClient &&other) noexcept;
        RegisteredClient &operator=(RegisteredClient &&other) noexcept;

        void PushResponse(ClientAction);

        ClientAction GetResponse();


    private:
        void Setup();
        static unsigned long long count;
        unique_ptr<mutex> m_AwaitingResponses;
        vector<ClientAction> AwaitingResponses;

    };

}

#endif //CHAT2_REGISTEREDCLIENT_H
