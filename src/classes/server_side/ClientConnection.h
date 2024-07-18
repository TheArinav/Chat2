#ifndef CHAT2_CLIENTCONNECTION_H
#define CHAT2_CLIENTCONNECTION_H

#include <sys/socket.h>
#include <netdb.h>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

typedef addrinfo AddressInfo;

using namespace std;

namespace classes::server_side {

    struct ClientConnection {
    public:
        AddressInfo Address;
        thread *ManagerThread;
        int FileDescriptor;
        bool ThreadInitialized;

        ClientConnection();
        explicit ClientConnection(AddressInfo addr);
        ClientConnection(const ClientConnection &other);
        ClientConnection &operator=(const ClientConnection &other);
        ClientConnection(ClientConnection &&other) noexcept;
        ClientConnection &operator=(ClientConnection &&other) noexcept;
        ~ClientConnection();

        void Start(function<void(int)> listener);
        void Stop();

    private:
        unique_ptr<atomic<bool>> StopFlag;
        function<void(int)> ListenerFunction;
    };
} // server_side

#endif //CHAT2_CLIENTCONNECTION_H