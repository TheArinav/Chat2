#include "ClientAction.h"
#include <sstream>
#include <string.h>

using namespace std;

namespace classes::general {
    ClientAction::ClientAction() = default;

    ClientAction::ClientAction(ClientActionType actType, addrinfo addr, std::string data, bool IsLast)
            : ActionType(actType), Address(addr), Data(std::move(data)), IsLast(IsLast), ai_addr_ptr(nullptr) {
        if (addr.ai_addr) {
            ai_addr_ptr = std::make_unique<sockaddr>(*addr.ai_addr);
            Address.ai_addr = ai_addr_ptr.get();
        }
        if (addr.ai_canonname) {
            ai_canonname_str = addr.ai_canonname;
            Address.ai_canonname = strdup(ai_canonname_str.c_str());
        }
    }

    ClientAction::~ClientAction() {
        // Ensure to reset pointers to avoid double-free or dangling pointer issues
        if (Address.ai_addr == ai_addr_ptr.get()) {
            Address.ai_addr = nullptr;
        }
        Address.ai_canonname = nullptr;
    }

    ClientAction::ClientAction(ClientAction&& other) noexcept
            : ActionType(other.ActionType), Address(other.Address), Data(std::move(other.Data)),
              ai_addr_ptr(std::move(other.ai_addr_ptr)), ai_canonname_str(std::move(other.ai_canonname_str)) {
        other.Address.ai_addr = nullptr;
        other.Address.ai_canonname = nullptr;
    }

    ClientAction& ClientAction::operator=(ClientAction&& other) noexcept {
        if (this != &other) {
            ActionType = other.ActionType;
            Address = other.Address;
            Data = std::move(other.Data);
            ai_addr_ptr = std::move(other.ai_addr_ptr);
            ai_canonname_str = std::move(other.ai_canonname_str);
            other.Address.ai_addr = nullptr;
            other.Address.ai_canonname = nullptr;
        }
        return *this;
    }

    string ClientAction::Serialize() {
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

    ClientAction ClientAction::Deserialize(string& serializedStr) {
        stringstream ss(serializedStr);
        ClientAction action;
        int actionType;
        ss >> actionType;
        action.ActionType = static_cast<ClientActionType>(actionType);
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
