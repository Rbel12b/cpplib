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

#define CLOSE_PIPE(pipe, end) \
    closePipe(m_std ## pipe ## Pipe, m_std ## pipe ## PipeOpen, end);

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

    size_t fd_streambuf::available() const {
        return egptr() - gptr(); // number of bytes currently buffered
    }
#ifdef _WIN32
    bool fd_streambuf::hasData() const {
        DWORD available = 0;
        return PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) && available > 0;
    }
#else
    bool fd_streambuf::hasData() const {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        timeval tv {0, 0}; // zero timeout = non-blocking
        return select(fd+1, &readfds, nullptr, nullptr, &tv) > 0;
    }
#endif

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

        closePipes();
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

            if (pipe(m_stdErrPipe) == -1)
            {
                freeArgvArray(argv);
                freeArgvArray(envp);
                throw std::runtime_error("pipe() failed");
                return -1;
            }
            m_stdErrPipeOpen[0] = true;
            m_stdErrPipeOpen[1] = true;

            if (pipe(m_stdInPipe) == -1)
            {
                freeArgvArray(argv);
                freeArgvArray(envp);
                throw std::runtime_error("pipe() failed");
                return -1;
            }
            m_stdInPipeOpen[0] = true;
            m_stdInPipeOpen[1] = true;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            freeArgvArray(argv);
            freeArgvArray(envp);
            closePipes();
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
                CLOSE_PIPE(Out, 0);
                CLOSE_PIPE(Err, 0);
                CLOSE_PIPE(In, 1);
                dup2(m_stdOutPipe[1], STDOUT_FILENO);
                dup2(m_stdErrPipe[1], STDERR_FILENO);
                dup2(m_stdInPipe[0], STDIN_FILENO);
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

        CLOSE_PIPE(Out, 1);
        CLOSE_PIPE(Err, 1);
        CLOSE_PIPE(In, 0);

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

        m_stderrBuf = new fd_streambuf(m_stdErrPipe[0], true);
        err.rdbuf(m_stderrBuf);

        m_stdinBuf = new fd_streambuf(m_stdInPipe[1], false);
        in.rdbuf(m_stdinBuf);

        m_pid = pid;
        m_running = true;

        m_monitorThread = std::thread(std::bind(&Process::monitorProcess, this));

        if (m_outputCallback)
        {
            std::thread outputThread([this]() {
                std::string line;
                while (std::getline(out, line)) {
                    m_outputCallback(line);
                }
            });
            outputThread.detach();
        }

        if (m_errorCallback)
        {
            std::thread errorThread([this]() {
                std::string line;
                while (std::getline(err, line)) {
                    m_errorCallback(line);
                }
            });
            errorThread.detach();
        }

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

        waitForExit();

        return 0;
    }

    int Process::waitForExit()
    {
        if (m_detached)
        {
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

        closePipes();

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

    void Process::closePipes()
    {
        closePipe(m_stdOutPipe, m_stdOutPipeOpen);
        closePipe(m_stdErrPipe, m_stdErrPipeOpen);
        closePipe(m_stdInPipe, m_stdInPipeOpen);
    }

    void Process::closePipe(int pipeFd[2], bool openFlags[2], int endsToClose)
    {
        if (endsToClose == 3)
        {
            if (openFlags[0])
            {
                close(pipeFd[0]);
                openFlags[0] = false;
            }
            if (openFlags[1])
            {
                close(pipeFd[1]);
                openFlags[1] = false;
            }
        }
        else
        {
            if ((endsToClose == 0) && openFlags[0])
            {
                close(pipeFd[0]);
                openFlags[0] = false;
            }
            if ((endsToClose == 1) && openFlags[1])
            {
                close(pipeFd[1]);
                openFlags[1] = false;
            }
        }
    }
}; // namespace cpplib