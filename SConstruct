import platform

##############################################################################
# source and output data
##############################################################################

# source cpp files. relative to src folder
src = [
    "imagepack.cpp",
    "image_io.cpp",
    "output.cpp",
    "cmdline.cpp",
]

# source help text file
cmd_help_src = "cmd_help"

# libraries
linux_libs        = ["freeimage", "boost_program_options", "boost_filesystem"]

windows_libs      = ["freeimage"]
windows_cpp_paths = ["C:/Program Files/boost/boost_1_47/", "FreeImage"]
windows_lib_paths = ["C:/Program Files/boost/boost_1_47/lib/", "FreeImage"]

# what to save the output binary as. the binary is written to root directory.
binary_name = "imagepack"

# where to put the temporary build files
build_dirs = {
    "debug"   : "build_debug",
    "release" : "build_release",
}

os = platform.system()


##############################################################################
# Command Line
##############################################################################
AddOption("--build", dest="build_type", metavar="[debug|release]", type="choice", choices=["debug", "release"], default="release", help="The type of build.")

build_type = GetOption("build_type")
build_dir  = build_dirs[build_type]


##############################################################################
# Build Environment
##############################################################################
VariantDir(build_dir, "src")

cpp_flags     = []
cpp_linkflags = []
cpp_defines   = []
cpp_path      = []


if os == "Linux":
    libs = linux_libs
    cpp_flags.append(["-Wall", "-Wextra"])
    cpp_defines.append("IMAGEPACK_BUILD_HELP")

    if build_type == "debug":
        cpp_flags.append(["-g"])
    elif build_type == "release":
        cpp_defines.append("NDEBUG")
        cpp_flags.append("-O3")

    env = Environment(
        CCFLAGS     = cpp_flags,
        CPPDEFINES  = cpp_defines,
        LINKFLAGS   = cpp_linkflags,
    )

elif os == "Windows":
    libs = windows_libs
    cpp_defines += ["IMAGEPACK_BUILD_HELP", ("_ITERATOR_DEBUG_LEVEL", 0), "NOMINMAX"]

    if build_type == "debug":
        print("windows debug build not supported")
        Exit(-1)
    elif build_type == "release":
        cpp_defines.append("NDEBUG")
        cpp_flags.append(["-O2", "-MT", "-EHsc"])

    env = Environment(
        CCFLAGS     = cpp_flags,
        CPPDEFINES  = cpp_defines,
        LINKFLAGS   = cpp_linkflags,
        CPPPATH     = windows_cpp_paths,
        LIBPATH     = windows_lib_paths,
    )

else:
    print("os not supported")
    Exit(-1)


##############################################################################
# Help file generation
##############################################################################
def writeHelpHeader(target, source, env):
    out = open(str(target[0]), "w")
    out.write('#ifndef CMD_HELP_H\n')
    out.write('#define CMD_HELP_H\n')
    out.write('static const char *cmd_help =\n')
    for line in open(str(source[0])):
        line = line.replace('"', '\\"').replace('\n', '')
        out.write('"' + line + '\\n"\n')
    out.write(';\n')
    out.write('#endif\n')
    out.close()

env.Append(BUILDERS = {
    "HelpGenerator" : Builder(action=writeHelpHeader, suffix=".h")
})


##############################################################################
# Building
##############################################################################

cmd_help = env.HelpGenerator(cmd_help_src, srcdir=build_dir)
objs = env.Object(src, srcdir=build_dir)
prog = env.Program(binary_name, objs, LIBS=libs, srcdir=build_dir)

Depends(objs, cmd_help)

