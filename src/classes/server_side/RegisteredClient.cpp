//
// Created by ariel on 7/17/2024.
//

#include "RegisteredClient.h"

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
            //Critical Section
            lock_guard<mutex> guard(*m_AwaitingResponses);
            AwaitingResponses.push_back(act);
        }
    }


    ClientAction RegisteredClient::GetResponse() {
        {
            //Critical Section
            if (!AwaitingResponses.empty()) {
                PoppedEmptyFlag = false;
                auto tmp = AwaitingResponses.front();
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
        m_AwaitingResponses = make_unique<mutex>();
    }

    RegisteredClient::RegisteredClient(RegisteredClient &other) {
        this->ClientID = other.ClientID;
        this->DisplayName = other.DisplayName;
        this->IsConnected= other.IsConnected;
        this->Connection= move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        // Deep copy of the mutex and the responses
        this->m_AwaitingResponses = make_unique<mutex>();
        lock_guard<mutex> guard(*other.m_AwaitingResponses);
        this->AwaitingResponses = other.AwaitingResponses;
    }


    RegisteredClient::RegisteredClient(RegisteredClient &&other) noexcept {
        this->ClientID = other.ClientID;
        this->DisplayName = std::move(other.DisplayName);
        this->IsConnected = other.IsConnected;
        this->Connection = std::move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        // Move the mutex and the responses
        this->m_AwaitingResponses = std::move(other.m_AwaitingResponses);
        this->AwaitingResponses = std::move(other.AwaitingResponses);

        // Reset the state of the moved-from object
        other.ClientID=-1;
        other.PoppedEmptyFlag= false;
        other.AwaitingResponses.clear();
        other.IsConnected= false;
        other.DisplayName.clear();
        other.LoginKey.clear();
        other.Connection=nullptr;
    }

    RegisteredClient &RegisteredClient::operator=(RegisteredClient &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        this->ClientID = other.ClientID;
        this->DisplayName = std::move(other.DisplayName);
        this->IsConnected = other.IsConnected;
        this->Connection = std::move(other.Connection);
        this->PoppedEmptyFlag = other.PoppedEmptyFlag;

        // Move the mutex and the responses
        this->m_AwaitingResponses = std::move(other.m_AwaitingResponses);
        this->AwaitingResponses = std::move(other.AwaitingResponses);

        // Reset the state of the moved-from object
        other.ClientID=-1;
        other.PoppedEmptyFlag= false;
        other.AwaitingResponses.clear();
        other.IsConnected= false;
        other.DisplayName.clear();
        other.LoginKey.clear();
        other.Connection=nullptr;

        return *this;
    }
} // server_side