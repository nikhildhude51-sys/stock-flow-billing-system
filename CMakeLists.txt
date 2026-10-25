cmake_minimum_required(VERSION 3.20)
project(InventoryBillingSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Definitions required for Windows socket and standalone Asio
add_definitions(-DASIO_STANDALONE -D_WIN32_WINNT=0x0601)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/Product.cpp
    src/Inventory.cpp
    src/Bill.cpp
    src/BillingSystem.cpp
    src/AuthManager.cpp
    src/main.cpp
)

# Server executable
add_executable(server ${SOURCES})

# Windows network and socket libraries + static runtime linkage
if(WIN32)
    target_link_libraries(server PRIVATE ws2_32 mswsock)
    target_link_options(server PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Automated Test Executable
add_executable(test_auth
    tests/test_auth.cpp
    src/Product.cpp
    src/Inventory.cpp
    src/Bill.cpp
    src/BillingSystem.cpp
    src/AuthManager.cpp
)

if(WIN32)
    target_link_libraries(test_auth PRIVATE ws2_32 mswsock)
    target_link_options(test_auth PRIVATE -static -static-libgcc -static-libstdc++)
endif()
