project('ps5-app', ['c', 'cpp'],
    default_options: {
        'default_library': 'static',
        'prefer_static':    true
    },
    meson_version: '>=1.2.0',
    version: '1.0'
)
cpp = meson.get_compiler('cpp')

# Define your app's serial below:
app_serial = 'PPSA99999'

# External static libraries
ps5_lib_dirs = [
    meson.project_source_root() + '/deps/ps5-payload-sdk/target/lib',
    meson.project_source_root() + '/deps/ps5-payload-sdk/target/user/homebrew/lib',
    meson.project_source_root() + '/deps/lib',
]

ps5_includes = include_directories(
    'deps/ps5-payload-sdk/target/user/homebrew/include',
    'deps/include',
    'include'
)

ps5app_libs = []

# Compile args
ps5app_shared_args = [
    '-g'
]

ps5app_c_args = ps5app_shared_args + [

]

ps5app_cpp_args = ps5app_shared_args + [

]

add_project_arguments(ps5app_c_args,
    language: 'c',
    native: false
)
add_project_arguments(ps5app_cpp_args,
    language: 'cpp',
    native: false
)

# PS5 App C Runtime & native tools
subdir('agc_stubs')
subdir('crt')
subdir('tools')

# App code: define your source files in src/meson.build
subdir('src')

# These arguments are required to make an .ELF file that the PS5 will understand
elf_link_args = [
    '-nostartfiles',
    '-T' + meson.global_source_root() + '/' + 'ps5-elf.ld',
    '-e',
    '_start',
    '-Wl,--version-script=' + meson.global_source_root() + '/' + 'ps5-app-symbols.map',
    '-pthread'
]

# Add your own link arguments here
ps5app_link_args = [
]

ps5app_external_libs = declare_dependency(
    dependencies: [
        # SCE Core libraries
        cpp.find_library('kernel', dirs: ps5_lib_dirs),
        cpp.find_library('SceAudioOut', dirs: ps5_lib_dirs),
        cpp.find_library('SceImeDialog', dirs: ps5_lib_dirs),
        cpp.find_library('SceKeyboard', dirs: ps5_lib_dirs),
        cpp.find_library('ScePad', dirs: ps5_lib_dirs),
        cpp.find_library('SceSystemService', dirs: ps5_lib_dirs),
        cpp.find_library('SceUserService', dirs: ps5_lib_dirs),
        cpp.find_library('SceVideoOut', dirs: ps5_lib_dirs),
        cpp.find_library('SceAgc', dirs: ps5_lib_dirs),
        cpp.find_library('SceAgcDriver', dirs: ps5_lib_dirs),

        # OpenGL libraries
        cpp.find_library('blake3', dirs: ps5_lib_dirs),
        cpp.find_library('compiler', dirs: ps5_lib_dirs),
        cpp.find_library('gallium', dirs: ps5_lib_dirs),
        cpp.find_library('glapi_bridge', dirs: ps5_lib_dirs),
        cpp.find_library('glapi', dirs: ps5_lib_dirs),
        cpp.find_library('glcpp', dirs: ps5_lib_dirs),
        cpp.find_library('glsl_util', dirs: ps5_lib_dirs),
        cpp.find_library('glsl', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_util_c11', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_util_simd', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_util_clflush', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_util_clflushopt', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_util', dirs: ps5_lib_dirs),
        cpp.find_library('mesa', dirs: ps5_lib_dirs),
        cpp.find_library('mesa_sse41', dirs: ps5_lib_dirs),
        cpp.find_library('nir', dirs: ps5_lib_dirs),
        cpp.find_library('ps5_opengl_core33', dirs: ps5_lib_dirs),
        cpp.find_library('psbc.ps5', dirs: ps5_lib_dirs),
        cpp.find_library('softpipe', dirs: ps5_lib_dirs),
        cpp.find_library('vtn', dirs: ps5_lib_dirs),

        # Add your own external libraries here. SDL2 is provided as an example
        cpp.find_library('SDL2', dirs: ps5_lib_dirs),
    ]
)

# Link ELF file (PIE)
ps5app_elf = executable(
    'ps5-app.elf',
    sources: ps5app_srcs,
    include_directories: ps5_includes,
    dependencies: ps5app_external_libs,
    link_args: [elf_link_args, ps5app_link_args],
    link_with: ps5app_libs,
    link_whole: crt_lib,
    native: false
)

# Convert elf file to PS5 ELF
eboot_elf = custom_target(
    'eboot.elf',
    output: 'eboot.elf',
    depends: [
        ps5app_elf, 
        sce_agc_lib, 
        sce_agc_driver_lib
    ],
    command: [
        ps5_native_tool_exe,
        'link',
        '--in',
        ps5app_elf,
        '--out',
        '@OUTPUT@',
        '--stub-dir',
        meson.global_source_root() + '/deps/ps5-payload-sdk/target/lib',
        '--stub',
        sce_agc_lib,
        '--stub',
        sce_agc_driver_lib,
        '--module-sdk',
        '0x02000009',
        '--companion_sdk',
        '0x08050001',
        '--file-name',
        'eboot.elf'
    ]
)

# Sign eboot.elf to eboot.bin
eboot_bin = custom_target(
    'eboot.bin',
    output: 'eboot.bin',
    depends: [eboot_elf],
    command: [ ps5_native_tool_exe, 
               'self',
               '--sign',
               '--in',
               eboot_elf,
               '--out',
               '@OUTPUT@' ]
)

# Generate libc.prx stub (required by PS5 loader)
libc_prx = custom_target(
    'libc.prx',
    output: 'libc.prx',
    depends: [libc_elf],
    command: [ ps5_native_tool_exe, 
               'self',
               '--sign',
               '--in',
               libc_elf,
               '--out',
               '@OUTPUT@' ]
)

# Build the app folder
build_app_folder_sh = files('scripts/build_app_folder.sh')
sce_sys_source = meson.project_source_root() + '/sce_sys'
app_folder = custom_target(
    app_serial,
    output: app_serial,
    build_by_default: true,
    depends: [libc_prx, eboot_bin],
    command: [
        build_app_folder_sh,
        meson.project_source_root(),
        eboot_bin,
        libc_prx,
        sce_sys_source,
        '@OUTPUT@'
    ]
)
