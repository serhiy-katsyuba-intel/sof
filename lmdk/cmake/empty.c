/* Loadable library executable is built just by linking static libs (modules) with no need
 * to compile any sources (modules static libs are compiled from source file). Unfortunately,
 * CMake's add_executable() requires at list one source file to be specified. Hence, this
 * empty dummy source file is used.
 */
