cmake_minimum_required(VERSION 3.10)
project(StudyMate3D VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src/backend)

set(SOURCES
    src/backend/main.cpp
)

set(HEADERS
    src/backend/server.h
    src/backend/auth.h
    src/backend/ai_service.h
    src/backend/models.h
    src/backend/storage.h
    src/backend/progress.h
    src/backend/crypto.h
    src/backend/http_types.h
    src/backend/sm_json.h
)

add_executable(studymate_server ${SOURCES} ${HEADERS})

if(WIN32)
    target_link_libraries(studymate_server ws2_32 advapi32 wininet)
endif()
