#include "ClientConnection.h"
#include "RegisteredClient.h"
#include <memory>
#include <mutex>

using namespace std;

namespace classes::server_side {

    ClientConnection::ClientConnection()
            : ManagerThread(nullptr), FileDescriptor(-1), ThreadInitialized(false),
              StopFlag(make_shared<atomic<bool>>(false)), ListenerFunction(nullptr), m_Host(make_shared<mutex>()) {}

    ClientConnection::ClientConnection(AddressInfo addr)
            : Address(addr), ManagerThread(nullptr), FileDescriptor(-1), ThreadInitialized(false),
              StopFlag(make_shared<atomic<bool>>(false)), ListenerFunction(nullptr), m_Host(make_shared<mutex>()) {}

    ClientConnection::ClientConnection(ClientConnection &&other) noexcept
            : Address(other.Address), ManagerThread(other.ManagerThread), FileDescriptor(other.FileDescriptor),
              ThreadInitialized(other.ThreadInitialized), StopFlag(move(other.StopFlag)),
              ListenerFunction(other.ListenerFunction), m_Host(move(other.m_Host)) {
        other.ManagerThread = nullptr;
        other.ThreadInitialized = false;
    }

    ClientConnection &ClientConnection::operator=(ClientConnection &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        Stop();  // Ensure the current instance stops its thread if running.

        Address = other.Address;
        FileDescriptor = other.FileDescriptor;
        ThreadInitialized = other.ThreadInitialized;
        StopFlag = move(other.StopFlag);
        ListenerFunction = other.ListenerFunction;
        ManagerThread = other.ManagerThread;
        m_Host = move(other.m_Host);

        other.ManagerThread = nullptr;
        other.ThreadInitialized = false;

        return *this;
    }

    ClientConnection::~ClientConnection() {
        Stop();
        delete ManagerThread;
    }

    void ClientConnection::Start(const function<void(shared_ptr<RegisteredClient> Host, int FD, shared_ptr<atomic<bool>> stop)>& listener) {
        if (ThreadInitialized || !listener) {
            return;
        }
        ListenerFunction = listener;
        StopFlag = make_unique<atomic<bool>>(false);
        ManagerThread = new thread([this] {
            while (!StopFlag->load()) {
                {
                    lock_guard<mutex> guard(*m_Host);
                    ListenerFunction(Host, FileDescriptor, StopFlag);
                }
            }
        });
        ManagerThread->detach();
        ThreadInitialized = true;
    }

    void ClientConnection::Stop() {
        if (ThreadInitialized) {
            StopFlag->store(true);
            if (ManagerThread->joinable()) {
                ManagerThread->join();
            }
            delete ManagerThread;
            ManagerThread = nullptr;
            ThreadInitialized = false;
        }
    }
} // namespace classes::server_side
