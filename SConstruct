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

# dynamic libraries
libs = ["freeimage", "boost_program_options", "boost_filesystem"]

# what to save the output binary as. the binary is written to root directory.
binary_name = "imagepack"

# where to put the temporary build files
build_dirs = {
    "debug"   : "build_debug",
    "release" : "build_release",
}


##############################################################################
# Command Line
##############################################################################
AddOption("--build", dest="build_type", metavar="[debug|release]", type="choice", choices=["debug", "release"], default="debug", help="The type of build.")
AddOption("--no-help",dest="no-help", action="store_true", default=False, help="Set to not generate and include the help text that gets printed to the console. Set this if xxd is not installed or not in the environment's path.")

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

if GetOption("no-help"):
    cpp_defines.append("NO_HELP")

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

if not GetOption("no-help"):
    cmd_help = env.HelpGenerator(cmd_help_src, chdir=1, srcdir=build_dir)

prog = env.Program(binary_name, src, LIBS=libs, srcdir=build_dir)

