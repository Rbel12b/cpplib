#include "Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>

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
        char *const *envp = buildArgvArray(m_environment);

        int pipefd[2];

        if (m_captureOutput && m_detached)
        {
            freeArgvArray(argv);
            freeArgvArray(envp);
            throw std::runtime_error("Cannot capture output in detached mode");
            return -1;
        }
        else if (m_captureOutput)
        {
            if (pipe(pipefd) == -1)
            {
                freeArgvArray(argv);
                freeArgvArray(envp);
                throw std::runtime_error("pipe() failed");
                return -1;
            }
        }
        
        pid_t pid = fork();
        if (pid == -1)
        {
            freeArgvArray(argv);
            freeArgvArray(envp);
            if (m_captureOutput)
                close(pipefd[0]), close(pipefd[1]);
            throw std::runtime_error("fork() failed");
            return -1;
        }
        else if (pid == 0)
        {
            // Child process
            if (m_detached)
            {
                // Detach from controlling terminal / session
                setsid();
                // Redirect stdin/out/err to /dev/null to avoid tying to parent's fds.
                int fd = open("/dev/null", O_RDWR);
                if (fd != -1)
                {
                    dup2(fd, STDIN_FILENO);
                    // Only redirect stdout/stderr if not echoing output
                    if (!m_echoOutput)
                    {
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                    }
                    if (fd > 2)
                        close(fd);
                }
            }

            if (m_hasCustomEnvironment)
                execve(argv[0], argv, envp);
            else
                execvp(argv[0], argv);

            // If execvp returns, it failed — clean up before exiting
            freeArgvArray(argv);
            freeArgvArray(envp);
            _exit(127);
        }

        // Parent process
        freeArgvArray(argv);
        freeArgvArray(envp);

        if (m_detached)
        {
            // In detached mode, do not wait for the child
            return 0;
        }
        if (m_captureOutput)
        {
            close(pipefd[1]);
            std::array<char, 4096> buffer;
            ssize_t n;
            std::string lineBuffer;
            while ((n = read(pipefd[0], buffer.data(), buffer.size() - 1)) > 0)
            {
                buffer[n] = '\0';
                lineBuffer += buffer.data();

                size_t pos = 0;
                while ((pos = lineBuffer.find('\n')) != std::string::npos)
                {
                    std::string line = lineBuffer.substr(0, pos + 1);
                    if (m_echoOutput)
                    {
                        std::cout << line;
                    }
                    if (m_outputCallback)
                    {
                        m_outputCallback(line);
                    }
                    else
                    {
                        m_capturedOutput += line;
                    }
                    lineBuffer.erase(0, pos + 1);
                }
            }
            close(pipefd[0]);

            if (!lineBuffer.empty())
            {
                if (m_echoOutput)
                {
                    std::cout << lineBuffer;
                }
                if (m_outputCallback)
                {
                    m_outputCallback(lineBuffer);
                }
                else
                {
                    m_capturedOutput += lineBuffer;
                }
            }
        }

        int status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status))
            m_exitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            m_exitCode = 128 + WTERMSIG(status);

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