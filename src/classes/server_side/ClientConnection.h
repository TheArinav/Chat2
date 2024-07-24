#ifndef CHAT2_CLIENTCONNECTION_H
#define CHAT2_CLIENTCONNECTION_H

#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <atomic>
#include <memory>
#include <mutex>
#include <functional>

typedef addrinfo AddressInfo;

using namespace std;

namespace classes::server_side {
    class RegisteredClient;

    struct ClientConnection {
    public:
        AddressInfo Address;
        thread *ManagerThread;
        int FileDescriptor;
        bool ThreadInitialized;
        shared_ptr<RegisteredClient> Host;

        ClientConnection();
        explicit ClientConnection(AddressInfo addr);
        ClientConnection(unique_ptr<ClientConnection> other) = delete;  // Deleted copy constructor
        ClientConnection &operator=(const ClientConnection &other) = delete;  // Deleted copy assignment operator
        ClientConnection(ClientConnection &&other) noexcept;
        ClientConnection &operator=(ClientConnection &&other) noexcept;
        ~ClientConnection();

        void Start(const function<void(shared_ptr<RegisteredClient> Host, int FD)>& listener);
        void Stop();
        shared_ptr<mutex> m_Host;  // Changed to shared_ptr
    private:
        unique_ptr<atomic<bool>> StopFlag;

        function<void(shared_ptr<RegisteredClient> Host, int FD)> ListenerFunction;
    };
} // server_side

#endif //CHAT2_CLIENTCONNECTION_H
