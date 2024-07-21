cmake_minimum_required(VERSION 3.22)
project(Chat2)

set(CMAKE_CXX_STANDARD 17)

add_executable(
        Chat2
        main.cpp
        src/classes/general/Enums.h
        src/classes/general/ServerAction.cpp
        src/classes/general/ServerAction.h
        src/classes/general/ClientAction.cpp
        src/classes/general/ClientAction.h
        src/classes/general/StreamableString.h
        src/classes/server_side/RegisteredClient.cpp
        src/classes/server_side/RegisteredClient.h
        src/classes/server_side/ClientConnection.cpp
        src/classes/server_side/ClientConnection.h
        src/classes/server_side/ChatroomHost.cpp
        src/classes/server_side/ChatroomHost.h
        src/classes/server_side/Server.cpp
        src/classes/server_side/Server.h
        src/classes/client_side/Account.cpp src/classes/client_side/Account.h src/classes/client_side/ServerConnection.cpp src/classes/client_side/ServerConnection.h src/testing/TestScript1.cpp src/testing/TestScript1.h)

find_package(Threads REQUIRED)
target_link_libraries(Chat2 PRIVATE Threads::Threads pthread)

# Optional: Set compiler and linker flags explicitly
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
