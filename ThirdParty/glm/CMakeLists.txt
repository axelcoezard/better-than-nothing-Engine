cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
cmake_policy(VERSION 3.2)


file(READ "include/detail/setup.hpp" GLM_SETUP_FILE)
string(REGEX MATCH "#define[ ]+GLM_VERSION_MAJOR[ ]+([0-9]+)" _ ${GLM_SETUP_FILE})
set(GLM_VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define[ ]+GLM_VERSION_MINOR[ ]+([0-9]+)" _ ${GLM_SETUP_FILE})
set(GLM_VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define[ ]+GLM_VERSION_PATCH[ ]+([0-9]+)" _ ${GLM_SETUP_FILE})
set(GLM_VERSION_PATCH "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define[ ]+GLM_VERSION_REVISION[ ]+([0-9]+)" _ ${GLM_SETUP_FILE})
set(GLM_VERSION_REVISION "${CMAKE_MATCH_1}")

set(GLM_VERSION ${GLM_VERSION_MAJOR}.${GLM_VERSION_MINOR}.${GLM_VERSION_PATCH}.${GLM_VERSION_REVISION})
project(glm VERSION ${GLM_VERSION} LANGUAGES CXX)
message(STATUS "GLM: Version " ${GLM_VERSION})

add_subdirectory(include)
add_library(glm::glm ALIAS glm)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})

	include(CPack)
	install(DIRECTORY glm DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "CMakeLists.txt" EXCLUDE)
	install(EXPORT glm FILE glmConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glm NAMESPACE glm::)
	include(CMakePackageConfigHelpers)
	write_basic_package_version_file("glmConfigVersion.cmake" COMPATIBILITY AnyNewerVersion)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/glmConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glm)

endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
