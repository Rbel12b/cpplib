#include "ProcessUtils.hpp"
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <thread>

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

#ifdef _WIN32
    fd_streambuf::fd_streambuf(HANDLE h, bool read_mode)
        : buffer(buf_size), handle(h), readable(read_mode)
    {
        if (readable)
            setg(buffer.data(), buffer.data(), buffer.data());
        else
            setp(buffer.data(), buffer.data() + buffer.size());
    }
#else
    fd_streambuf::fd_streambuf(int f, bool read_mode)
        : buffer(buf_size), fd(f), readable(read_mode)
    {
        if (readable)
            setg(buffer.data(), buffer.data(), buffer.data());
        else
            setp(buffer.data(), buffer.data() + buffer.size());
    }
#endif

    int fd_streambuf::sync()
    {
        if (!readable)
            return overflow(EOF) == EOF ? -1 : 0;
        return 0;
    }

    int fd_streambuf::overflow(int ch)
    {
        if (!readable)
        {
            size_t n = pptr() - pbase();
#ifdef _WIN32
            DWORD written = 0;
            if (n > 0 && !WriteFile(handle, buffer.data(), (DWORD)n, &written, nullptr))
                return EOF;
#else
            ssize_t written = ::write(fd, buffer.data(), n);
            if (written < 0)
                return EOF;
#endif
            setp(buffer.data(), buffer.data() + buffer.size());
            if (ch != EOF)
                sputc(ch);
            return 0;
        }
        return EOF;
    }

    int fd_streambuf::underflow()
    {
        if (readable)
        {
#ifdef _WIN32
            DWORD read = 0;
            if (!ReadFile(handle, buffer.data(), (DWORD)buffer.size(), &read, nullptr) || read == 0)
                return EOF;
#else
            ssize_t read = ::read(fd, buffer.data(), buffer.size());
            if (read <= 0)
                return EOF;
#endif
            setg(buffer.data(), buffer.data(), buffer.data() + read);
            return (unsigned char)*gptr();
        }
        return EOF;
    }

    Process::~Process()
    {
        if (m_monitorThread.joinable())
        {
            m_monitorThread.join();
        }

        if (m_stdoutBuf)
        {
            m_stdoutBuf->sync();
            out.rdbuf(nullptr);
            delete m_stdoutBuf;
            m_stdoutBuf = nullptr;
        }
        if (m_stderrBuf)
        {
            m_stderrBuf->sync();
            err.rdbuf(nullptr);
            delete m_stderrBuf;
            m_stderrBuf = nullptr;
        }
        if (m_stdinBuf)
        {
            m_stdinBuf->sync();
            in.rdbuf(nullptr);
            delete m_stdinBuf;
            m_stdinBuf = nullptr;
        }

        if (m_running && !m_detached)
        {
            waitForExit();
        }

        if (m_stdOutPipeOpen[0])
        {
            close(m_stdOutPipe[0]);
            m_stdOutPipeOpen[0] = false;
        }
        if (m_stdOutPipeOpen[1])
        {
            close(m_stdOutPipe[1]);
            m_stdOutPipeOpen[1] = false;
        }
    }

    int Process::start()
    {
        std::vector<std::string> argv_vec;
        argv_vec.push_back(m_exePath.string());
        argv_vec.insert(argv_vec.end(), m_arguments.begin(), m_arguments.end());

        char *const *argv = buildArgvArray(argv_vec);
        char *const *envp = buildArgvArray(m_environment);

        if (!m_detached)
        {
            if (pipe(m_stdOutPipe) == -1)
            {
                freeArgvArray(argv);
                freeArgvArray(envp);
                throw std::runtime_error("pipe() failed");
                return -1;
            }
            m_stdOutPipeOpen[0] = true;
            m_stdOutPipeOpen[1] = true;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            freeArgvArray(argv);
            freeArgvArray(envp);
            if (m_stdOutPipeOpen[0])
            {
                close(m_stdOutPipe[0]);
                m_stdOutPipeOpen[0] = false;
            }
            if (m_stdOutPipeOpen[1])
            {
                close(m_stdOutPipe[1]);
                m_stdOutPipeOpen[1] = false;
            }
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
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    if (fd > 2)
                        close(fd);
                }
            }
            else
            {
                // Redirect stdout to pipe
                if (m_stdOutPipeOpen[0])
                {
                    close(m_stdOutPipe[0]);
                    m_stdOutPipeOpen[0] = false;
                }
                dup2(m_stdOutPipe[1], STDOUT_FILENO);
                dup2(m_stdOutPipe[1], STDERR_FILENO);
            }

            if (!m_workingDirectory.empty())
            {
                chdir(m_workingDirectory.c_str());
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

        if (m_stdOutPipeOpen[1])
        {
            close(m_stdOutPipe[1]);
            m_stdOutPipeOpen[1] = false;
        }

        // Parent process
        freeArgvArray(argv);
        freeArgvArray(envp);

        if (m_stdoutBuf)
            delete m_stdoutBuf;
        if (m_stderrBuf)
            delete m_stderrBuf;
        if (m_stdinBuf)
            delete m_stdinBuf;

        m_stdoutBuf = new fd_streambuf(m_stdOutPipe[0], true);
        out.rdbuf(m_stdoutBuf);

        m_pid = pid;
        m_running = true;

        m_monitorThread = std::thread(std::bind(&Process::monitorProcess, this));

        return 0;
    }

    int Process::run()
    {
        if (start())
        {
            return -1;
        }

        int pid = m_pid;

        if (pid == -1)
        {
            return -1;
        }

        if (m_detached)
        {
            // In detached mode, do not wait for the child
            return 0;
        }
        if (m_outputCallback)
        {
            m_stdOutPipeOpen[1] = false;
            std::array<char, 4096> buffer;
            ssize_t n;
            std::string lineBuffer;
            while ((n = read(m_stdOutPipe[0], buffer.data(), buffer.size() - 1)) > 0)
            {
                buffer[n] = '\0';
                lineBuffer += buffer.data();

                size_t pos = 0;
                while ((pos = lineBuffer.find('\n')) != std::string::npos)
                {
                    std::string line = lineBuffer.substr(0, pos + 1);
                    if (m_outputCallback)
                    {
                        m_outputCallback(line);
                    }
                    lineBuffer.erase(0, pos + 1);
                }
            }

            if (!lineBuffer.empty())
            {
                if (m_outputCallback)
                {
                    m_outputCallback(lineBuffer);
                }
            }
        }

        waitForExit();

        return 0;
    }

    int Process::waitForExit()
    {
        if (m_detached)
        {
            throw std::runtime_error("Cannot wait for exit of detached process");
            return -1;
        }
        if (m_pid == -1)
        {
            throw std::runtime_error("Process not started");
            return -1;
        }

        if (m_stdoutBuf)
        {
            m_stdoutBuf->sync();
        }
        if (m_stderrBuf)
        {
            m_stderrBuf->sync();
        }
        if (m_stdinBuf)
        {
            m_stdinBuf->sync();
        }

        if (m_stdOutPipeOpen[0])
        {
            close(m_stdOutPipe[0]);
            m_stdOutPipeOpen[0] = false;
        }
        if (m_stdOutPipeOpen[1])
        {
            close(m_stdOutPipe[1]);
            m_stdOutPipeOpen[1] = false;
        }

        if (!m_running)
        {
            return m_exitCode;
        }

        int status = 0;
        waitpid(m_pid, &status, 0);
        m_running = false;

        if (WIFEXITED(status))
            m_exitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
            m_exitCode = 128 + WTERMSIG(status);
        return m_exitCode;
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

    void Process::monitorProcess()
    {
        int status;
        while (m_running)
        {
            pid_t result = waitpid(m_pid, &status, WNOHANG);
            if (result == 0)
            {
                // Still running
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else if (result == m_pid)
            {
                if (WIFEXITED(status))
                    m_exitCode = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                    m_exitCode = 128 + WTERMSIG(status);
                onProcessExit();
                break;
            }
            else
            {
                perror("waitpid failed");
                break;
            }
        }
    }

    void Process::onProcessExit()
    {
        m_running = false;
        waitForExit();
    }
}; // namespace cpplib