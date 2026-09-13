set(MANAGE_SOFT_CLIENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionsettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clientupdatemanager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/clientupdatemanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/accountsecuritydialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/accountsecuritydialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionsettingsdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionsettingsdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/demandlibraryoverviewpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/demandlibraryoverviewpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/demandlibrarydetailpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/demandlibrarydetailpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryhistorydialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryhistorydialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryfulfillmentdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryfulfillmentdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryitempickerdialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryitempickerdialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventorysearchutils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventorysearchutils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryrecorddialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventoryrecorddialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inventorytransactiondialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inventorytransactiondialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/manualstockindialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/manualstockindialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scanstockindialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scanstockindialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/reimbursementoverviewpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/reimbursementoverviewpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/recorddialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/recorddialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/searchhighlightdelegate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/searchhighlightdelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/managementpage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/managementpage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logindialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/logindialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpappserviceclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tcpappserviceclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NoWheelSpinBox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NoWheelDoubleSpinBox.h
    ${CMAKE_CURRENT_SOURCE_DIR}/NoWheelDateEdit.h
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(ManageSoftCpp
        MANUAL_FINALIZATION
        ${MANAGE_SOFT_CLIENT_SOURCES}
    )
else()
    add_executable(ManageSoftCpp ${MANAGE_SOFT_CLIENT_SOURCES})
endif()

target_include_directories(ManageSoftCpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ManageSoftCpp
    PRIVATE
        ManageSoftShared
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ManageSoftCpp)
endif()
