cmake_minimum_required(VERSION 2.8.7)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

set(PROJ nonblockpp)
project(nonblockpp)

if(NOT ${CMAKE_VERSION} LESS 3.2)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
else()
    message(STATUS "Checking compiler flags for C++11 support.")
    # Set C++11 support flags for various compilers
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-std=c++11" COMPILER_SUPPORTS_CXX11)
    check_cxx_compiler_flag("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
    if(COMPILER_SUPPORTS_CXX11)
        message(STATUS "C++11 is supported.")
        if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        endif()
    elseif(COMPILER_SUPPORTS_CXX0X)
        message(STATUS "C++0x is supported.")
        if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        endif()
    else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()
endif()

IF (NOT DEFINED CMAKE_INSTALL_BINDIR)
SET(CMAKE_INSTALL_BINDIR /usr/local/bin)
ENDIF (NOT DEFINED CMAKE_INSTALL_BINDIR)

IF (NOT DEFINED CMAKE_INSTALL_LIBDIR)
SET(CMAKE_INSTALL_LIBDIR /usr/local/lib)
ENDIF (NOT DEFINED CMAKE_INSTALL_LIBDIR)

IF (NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
SET(CMAKE_INSTALL_INCLUDEDIR /usr/local/include/nonblockpp)
ENDIF(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

MESSAGE(STATUS "Current LIBRARY DIRECTORY located at ${CMAKE_INSTALL_LIBDIR}")
MESSAGE(STATUS "Current SOURCES DIRECTORY located at ${CMAKE_CURRENT_SOURCE_DIR}")
MESSAGE(STATUS "Current Binary DIRECTORY located at ${CMAKE_CURRENT_BINARY_DIR}")

SET(LIBPATH /usr/lib /usr/local/lib)

include_directories(src /usr/local/include)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

file(GLOB SRCS src/*.cpp)
file(GLOB HDRS src/*.h)
file(GLOB_RECURSE SRCS_TEST test/*.cpp)


find_library(THREAD_LIB NAMES pthread PATHS ${LIBPATH})
if(THREAD_LIB)  
  message( STATUS "pthread found.")
else(THREAD_LIB)
  message( FATAL_ERROR "pthread not found. Please install pthread")
endif(THREAD_LIB)






# add_executable(${PROJ} ${HDRS} ${SRCS})


add_library(${PROJ}.static STATIC ${HDRS} ${SRCS})
set_target_properties(${PROJ}.static PROPERTIES OUTPUT_NAME ${PROJ})
add_library(${PROJ} SHARED ${HDRS} ${SRCS})

link_directories(/usr/local/lib /usr/lib)

target_link_libraries(${PROJ} pthread)


###############################################################################
## testing ####################################################################
###############################################################################

# this is for our testing framework
# we don't add REQUIRED because it's just for testing
find_package(GTest)

if(GTEST_FOUND)
  message(STATUS "GTEST Available.")
  enable_testing()

  add_executable(unit_tests ${SRCS_TEST})

  # we add this define to prevent collision with the main
  # this might be better solved by not adding the source with the main to the
  # testing target
  target_compile_definitions(unit_tests PUBLIC UNIT_TESTS)

  # this allows us to use our executable as a link library
  # therefore we can inherit all compiler options and library dependencies
  set_target_properties(${PROJ} PROPERTIES ENABLE_EXPORTS on)

  target_link_libraries(unit_tests PUBLIC
    ${GTEST_BOTH_LIBRARIES} nonblockpp pthread
    # addr_svc_cpp
  )

  target_include_directories(unit_tests PUBLIC
    ${GTEST_INCLUDE_DIRS} # doesn't do anything on Linux
  )

  add_test(unblockppTest unit_tests)

endif()

install(TARGETS ${PROJ} EXPORT ${PROJ}
        ARCHIVE DESTINATION lib${LIB_SUFFIX}
        LIBRARY DESTINATION lib${LIB_SUFFIX}
        # RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
install(TARGETS ${PROJ}.static EXPORT ${PROJ}
        ARCHIVE DESTINATION lib${LIB_SUFFIX}
        LIBRARY DESTINATION lib${LIB_SUFFIX}
        # RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

install(FILES src/nonblock.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(CODE "execute_process(COMMAND ldconfig)")
