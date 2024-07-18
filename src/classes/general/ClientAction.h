//
// Created by ariel on 7/17/2024.
//

#ifndef CHAT2_CLIENTACTION_H
#define CHAT2_CLIENTACTION_H

#include <sys/socket.h>
#include <netdb.h>
#include <string>

#include "Enums.h"

typedef addrinfo AddressInfo;

using std::string;

namespace classes::general {

    class ClientAction {
    public:
        ClientActionType ActionType;
        AddressInfo Address;
        string Data;
        bool IsLast;

        ClientAction();
        ClientAction(ClientActionType actType, AddressInfo addr, string data, bool=false);

        string Serialize();
        static ClientAction Deserialize(const string &);
    };

} // general

#endif //CHAT2_CLIENTACTION_H
