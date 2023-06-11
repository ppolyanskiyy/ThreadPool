cmake_minimum_required (VERSION 3.2)

option(BUILD_TESTS "Build test" ON)

set(PROJECT_NAME ThreadPool)
set(THREAD_POOL_LIBRARY ThreadPool)
set(THREAD_POOL_TEST UnitTests)

file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "inc/*.h")
file(GLOB THREADPOOLTASK_SRC "src/ThreadPoolTask/*.cpp")
file(GLOB THREADPOOLTASK_HEADER "inc/ThreadPoolTask/*.h")
file(GLOB TASKSCHEDULER_SRC "src/TaskScheduler/*.cpp")
file(GLOB TASKSCHEDULER_HEADER "inc/TaskScheduler/*.h")
file(GLOB THREADPOOL_SRC "src/ThreadPool/*.cpp")
file(GLOB THREADPOOL_HEADER "inc/ThreadPool/*.h")
file(GLOB OSAL_SRC "src/OSAL/*.cpp")
file(GLOB OSAL_HEADER "inc/OSAL/*.h")

project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 11)

if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

source_group("src" FILES ${SOURCES})
source_group("include" FILES ${HEADERS})

source_group("ThreadPoolTask" FILES ${THREADPOOLTASK_SRC} ${THREADPOOLTASK_HEADER})
source_group("TaskScheduler" FILES ${TASKSCHEDULER_SRC} ${TASKSCHEDULER_HEADER})
source_group("ThreadPool" FILES ${THREADPOOL_SRC} ${THREADPOOL_HEADER})
source_group("OSAL" FILES ${OSAL_SRC} ${OSAL_HEADER})

add_library(${THREAD_POOL_LIBRARY} STATIC
            ${SOURCES} ${HEADERS}
            ${THREADPOOLTASK_SRC} ${THREADPOOLTASK_HEADER}
            ${TASKSCHEDULER_SRC} ${TASKSCHEDULER_HEADER}
            ${THREADPOOL_SRC} ${THREADPOOL_HEADER}
            ${OSAL_SRC} ${OSAL_HEADER})

target_include_directories(${THREAD_POOL_LIBRARY} PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc/ThreadPoolTask
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc/TaskScheduler
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc/ThreadPool
                           ${CMAKE_CURRENT_SOURCE_DIR}/inc/OSAL
                           PRIVATE src src/ThreadPoolTask src/TaskScheduler src/ThreadPool src/OSAL)

if (BUILD_TESTS)
    message("Building Test...")

    file(GLOB UNIT_TESTS "UnitTests/*.cc")

    set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/)
    set(GTEST_INCLUDE_DIRS ${THIRD_PARTY_DIR}/googletest/googletest/include)
    set(GTEST_LIBRARIES ${THIRD_PARTY_DIR}/gtest.lib ${THIRD_PARTY_DIR}/gtest_main.lib)

    add_executable(${THREAD_POOL_TEST} ${UNIT_TESTS} ${CMAKE_CURRENT_SOURCE_DIR}/UnitTests/main.cpp)
    get_target_property(THREAD_POOL_INCLUDE_DIRS ${THREAD_POOL_LIBRARY} INTERFACE_INCLUDE_DIRECTORIES)

    target_include_directories(${THREAD_POOL_TEST} PUBLIC ${THREAD_POOL_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})
    target_link_libraries(${THREAD_POOL_TEST} PUBLIC ${THREAD_POOL_LIBRARY} ${GTEST_LIBRARIES})
endif()

