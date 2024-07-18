//
// Created by ariel on 7/17/2024.
//

#include "ClientAction.h"
#include "StreamableString.h"

namespace classes::general {
    ClientAction::ClientAction()=default;

    ClientAction::ClientAction(ClientActionType actType, AddressInfo addr, string data,bool isLast) :
    ActionType(actType), Address(addr),Data(move(data)),IsLast(isLast){}

    string ClientAction::Serialize() {
        auto ss = StreamableString{};
        ss << static_cast<int>(this->ActionType) << " "
           << Address.ai_socktype << " "
           << Address.ai_family << " "
           << Address.ai_addr->sa_data << " "
           << Address.ai_addr->sa_family << " "
           << Address.ai_addrlen << " "
           << Address.ai_canonname << " "
           << Address.ai_flags << " "
           << Address.ai_protocol << " "
           << Data;
        return (string)ss;
    }

    ClientAction ClientAction::Deserialize(const std::string& serializedStr) {
        std::stringstream ss(serializedStr);
        ClientAction action;
        int actionType;
        ss >> actionType;
        action.ActionType = static_cast<ClientActionType>(actionType);
        ss >> action.Address.ai_socktype;
        ss >> action.Address.ai_family;
        action.Address.ai_addr = new sockaddr;
        ss >> action.Address.ai_addr->sa_data;
        ss >> action.Address.ai_addr->sa_family;
        ss >> action.Address.ai_addrlen;
        ss >> action.Address.ai_canonname;
        ss >> action.Address.ai_flags;
        ss >> action.Address.ai_protocol;
        ss >> std::ws;
        std::getline(ss, action.Data);
        return action;
    }
}