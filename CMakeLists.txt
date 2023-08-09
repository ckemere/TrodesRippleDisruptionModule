cmake_minimum_required(VERSION 3.5)

project(RippleDisruption VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Charts)
find_package(Qt6 REQUIRED COMPONENTS Widgets Xml Network)

# set up Trodes interface link_directories
set(TRODES_DIR /home/kemerelab/Code/Trodes)
set(TRODES_NETWORK_DIR ${TRODES_DIR}/trodesnetwork)
link_directories(${TRODES_DIR}/Libraries/Linux/TrodesNetwork/lib)


set(PROJECT_SOURCES
    src/main.cpp 
    src/mainwindow.cpp
    src/mainwindow.ui
    
    src/trodesinterface.cpp
    src/stiminterface.cpp

    src/ripplepower.cpp
)

set(PROJECT_INCLUDES
    include/moduledefines.h
    include/mainwindow.h
    include/trodesinterface.h
    include/stiminterface.h
    include/ripplepower.h
)

set(TRODES_SOURCES
    ${TRODES_DIR}/Trodes/src-main/trodesSocketDefines.h
    ${TRODES_DIR}/Trodes/src-config/configuration.h
    ${TRODES_DIR}/Trodes/src-config/configuration.cpp
)

qt_add_executable(RippleDisruption
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    ${PROJECT_INCLUDES}
    ${TRODES_SOURCES}
)

set_source_files_properties(${PROJECT_SOURCES} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -Wconversion -Wsign-conversion")
#add_executable(RippleDisruption
#    ${PROJECT_SOURCES}
#)

target_include_directories(RippleDisruption 
    PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${TRODES_NETWORK_DIR}/include
    ${TRODES_DIR}
    ${TRODES_DIR}/Trodes/src-main/
    ${TRODES_DIR}/Trodes/src-config/
    SYSTEM
    ${TRODES_DIR}/extern/msgpack-c/include
    ${TRODES_DIR}/extern/cppzmq
    ${TRODES_DIR}/extern/zmq/include
    )

target_link_libraries(RippleDisruption 
    PRIVATE 
    Qt6::Widgets 
    Qt6::Xml
    Qt6::Network
    trodesnetwork
    )

target_compile_definitions(RippleDisruption PRIVATE
    GIT_COMMIT="${GIT_COMMIT_STRING}" # needed for stuff included in in configuration reading
    # TRODES_CODE
)
# target_compile_options(RippleDisruption PRIVATE -Wall -Wextra -Wconversion -Wsign-conversion)
# target_compile_options(RippleDisruption PRIVATE -Werror -Wall -Wextra -Wconversion -Wsign-conversion)

set_target_properties(RippleDisruption PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS RippleDisruption
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(RippleDisruption)
