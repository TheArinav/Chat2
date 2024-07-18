#ifndef CHAT2_ENUMS_H
#define CHAT2_ENUMS_H

namespace classes::general{
    enum class ClientActionType{
        InformActionSuccess,
        InformActionFailure,
        MessageReceived,
        JoinedChatroom,
        LeftChatroom
    };
    enum class ServerActionType{
        SendMessage,
        RegisterClient,
        LoginClient,
        LogoutClient,
        CreateChatroom,
        RemoveChatroom,
        AddChatRoomMember,
        RemoveChatroomMember
    };
}

#endif
