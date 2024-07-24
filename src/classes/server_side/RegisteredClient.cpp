#include "RegisteredClient.h"
#include "ClientConnection.h"

#include <utility>

namespace classes::server_side {
    unsigned long long RegisteredClient::count = 0;

    RegisteredClient::RegisteredClient() {
        Setup();
    }

    RegisteredClient::RegisteredClient(string name) :
            DisplayName(std::move(name)) {
        Setup();
    }

    void RegisteredClient::PushResponse(ClientAction act) {
        {
            lock_guard<mutex> guard(*m_AwaitingResponses);
            AwaitingResponses.push_back(move(act));
        }
    }

    ClientAction RegisteredClient::GetResponse() {
        {
            lock_guard<mutex> guard(*m_AwaitingResponses);
            if (!AwaitingResponses.empty()) {
                PoppedEmptyFlag = false;
                auto tmp = move(AwaitingResponses.front());
                AwaitingResponses.pop_back();
                return tmp;
            }
            PoppedEmptyFlag = true;
            return {};
        }
    }

    void RegisteredClient::Setup() {
        PoppedEmptyFlag = false;
        ClientID = count++;
        IsConnected = false;
        m_AwaitingResponses = make_shared<mutex>();
    }

    RegisteredClient::RegisteredClient(RegisteredClient& other) {
        this->ClientID = other.ClientID;
        this->DisplayName = other.DisplayName;
        this->IsConnected = other.IsConnected;
        this->Connection = move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        this->m_AwaitingResponses = make_shared<mutex>();
        lock_guard<mutex> guard(*other.m_AwaitingResponses);
        this->AwaitingResponses = move(other.AwaitingResponses);
    }

    RegisteredClient::RegisteredClient(RegisteredClient&& other) noexcept {
        this->ClientID = other.ClientID;
        this->DisplayName = std::move(other.DisplayName);
        this->IsConnected = other.IsConnected;
        this->Connection = std::move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        this->m_AwaitingResponses = std::move(other.m_AwaitingResponses);
        this->AwaitingResponses = std::move(other.AwaitingResponses);

        other.ClientID = -1;
        other.PoppedEmptyFlag = false;
        other.AwaitingResponses.clear();
        other.IsConnected = false;
        other.DisplayName.clear();
        other.LoginKey.clear();
        other.Connection = nullptr;
    }

    RegisteredClient& RegisteredClient::operator=(RegisteredClient&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        this->ClientID = other.ClientID;
        this->DisplayName = std::move(other.DisplayName);
        this->IsConnected = other.IsConnected;
        this->Connection = std::move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        this->m_AwaitingResponses = std::move(other.m_AwaitingResponses);
        this->AwaitingResponses = std::move(other.AwaitingResponses);

        other.ClientID = -1;
        other.PoppedEmptyFlag = false;
        other.AwaitingResponses.clear();
        other.IsConnected = false;
        other.DisplayName.clear();
        other.LoginKey.clear();
        other.Connection = nullptr;

        return *this;
    }

    void RegisteredClient::LinkClientConnection(unique_ptr<ClientConnection> conn) {
        Connection = move(conn);
        auto self_shared = shared_from_this();  // Get a shared_ptr to this
        {
            //lock_guard<mutex> guard(*Connection->m_Host);
            Connection->Host = self_shared;
        }
    }
} // namespace classes::server_side
