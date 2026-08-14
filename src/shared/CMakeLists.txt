set(MANAGE_SOFT_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/appversion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/appversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aiapisettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aiapisettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aiinventoryenricher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aiinventoryenricher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/appservice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/appschema.h
    ${CMAKE_CURRENT_SOURCE_DIR}/datainitializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/datainitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emailsettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/emailsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jsonstorageservice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/jsonstorageservice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simplexlsxdocument.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simplexlsxdocument.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/smtpemailclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/smtpemailclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpmessagecodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpmessagecodec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/updateinfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/updateinfo.cpp
)

add_library(ManageSoftShared STATIC ${MANAGE_SOFT_SHARED_SOURCES})

target_include_directories(ManageSoftShared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ManageSoftShared
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
        ZLIB::ZLIB
)
