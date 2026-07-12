file(GLOB iqdb_SRC CONFIGURE_DEPENDS "*.h" "*.cpp")
add_executable(iqdb ${iqdb_SRC})

target_link_libraries(
  iqdb PRIVATE
  Threads::Threads
  nlohmann_json::nlohmann_json
  httplib::httplib
  sqlite_orm::sqlite_orm
  ${CMAKE_DL_LIBS} # libdl (for dlsym)
)

# https://cmake.org/cmake/help/latest/command/target_include_directories.html
target_include_directories(iqdb PUBLIC ../include)

# Treat these headers as system headers (using -isystem instead of -I), so they
# don't trigger compiler warnings.
# https://gcc.gnu.org/onlinedocs/cpp/System-Headers.html
get_target_property(NLOHMANN_JSON_INCLUDE_DIR nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(SQLITE_ORM_INCLUDE_DIR sqlite_orm::sqlite_orm INTERFACE_INCLUDE_DIRECTORIES)

target_include_directories(iqdb SYSTEM PRIVATE
  ${HTTPLIB_INCLUDE_DIR}
  ${NLOHMANN_JSON_INCLUDE_DIR}
  ${SQLITE_ORM_INCLUDE_DIR}
)

set(IQDB_DEBUG_CFLAGS
  # https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html
  -g3

  # https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
  -Wall -Wextra -Werror -pedantic -pedantic-errors -Wno-unused-parameter
  -Wdouble-promotion -Wformat-overflow=2 -Wuninitialized -Wstrict-overflow=5
  -Wconversion -Wshadow

  # https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
  -fsanitize=undefined -fsanitize=leak -fsanitize=address -fstack-protector-all

  # Provides lightweight support for detecting buffer overflows in various
  # functions that perform operations on memory and strings
  # https://access.redhat.com/blogs/766093/posts/3606481
  # https://access.redhat.com/blogs/766093/posts/1976213
  -D_FORTIFY_SOURCE=2

  # When defined, enables extra error checking in the form of precondition
  # assertions, such as bounds checking in strings and null pointer checks when
  # dereferencing smart pointers.
  # https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
  -D_GLIBCXX_ASSERTIONS

  # When defined, compiles user code using the debug mode. When defined,
  # _GLIBCXX_ASSERTIONS is defined automatically, so all the assertions enabled
  # by that macro are also enabled in debug mode.
  # https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
  # XXX Disabled because our libraries (e.g. httplib) must be built with it too.
  #-D_GLIBCXX_DEBUG

  # When defined while compiling with the debug mode, makes the debug mode
  # extremely picky by making the use of libstdc++ extensions and
  # libstdc++-specific behavior into errors.
  # https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
  #-D_GLIBCXX_DEBUG_PEDANTIC

  # When defined, std::vector operations will be annotated so that
  # AddressSanitizer can detect invalid accesses to the unused capacity of a
  # std::vector.
  # https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_macros.html
  -D_GLIBCXX_SANITIZE_VECTOR
)

# -fsanitize needs to be passed to both the compiler and the linker
set(IQDB_DEBUG_LDFLAGS -fsanitize=undefined -fsanitize=leak -fsanitize=address)

# https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html
# https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
# https://funroll-loops.oya.to/
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
  set(IQDB_DOCKER_CFLAGS -Wall -O3 -g3 -pipe -DNDEBUG -flto -fno-strict-aliasing -march=armv8-a)
else()
  set(IQDB_DOCKER_CFLAGS -Wall -O3 -g3 -pipe -DNDEBUG -flto -fno-strict-aliasing -march=x86)
endif()

target_compile_options(iqdb PRIVATE $<$<CONFIG:DEBUG>:${IQDB_DEBUG_CFLAGS}>)
target_compile_options(iqdb PRIVATE $<$<CONFIG:RELEASE>:${IQDB_RELEASE_CFLAGS}>)

# GCC's -Wstrict-overflow is emitted by the optimizer rather than the front
# end, so unlike our other warnings it isn't suppressed by marking
# nlohmann/json's headers as system includes above. Downgrade it to a
# non-fatal warning for the one file that includes json.hpp.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89543
set_source_files_properties(server.cpp PROPERTIES
  COMPILE_OPTIONS "$<$<CONFIG:DEBUG>:-Wno-error=strict-overflow>"
)

target_link_options(iqdb PRIVATE $<$<CONFIG:DEBUG>:${IQDB_DEBUG_LDFLAGS}>)
