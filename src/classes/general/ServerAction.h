#ifndef SERVERACTION_H
#define SERVERACTION_H

#include <string>
#include <memory>
#include <sys/socket.h>
#include <netdb.h>
#include "Enums.h"

namespace classes::general {
    class ServerAction {
    public:
        ServerAction();
        ServerAction(ServerActionType actType, addrinfo addr, std::string data);
        ~ServerAction();
        ServerAction(const ServerAction&) = delete;
        ServerAction& operator=(const ServerAction&) = delete;
        ServerAction(ServerAction&&) noexcept;
        ServerAction& operator=(ServerAction&&) noexcept;

        std::string Serialize();
        static ServerAction Deserialize(std::string& serializedStr);

        ServerActionType ActionType;
        addrinfo Address;
        std::string Data;
    private:
        std::unique_ptr<sockaddr> ai_addr_ptr;
        std::string ai_canonname_str;
    };
}

#endif // SERVERACTION_H
