##############################################################################
# source and output data
##############################################################################
src = [
    "imagepack.cpp",
    "cmdline.cpp",
]

cmd_help_src = "cmd_help"

libs = ["freeimage", "boost_program_options", "boost_filesystem"]

binary_name = "imagepack"

build_dirs = {
    "debug"   : "build_debug",
    "release" : "build_release",
}


##############################################################################
# Command Line
##############################################################################
AddOption("--build", dest="build_type", metavar="[debug|release]", type="choice", choices=["debug", "release"], default="debug", help="the type of build")
build_type = GetOption("build_type")
build_dir  = build_dirs[build_type]


##############################################################################
# Build Environment
##############################################################################
VariantDir(build_dir, "src")

cpp_flags     = ["-Wall", "-Wextra"]
cpp_linkflags = []
cpp_defines   = []

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

env.Append(BUILDERS = {
    "HelpGenerator" : Builder(action="xxd -i ${SOURCE.file} ${TARGET.file}", suffix=".h")
})


##############################################################################
# Building
##############################################################################

cmd_help = env.HelpGenerator(cmd_help_src, chdir=1, srcdir=build_dir)
prog     = env.Program(binary_name, src, LIBS=libs, srcdir=build_dir)

