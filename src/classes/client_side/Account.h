#ifndef CHAT2_ACCOUNT_H
#define CHAT2_ACCOUNT_H

#include <vector>
#include <map>
#include <tuple>
#include <string>

using namespace std;

namespace classes::client_side {

    class Account {
    public:
        unsigned long long ID;
        string ConnectionKey;
        map<string, unsigned long long> Friends;
        map<string, unsigned long long> ChatRooms;
        vector<tuple<unsigned long long, unsigned long long, string>> Messages;

        Account();
    };

}

#endif //CHAT2_ACCOUNT_H
