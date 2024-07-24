#include "ServerAction.h"
#include <sstream>
#include <string.h>

using namespace std;

namespace classes::general {
    ServerAction::ServerAction() = default;

    ServerAction::ServerAction(ServerActionType actType, addrinfo addr, string data) :
            ActionType(actType), Address(addr), Data(move(data)), ai_addr_ptr(nullptr) {
        if (addr.ai_addr) {
            ai_addr_ptr = make_unique<sockaddr>(*addr.ai_addr);
            Address.ai_addr = ai_addr_ptr.get();
        }
    }

    ServerAction::~ServerAction() = default;

    ServerAction::ServerAction(ServerAction&& other) noexcept :
            ActionType(other.ActionType), Address(other.Address), Data(std::move(other.Data)), ai_addr_ptr(std::move(other.ai_addr_ptr)) {
        other.Address.ai_addr = nullptr;
    }

    ServerAction& ServerAction::operator=(ServerAction&& other) noexcept {
        if (this != &other) {
            ActionType = other.ActionType;
            Address = other.Address;
            Data = std::move(other.Data);
            ai_addr_ptr = std::move(other.ai_addr_ptr);
            other.Address.ai_addr = nullptr;
        }
        return *this;
    }

    string ServerAction::Serialize() {
        auto ss = stringstream {};
        ss << static_cast<int>(this->ActionType) << " "
           << Address.ai_socktype << " "
           << Address.ai_family << " "
           << Address.ai_addrlen << " "
           << ((Address.ai_canonname != nullptr) ? Address.ai_canonname : "NULL") << " "
           << Address.ai_flags << " "
           << Address.ai_protocol << " "
           << Data;
        return ss.str();
    }

    ServerAction ServerAction::Deserialize(string& serializedStr) {
        stringstream ss(serializedStr);
        ServerAction action;
        int actionType;
        ss >> actionType;
        action.ActionType = static_cast<ServerActionType>(actionType);
        ss >> action.Address.ai_socktype;
        ss >> action.Address.ai_family;

        action.ai_addr_ptr = make_unique<sockaddr>();
        action.Address.ai_addr = action.ai_addr_ptr.get();

        ss >> action.Address.ai_addrlen;
        char tmpCannoname[1024];
        ss >> tmpCannoname;
        if (string(tmpCannoname) != "NULL") {
            action.ai_canonname_str = std::move(tmpCannoname);
            action.Address.ai_canonname = strdup(tmpCannoname);
        }
        ss >> action.Address.ai_flags;
        ss >> action.Address.ai_protocol;
        ss >> ws;
        getline(ss, action.Data, '\0'); // Get the rest of the string as Data
        return action;
    }
} // namespace classes::general
