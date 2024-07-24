#ifndef CLIENTACTION_H
#define CLIENTACTION_H

#include <string>
#include <memory>
#include <sys/socket.h>
#include <netdb.h>
#include "Enums.h"

namespace classes::general {
    class ClientAction {
    public:
        ClientAction();
        ClientAction(ClientActionType actType, addrinfo addr, std::string data,bool IsLast=true);
        ~ClientAction();
        ClientAction(const ClientAction&) = delete;
        ClientAction& operator=(const ClientAction&) = delete;
        ClientAction(ClientAction&&) noexcept;
        ClientAction& operator=(ClientAction&&) noexcept;

        std::string Serialize();
        static ClientAction Deserialize(std::string& serializedStr);

        ClientActionType ActionType;
        addrinfo Address;
        std::string Data;
        bool IsLast;
    private:
        std::unique_ptr<sockaddr> ai_addr_ptr;
        std::string ai_canonname_str;
    };
}

#endif // CLIENTACTION_H
