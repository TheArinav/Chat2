//
// Created by ariel on 7/17/2024.
//

#include "ChatroomHost.h"

namespace classes::server_side {
    unsigned long long ChatroomHost::count = 0;
    ChatroomHost::ChatroomHost() {
        this->RoomID=count++;
    }
    ChatroomHost::ChatroomHost(string name, RegisteredClient *Admin) :
    DisplayName(move(name)){
        Members.push_back(Admin);
        this->Admin=Admin;
    }

    void ChatroomHost::PushMessage(unsigned long long int senderID, const string& content) {
        Messages.emplace(this->Messages.size(),tuple(senderID,content));
    }
}