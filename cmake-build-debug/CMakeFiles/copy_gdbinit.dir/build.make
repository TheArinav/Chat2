# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /tmp/tmp.AhEH0UNdug

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /tmp/tmp.AhEH0UNdug/cmake-build-debug

# Utility rule file for copy_gdbinit.

# Include any custom commands dependencies for this target.
include CMakeFiles/copy_gdbinit.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/copy_gdbinit.dir/progress.make

CMakeFiles/copy_gdbinit: .gdbinit

copy_gdbinit: CMakeFiles/copy_gdbinit
copy_gdbinit: CMakeFiles/copy_gdbinit.dir/build.make
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Copying .gdbinit to home directory"
	/usr/local/bin/cmake -E copy /tmp/tmp.AhEH0UNdug/cmake-build-debug/.gdbinit /home/ubuntu/.gdbinit
.PHONY : copy_gdbinit

# Rule to build all files generated by this target.
CMakeFiles/copy_gdbinit.dir/build: copy_gdbinit
.PHONY : CMakeFiles/copy_gdbinit.dir/build

CMakeFiles/copy_gdbinit.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/copy_gdbinit.dir/cmake_clean.cmake
.PHONY : CMakeFiles/copy_gdbinit.dir/clean

CMakeFiles/copy_gdbinit.dir/depend:
	cd /tmp/tmp.AhEH0UNdug/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /tmp/tmp.AhEH0UNdug /tmp/tmp.AhEH0UNdug /tmp/tmp.AhEH0UNdug/cmake-build-debug /tmp/tmp.AhEH0UNdug/cmake-build-debug /tmp/tmp.AhEH0UNdug/cmake-build-debug/CMakeFiles/copy_gdbinit.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/copy_gdbinit.dir/depend

