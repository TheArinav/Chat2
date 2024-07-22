#ifndef CHAT2_CHATROOMHOST_H
#define CHAT2_CHATROOMHOST_H

#include <vector>
#include <tuple>
#include <map>
#include <string>

#include "RegisteredClient.h"

using namespace std;

namespace classes::server_side {
    class ChatroomHost {
    public:
        unsigned long long RoomID;
        string DisplayName;
        RegisteredClient *Admin;
        vector<RegisteredClient*> Members;
        map<int,tuple<unsigned long long, string>> Messages;

        ChatroomHost();
        explicit ChatroomHost(string name, RegisteredClient *Admin);
        void PushMessage(unsigned long long senderID, const string& content);
    private:
        static unsigned long long count;
    };
} // server_side

#endif //CHAT2_CHATROOMHOST_H
