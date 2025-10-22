#include "Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <limits.h>
#include <fcntl.h>
#endif

namespace cpplib
{
    int Process::runCommand()
    {
        std::vector<std::string> argv_vec;
        argv_vec.push_back(m_exePath.string());
        argv_vec.insert(argv_vec.end(), m_arguments.begin(), m_arguments.end());

        char *const *argv = buildArgvArray(argv_vec);
        
        pid_t pid = fork();
        if (pid == -1)
        {
            freeArgvArray(argv);
            return -1;
        }
        else if (pid == 0)
        {
            // Child process
            execvp(argv[0], argv);

            // If execvp returns, it failed — clean up before exiting
            freeArgvArray(argv);
            _exit(127);
        }

        // Parent process
        freeArgvArray(argv);
        return 0;
    }

    std::string Process::buildCommandLine() const
    {
        std::string cmdLine;
        cmdLine += m_exePath.string();
        for (const auto &arg : m_arguments)
        {
            cmdLine += " " + arg;
        }
        return cmdLine;
    }

    std::vector<std::string> Process::buildArgv(const std::string &cmd) const
    {
        std::vector<std::string> out;
        size_t pos = 0, start = 0;
        while ((pos = cmd.find(' ', start)) != std::string::npos)
        {
            if (pos > start)
                out.push_back(cmd.substr(start, pos - start));
            start = pos + 1;
        }
        if (start < cmd.size())
            out.push_back(cmd.substr(start));
        return out;
    }

    char *const *Process::buildArgvArray(const std::vector<std::string> &argv) const
    {
        // Allocate array of char* (size + 1 for null terminator)
        char **argArray = new char *[argv.size() + 1];

        for (size_t i = 0; i < argv.size(); ++i)
        {
            // Duplicate string data (so lifetime is independent of std::string)
            argArray[i] = strdup(argv[i].c_str());
        }

        argArray[argv.size()] = nullptr; // NULL terminator
        return argArray;
    }

    void Process::freeArgvArray(char *const *argv) const
    {
        if (!argv)
            return;

        for (size_t i = 0; argv[i] != nullptr; ++i)
        {
            free(const_cast<char *>(argv[i]));
        }
        delete[] argv;
    }

}; // namespace cpplib