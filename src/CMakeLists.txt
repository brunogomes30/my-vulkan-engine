
# Add source to this project's executable.


#print

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Generate a list of all .c & .h files in the current directory and sub directores.
file(
     GLOB_RECURSE source_list RELATIVE
     "${CMAKE_CURRENT_SOURCE_DIR}"
     *.cpp 
     *.h
    )
add_executable (engine  ${source_list})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${source_list} PREFIX src/)



set_property(TARGET engine PROPERTY CXX_STANDARD 20)
target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
  COMMAND_EXPAND_LISTS
  )