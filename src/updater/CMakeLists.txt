set(MANAGE_SOFT_UPDATER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/updater_main.cpp
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(ManageSoftUpdater
        MANUAL_FINALIZATION
        ${MANAGE_SOFT_UPDATER_SOURCES}
    )
else()
    add_executable(ManageSoftUpdater ${MANAGE_SOFT_UPDATER_SOURCES})
endif()

target_link_libraries(ManageSoftUpdater
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ManageSoftUpdater)
endif()
