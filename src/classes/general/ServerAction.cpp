//
// Created by ariel on 7/17/2024.
//

#include "ServerAction.h"
#include "StreamableString.h"

#include <string>

using namespace std;

namespace classes::general {

    ServerAction::ServerAction() = default;

    ServerAction::ServerAction(ServerActionType actType, AddressInfo addr, string data) :
    ActionType(actType), Address(addr),Data(move(data)){}

    string ServerAction::Serialize() {
        auto ss = StreamableString{};
        ss << static_cast<int>(this->ActionType) << " "
        << Address.ai_socktype << " "
        << Address.ai_family << " "
        << Address.ai_addrlen << " "
        << Address.ai_canonname << " "
        << Address.ai_flags << " "
        << Address.ai_protocol << " "
        << Data;
        return (string)ss;
    }

    ServerAction ServerAction::Deserialize(string& serializedStr) {
        stringstream ss(serializedStr);
        ServerAction action;
        int actionType;
        ss >> actionType;
        action.ActionType = static_cast<ServerActionType>(actionType);
        ss >> action.Address.ai_socktype;
        ss >> action.Address.ai_family;
        action.Address.ai_addr = new sockaddr;
        ss >> action.Address.ai_addrlen;
        ss >> action.Address.ai_canonname;
        ss >> action.Address.ai_flags;
        ss >> action.Address.ai_protocol;
        ss >> ws;
        getline(ss, action.Data);
        return action;
    }
} // general