# ProcessUtils

ProcessUtils is a utility module of the cpplib collection that provides cross‐platform (Windows / Linux) process--related helpers. It is designed to simplify common tasks such as launching external processes, capturing output.

## Features

- Launch an external process with specified arguments and environment.
- Capture standard output and/or standard error of the spawned process.
- Wait for process termination, retrieve exit code.
- Easy to integrate via CMake as part of your application or library.

## Structure

```text
lib/ProcessUtils/
  include/Rbel12b-cpplib/ProcessUtils/   ← public headers
    ProcessUtils.hpp
  src/                                     ← implementation files
  CMakeLists.txt                            ← module’s CMake entry
```

Including ProcessUtils in your project

```cpp
#include <Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp>
```

## Installing and Linking

#### Install cpplib with cmake

```bash
git clone https://github.com/Rbel12b/cpplib.git
cd cpplib
mkdir build && cd build
cmake ..
cmake --build . --target install
```

then link ProcessUtils in your project’s CMakeLists.txt

```cmake
find_package(Rbel12b-cpplib REQUIRED COMPONENTS ProcessUtils)
target_link_libraries(your_target PRIVATE Rbel12b-cpplib::ProcessUtils)
```

#### Or add cpplib as a subdirectory in your project’s CMakeLists.txt

```cmake
add_subdirectory(path/to/cpplib)
target_link_libraries(your_target PRIVATE Rbel12b-cpplib::ProcessUtils)
```

## Usage Examples

Launching a process and capturing output

```cpp
#include <Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp>

cpplib::Process proc;
proc.setCommand(std::filesystem::path("myExecutable"));
proc.appendArguments({"--arg1", "value"});

proc.start();

std::string outPutLine;
while (std::getline(proc.out, &outPutLine)) {
    std::cout << "Process output: " << outPutLine << std::endl;
}
proc.wait(); // blocking wait for process to finish, unnescessary since output reading already blocks until EOF
int exitCode = proc.getExitCode();
std::cout << "Process exited with code: " << exitCode << std::endl;
if (exitCode != 0) {
    std::cerr << "stderr:" << std::endl;
    while (std::getline(proc.err, &outPutLine)) {
        std::cout << "Error: " << outPutLine << std::endl;
    }
}
```

running a process without capturing output

```cpp
#include <Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp>

cpplib::Process proc;
proc.setCommand(std::filesystem::path("myExecutable"));
proc.appendArguments({"--arg1", "value"});
proc.run(); // blocking
std::cout << "Process exited with code: " << proc.getExitCode() << std::endl;
```

## Supported Platforms

Windows (tested on MinGW)

Linux (GCC/Clang tested on Arch with gcc)

## License

This module is covered under the same license as the overall cpplib project (AGPLv3).

## Contribution

Contributions, bug‐reports and enhancements are very welcome!
Please follow the general contribution guidelines of cpplib.
When submitting pull requests for ProcessUtils, please include:

A clear description of the feature / bug

Cross‐platform verification (Windows + Linux if applicable)

Documentation updates (this README and header comments)
