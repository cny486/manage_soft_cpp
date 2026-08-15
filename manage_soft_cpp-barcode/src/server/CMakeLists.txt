set(MANAGE_SOFT_SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/server_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpbackendserver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpbackendserver.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(ManageSoftServer
        MANUAL_FINALIZATION
        ${MANAGE_SOFT_SERVER_SOURCES}
    )
else()
    add_executable(ManageSoftServer ${MANAGE_SOFT_SERVER_SOURCES})
endif()

target_include_directories(ManageSoftServer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ManageSoftServer
    PRIVATE
        ManageSoftShared
        Qt${QT_VERSION_MAJOR}::Network
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ManageSoftServer)
endif()