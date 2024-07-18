#ifndef CHAT2_SERVERACTION_H
#define CHAT2_SERVERACTION_H

#include <sys/socket.h>
#include <netdb.h>
#include <string>

#include "Enums.h"

typedef addrinfo AddressInfo;

using std::string;

namespace classes::general {

    class ServerAction {
    public:
        ServerActionType ActionType;
        AddressInfo Address;
        string Data;

        ServerAction();
        ServerAction(ServerActionType actType,AddressInfo addr, string data);

        string Serialize();
        static ServerAction Deserialize(string &);
    };

}

#endif //CHAT2_SERVERACTION_H
