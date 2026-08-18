#include "../../buildcpp/build.hpp"

#include <fstream>
#include <string_view>
#include <vector>

// Step 1 of the CMake -> buildcpp port (see repo notes). Scope: src/rp2_common +
// src/rp2040 + src/common, RP2040 only. src/host is excluded — it defines the same
// target names (hardware_gpio, pico_stdlib, ...) with colliding strong symbols
// (gpio_set_function etc. in both src/host/hardware_gpio/gpio.c and
// src/rp2_common/hardware_gpio/gpio.c), so it can't be linked into the same library.
// src/rp2350 is excluded — different platform.

namespace {

// From pico_sdk_version.cmake.
constexpr std::string_view kSdkVersionMajor    = "2";
constexpr std::string_view kSdkVersionMinor    = "3";
constexpr std::string_view kSdkVersionRevision = "0";
constexpr std::string_view kSdkVersionString   = "2.3.0";

const std::vector<std::filesystem::path> kSourceRoots = {
    "src/rp2_common",
    "src/rp2040",
    "src/common",
};

// Whole directories left out of this build: components needing an external submodule
// that isn't cloned into this fork (lib/tinyusb, lib/lwip, lib/mbedtls,
// lib/cyw43-driver), pico_status_led (needs pioasm PIO codegen, not ported yet),
// boot_stage2 and the RP2350 CMSIS device tree (out of scope — firmware-image /
// wrong-platform concerns, not part of "compile the SDK as a library").
// Beyond the submodule/pio/rtt exclusions below, src/cmake/rp2_common.cmake (the real
// CMake aggregator that add_subdirectory()'s every rp2_common component — the
// authoritative source for this, not each leaf CMakeLists.txt individually) gates a
// handful of directories on `if (PICO_COMBINED_DOCS OR NOT PICO_RP2040)` (RP2350-only
// peripherals: powman, psram, sha256, dcp, rcp, the RISC-V platform timer) or
// `if (PICO_RISCV OR PICO_COMBINED_DOCS)` (RISC-V-only: hardware_riscv, hardware_hazard3).
// None of those apply when PICO_RP2040=1, PICO_RISCV=0, so they're excluded here too.
const std::vector<std::filesystem::path> kExcludedDirs = {
    "src/rp2_common/tinyusb",
    "src/rp2_common/pico_stdio_usb",
    "src/rp2_common/pico_usb_reset",
    "src/rp2_common/pico_btstack",
    "src/rp2_common/pico_lwip",
    "src/rp2_common/pico_mbedtls",
    "src/rp2_common/pico_cyw43_arch",
    "src/rp2_common/pico_cyw43_driver",
    "src/rp2_common/pico_status_led",
    "src/rp2_common/pico_stdio_rtt", // needs third-party SEGGER_RTT.h, not vendored in this fork
    "src/rp2040/boot_stage2",
    "src/rp2_common/cmsis/stub/CMSIS/Device/RP2350",
    "src/rp2_common/hardware_powman",             // RP2350-only peripheral
    "src/rp2_common/hardware_psram",               // RP2350-only peripheral
    "src/rp2_common/hardware_riscv_platform_timer", // RISC-V-gated in rp2_common.cmake
    "src/rp2_common/hardware_sha256",               // RP2350-only peripheral (no hardware/structs/sha256.h on RP2040)
    "src/rp2_common/hardware_dcp",                  // RP2350-only peripheral
    "src/rp2_common/hardware_rcp",                  // RP2350-only peripheral
    "src/rp2_common/hardware_riscv",                // RISC-V only
    "src/rp2_common/hardware_hazard3",              // RISC-V only
    "src/rp2_common/pico_sha256",                   // depends on hardware_sha256, RP2350-only
};

// Individual files left out within an otherwise-included directory: pico-sdk's CMake
// build picks exactly one of these per component via a generator-expression-selected
// "implementation" (PICO_DEFAULT_*_IMPL, default "pico") that only resolves at the
// final executable target. We bake in that same default ("pico"/RP2040-optimized)
// now, since we're building one concrete library rather than deferring the choice.
const std::vector<std::filesystem::path> kExcludedFiles = {
    "src/rp2_common/hardware_divider/divider.c",                 // RP2040 uses divider.S instead
    "src/rp2_common/pico_divider/divider_compiler.c",            // "pico" impl uses divider_hardware.S instead
    "src/rp2_common/pico_double/double_none.S",                  // "none" impl, not the default
    "src/rp2_common/pico_double/double_aeabi_dcp.S",             // RP2350 DCP-only variant
    "src/rp2_common/pico_double/double_fma_dcp.S",               // RP2350 DCP-only variant
    "src/rp2_common/pico_double/double_sci_m33.S",                // RP2350 DCP-only variant
    "src/rp2_common/pico_double/double_conv_m33.S",               // RP2350 DCP-only variant
    "src/rp2_common/pico_float/float_none.S",                     // "none" impl, not the default
    "src/rp2_common/pico_float/float_aeabi_dcp.S",                // RP2350 DCP-only variant
    "src/rp2_common/pico_float/float_common_m33.S",                // RP2350 DCP/VFP-only variant
    "src/rp2_common/pico_float/float_sci_m33.S",                   // RP2350 DCP-only variant
    "src/rp2_common/pico_float/float_conv32_vfp.S",                // RP2350 VFP-only variant
    "src/rp2_common/pico_float/float_sci_m33_vfp.S",               // RP2350 VFP-only variant
    "src/rp2_common/pico_float/float_single_hazard3.S",            // RISC-V only variant
    "src/rp2_common/pico_printf/printf_none.S",                    // "none" impl, not the default
    "src/rp2_common/hardware_exception/exception_table_riscv.S",   // RISC-V only
    "src/rp2_common/pico_crt0/crt0_riscv.S",                       // RISC-V only (RP2040 uses crt0.S)
    // pico_clib_interface: three alternative C-library backends (newlib/picolibc/
    // llvm_libc), CMake auto-detects and links exactly one (PICO_CLIB). We're building
    // against the ARM GNU Toolchain's newlib sysroot, so keep newlib_interface.c (+
    // the shared cxa_guard.c) and drop the other two backends' sources.
    "src/rp2_common/pico_clib_interface/picolibc_interface.c",
    "src/rp2_common/pico_clib_interface/llvm_libc_interface.c",
    "src/rp2_common/pico_clib_interface/llvm_libc_interface_cpp.cpp",
    // pico_async_context: 4 independent backends (base, poll, threadsafe_background,
    // freertos), app picks one at link time. base/poll/threadsafe_background are
    // self-contained; freertos needs an external FreeRTOS submodule we don't have.
    "src/rp2_common/pico_async_context/async_context_freertos.c",
};

bool isExcludedDir(const std::filesystem::path& path) {
    for (const auto& excluded : kExcludedDirs) {
        if (path == excluded) return true;
    }
    return false;
}

bool isExcludedFile(const std::filesystem::path& path) {
    for (const auto& excluded : kExcludedFiles) {
        if (path == excluded) return true;
    }
    return false;
}

// Generates the two headers pico-sdk's CMake build produces at configure time
// (src/common/pico_base_headers/generate_config_header.cmake +
// .../include/pico/version.h.in) and that don't exist as checked-in files.
// pico.h includes version.h; pico/config.h includes config_autogen.h — both are
// transitively pulled in by nearly every component.
void generateHeaders(const std::filesystem::path& build_dir) {
    std::filesystem::path out_dir = build_dir / "generated" / "pico_base" / "pico";
    std::filesystem::create_directories(out_dir);

    std::ofstream version(out_dir / "version.h");
    version << "#ifndef _PICO_VERSION_H\n#define _PICO_VERSION_H\n\n"
            << "#define PICO_SDK_VERSION_MAJOR    " << kSdkVersionMajor << "\n"
            << "#define PICO_SDK_VERSION_MINOR    " << kSdkVersionMinor << "\n"
            << "#define PICO_SDK_VERSION_REVISION " << kSdkVersionRevision << "\n"
            << "#define PICO_SDK_VERSION_STRING   \"" << kSdkVersionString << "\"\n\n"
            << "#endif\n";

    // Upstream dumps PICO_CONFIG_HEADER_FILES (empty by default) then
    // PICO_RP2040_CONFIG_HEADER_FILES (also empty by default) as #includes here.
    // cmake/generic_board.cmake additionally appends the resolved board header path
    // into PICO_CONFIG_HEADER_FILES before generation — we hardcode that same board
    // (the default, "pico") directly instead.
    std::ofstream config(out_dir / "config_autogen.h");
    config << "// AUTOGENERATED — DO NOT EDIT\n\n#include \"boards/pico.h\"\n";
}

// Every component publishes its own include/ subdir (e.g.
// src/rp2_common/hardware_gpio/include) — walk the in-scope trees once, adding each
// as an include path and collecting every .c/.S outside of include/ dirs (headers
// there are never compiled standalone, even ones with a .S-like double extension
// such as hardware_dcp's *.inc.S snippets, which are meant to be #included from a
// real .S file, not compiled directly).
void collectIncludesAndSources(BuildGroup& group, std::vector<std::filesystem::path>& sources) {
    for (const auto& root : kSourceRoots) {
        for (auto it = std::filesystem::recursive_directory_iterator(root); it != std::filesystem::recursive_directory_iterator(); ++it) {
            const auto& entry = *it;

            if (entry.is_directory()) {
                if (isExcludedDir(entry.path())) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (entry.path().filename() == "include") {
                    group.addInclude(Include<Direct>(entry.path()));
                    it.disable_recursion_pending();
                }
                continue;
            }

            if (entry.is_regular_file()) {
                const auto& ext = entry.path().extension();
                if ((ext == ".c" || ext == ".S") && !isExcludedFile(entry.path())) {
                    sources.push_back(entry.path());
                }
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    initLog(4096);

    Build build(".build", "clang++", argc, argv);
    build.defineArg<std::string>("link");
    build.parseArgs();

    std::string link_type = build.arg<std::string>("link").value_or("static");
    Linkage      linkage  = (link_type == "shared") ? Linkage::Shared : Linkage::Static;

    generateHeaders(build.buildDir());

    BuildGroup& sdk = build.addGroup();
    // NOT clang++: pico-sdk's C sources use `restrict` and other C-only idioms that
    // break under clang++'s forced C++ mode. Matches upstream exactly — pico-sdk's own
    // clang toolchain file uses a plain `clang` binary for C and ASM, `clang++` only
    // for the (out-of-scope) C++ files. build.hpp's selfRebuild() always compiles this
    // very file with the Build's own top-level compiler, which is why that one stays
    // "clang++" while this group overrides to "clang".
    sdk.setCompiler("clang");

    // From cmake/preload/toolchains/pico_arm_cortex_m0plus_clang.cmake.
    sdk.addCompileFlag("--target=armv6m-none-eabi");
    sdk.addCompileFlag("-mfloat-abi=soft");
    sdk.addCompileFlag("-march=armv6m");
    // ARM GNU Toolchain (newlib) sysroot found on this machine — see plan verification
    // notes. Xcode's bundled clang has no bare-metal ARM sysroot of its own.
    sdk.addCompileFlag("--sysroot=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/arm-none-eabi");

    // Needed again at link time (only exercised by --link shared): without --target,
    // clang's link step defaults to the host (macOS) linker, which can't parse ARM ELF
    // .o files at all ("unknown file type").
    sdk.addLinkFlag("--target=armv6m-none-eabi");
    sdk.addLinkFlag("--sysroot=/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/arm-none-eabi");
    sdk.addLinkFlag("-fuse-ld=lld");

    // From src/rp2040/pico_platform/CMakeLists.txt's target_compile_definitions —
    // normally propagated transitively via CMake's INTERFACE target graph; flattened
    // here since this build is single-platform (RP2040 only), not modeling that graph.
    sdk.addCompileFlag("-DPICO_NO_HARDWARE=0");
    sdk.addCompileFlag("-DPICO_ON_DEVICE=1");
    sdk.addCompileFlag("-DPICO_BUILD=1");
    sdk.addCompileFlag("-DPICO_RP2040=1");
    sdk.addCompileFlag("-DPICO_32BIT=1");
    // Command::exec() runs through a shell (popen), which would otherwise strip the
    // quotes around "pico" before clang ever sees them — the extra backslashes here
    // survive shell parsing as literal `"` characters, so clang receives
    // -DPICO_BOARD="pico" (a string literal), not -DPICO_BOARD=pico (a bare identifier).
    sdk.addCompileFlag("-DPICO_BOARD=\\\"pico\\\"");
    // Default implementation for pico_thread_local (PICO_DEFAULT_THREAD_LOCAL_IMPL,
    // default "per_thread") — same file compiles under all three variants, gated by
    // this define rather than by file selection.
    sdk.addCompileFlag("-DPICO_THREAD_LOCAL_MODE_PER_THREAD=1");

    // Include dirs that don't follow the plain-"include/"-subdir convention.
    sdk.addInclude(Include<Direct>("src/boards/include"));
    sdk.addInclude(Include<Direct>("src/rp2_common/cmsis/stub/CMSIS/Core/Include"));
    sdk.addInclude(Include<Direct>("src/rp2_common/cmsis/stub/CMSIS/Device/RP2040/Include"));
    sdk.addInclude(Include<Direct>(build.buildDir() / "generated" / "pico_base"));

    std::vector<std::filesystem::path> sources;
    collectIncludesAndSources(sdk, sources);

    Task& lib_task = sdk.addTask(Library("pico_sdk_rp2040", linkage));
    for (const auto& source : sources) {
        lib_task.depends_on(sdk.addTask(Object(source)));
    }

    build.build();

    return 0;
}
