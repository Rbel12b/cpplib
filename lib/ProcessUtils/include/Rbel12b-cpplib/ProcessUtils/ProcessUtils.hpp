#pragma once
#include <string>

namespace ProcessUtils
{
    /**
     * runs a command without a terminal
     * @param cmd Command to run
     * @return Exit code of the command
     */
    int runCommand(const std::string &cmd);

};