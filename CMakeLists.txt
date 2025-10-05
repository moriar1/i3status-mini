cmake_minimum_required(VERSION 3.22)

project(
  MyProject
  VERSION 0.1.0
  DESCRIPTION "C Project"
  LANGUAGES C)

# ---- Sources and Includes ----
set(SOURCES src/main.c)
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)
# file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)

# ---- Compiler Standard ----
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES C_STANDARD 17
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
# target_compile_features(${PROJECT_NAME} PRIVATE c_std_17)

# ---- Inject Project Version as a Preprocessor Definition ----
target_compile_definitions(${PROJECT_NAME}
                           PRIVATE PROJECT_VERSION=\"${PROJECT_VERSION}\")

# ---- Compiler Hardening and Warning Flags ----
set(COMMON_CLANG_FLAGS
    # Security and Safety
    -fPIE
    -fcolor-diagnostics # gcc incompatible
    -fcf-protection
    -fstack-clash-protection
    -fstack-protector-strong # -all
    -fstrict-flex-arrays=3
    -fno-common
    # Warning and style checks:
    -Weverything # gcc incompatible
    -Wall
    -Wextra
    -Wpedantic
    # -Werror
    # if -Weverything is off
    -Wmissing-prototypes
    -Wstrict-prototypes
    -Wold-style-definition
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wfloat-equal
    -Wdouble-promotion
    -Wcast-qual
    -Wpointer-arith
    -Wredundant-decls
    -Wwrite-strings
    -Wtype-limits
    -Wundef
    -Wunused
    -Wcast-align
    -Wstack-protector
    -Walloca
    -Wimplicit-fallthrough
    -Wnull-dereference
    -Wformat=2
    -Wformat-signedness
    -Wstrict-overflow=2
    # Security as errors (if -Werror if off)
    -Werror=format-security
    -Werror=incompatible-pointer-types
    # Disable specific warnings (if -Weveryting is on)
    -Wno-unsafe-buffer-usage
    -Wno-declaration-after-statement
    -Wno-conditional-uninitialized
    -Wno-covered-switch-default)

# ---- Common Linker Flags ----
set(COMMON_LINK_FLAGS
    -pie
    -Wl,-z,noexecstack
    -Wl,-z,relro
    -Wl,-z,now
    -Wl,--as-needed
    -Wl,--no-copy-dt-needed-entries
    -Wl,-z,nodlopen
    -fuse-ld=lld
    -Wl,-z,defs)

# ---- Configuration: Release, Debug, RelWithDebInfo ----
set(DEBUG_FLAGS -g -O0 -DDEBUG)
set(DEBUG_LINK_FLAGS "")

set(RELEASE_FLAGS
    -O3
    -DNDEBUG
    -march=native
    -mtune=native
    -flto
    -ffunction-sections
    -fdata-sections
    # -D_FORTIFY_SOURCE=3
)
set(RELEASE_LINK_FLAGS -flto -Wl,--gc-sections)

set(RELWITHDEBINFO_FLAGS
    -O2
    -g
    -DNDEBUG
    -march=native
    -mtune=native
    -flto
    -ffunction-sections
    -fdata-sections
    # -D_FORTIFY_SOURCE=3
)
set(RELWITHDEBINFO_LINK_FLAGS ${RELEASE_LINK_FLAGS})

set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined
                    -fno-omit-frame-pointer)

# TODO?: use INTERFACE libraries for many targets
target_compile_options(
  ${PROJECT_NAME}
  PRIVATE ${COMMON_CLANG_FLAGS} $<$<CONFIG:Release>:${RELEASE_FLAGS}>
          $<$<CONFIG:Debug>:${DEBUG_FLAGS}>
          $<$<CONFIG:RelWithDebInfo>:${RELWITHDEBINFO_FLAGS}>)

target_link_options(
  ${PROJECT_NAME}
  PRIVATE
  ${COMMON_LINK_FLAGS}
  $<$<CONFIG:Release>:${RELEASE_LINK_FLAGS}>
  $<$<CONFIG:Debug>:${DEBUG_LINK_FLAGS}>
  $<$<CONFIG:RelWithDebInfo>:${RELWITHDEBINFO_LINK_FLAGS}>)

option(ENABLE_SANITIZERS
       "Enable address,undefined behavior sanitizers in Debug/RelWithDebInfo"
       ON)
if(ENABLE_SANITIZERS)
  target_compile_options(
    ${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:${SANITIZER_FLAGS}>
                            $<$<CONFIG:RelWithDebInfo>:${SANITIZER_FLAGS}>)
  target_link_options(
    ${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:${SANITIZER_FLAGS}>
    $<$<CONFIG:RelWithDebInfo>:${SANITIZER_FLAGS}>)
endif()

# ---- Threading lib ----
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# ---- Install ----
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# TODO (add gcc): if(CMAKE_C_COMPILER_ID MATCHES "Clang")
# target_compile_options(${PROJECT_NAME} PRIVATE -fcolor-diagnostics
# -Weverything) elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
# target_compile_options(${PROJECT_NAME} PRIVATE -fdiagnostics-color=always)
# endif()
