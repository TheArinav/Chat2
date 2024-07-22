
#include "ClientConnection.h"

namespace classes::server_side {

    ClientConnection::ClientConnection()
            : ManagerThread(nullptr), FileDescriptor(-1), ThreadInitialized(false),
              StopFlag(make_unique<atomic<bool>>(false)), ListenerFunction(nullptr) {}

    ClientConnection::ClientConnection(AddressInfo addr)
            : Address(addr), ManagerThread(nullptr), FileDescriptor(-1), ThreadInitialized(false),
              StopFlag(make_unique<atomic<bool>>(false)), ListenerFunction(nullptr) {}

    ClientConnection::ClientConnection(const ClientConnection &other)
            : Address(other.Address), FileDescriptor(other.FileDescriptor), ThreadInitialized(other.ThreadInitialized),
              StopFlag(make_unique<atomic<bool>>((bool)other.StopFlag.get())), ListenerFunction(other.ListenerFunction) {
        if (other.ThreadInitialized) {
            ManagerThread = new thread([this] {
                while (!StopFlag->load()) {
                    ListenerFunction(FileDescriptor);
                }
            });
        } else {
            ManagerThread = nullptr;
        }
    }

    ClientConnection &ClientConnection::operator=(const ClientConnection &other) {
        if (this == &other) {
            return *this;
        }

        Stop();  // Ensure the current instance stops its thread if running.

        Address = other.Address;
        FileDescriptor = other.FileDescriptor;
        ThreadInitialized = other.ThreadInitialized;
        StopFlag = make_unique<atomic<bool>>((bool)other.StopFlag.get());
        ListenerFunction = other.ListenerFunction;

        if (other.ThreadInitialized) {
            ManagerThread = new thread([this] {
                while (!StopFlag->load()) {
                    ListenerFunction(FileDescriptor);
                }
            });
        } else {
            ManagerThread = nullptr;
        }

        return *this;
    }

    ClientConnection::ClientConnection(ClientConnection &&other) noexcept
            : Address(other.Address), ManagerThread(other.ManagerThread), FileDescriptor(other.FileDescriptor),
              ThreadInitialized(other.ThreadInitialized), StopFlag(move(other.StopFlag)),
              ListenerFunction(other.ListenerFunction) {
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

        other.ManagerThread = nullptr;
        other.ThreadInitialized = false;

        return *this;
    }

    ClientConnection::~ClientConnection() {
        Stop();
        if (ManagerThread) {
            delete ManagerThread;
        }
    }

    void ClientConnection::Start(function<void(int)> listener) {
        if (!listener || ThreadInitialized) {
            return;
        }
        ListenerFunction = listener;
        StopFlag = make_unique<atomic<bool>>(false);
        ManagerThread = new thread([this] {
            while (!StopFlag->load()) {
                ListenerFunction(FileDescriptor);
            }
        });
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