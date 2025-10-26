#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <streambuf>
#include <vector>
#include <thread>

namespace cpplib
{
    class fd_streambuf : public std::streambuf
    {
        static const size_t buf_size = 4096;
        std::vector<char> buffer;
#ifdef _WIN32
        HANDLE handle;
#else
        int fd;
#endif
        bool readable;

    public:
#ifdef _WIN32
        fd_streambuf(HANDLE h, bool read_mode);
#else
        fd_streambuf(int f, bool read_mode);
#endif

        int sync() override;
        int overflow(int ch) override;
        int underflow() override;
        size_t available() const;
        bool hasData() const;
    };
    class Process
    {
    public:
        using OutputLineCallback = std::function<void(const std::string &)>;

        ~Process();

        inline void setCommand(const std::filesystem::path &exePath)
        {
            m_exePath = exePath;
        }

        /**
         * Sets the command to execute by parsing a command line string, note that
         * the parser splits arguments by spaces and does not handle complex shell features. (eg. quoting ...)
         * @param cmd The command line string to parse.
         */
        inline void setCommand(const std::string &cmd)
        {
            auto argv = buildArgv(cmd);
            if (!argv.empty())
            {
                m_exePath = argv[0];
                m_arguments.assign(argv.begin() + 1, argv.end());
            }
        }

        inline void clearArguments()
        {
            m_arguments.clear();
        }

        inline void appendArgument(const std::string &arg)
        {
            m_arguments.push_back(arg);
        }

        inline void appendArguments(const std::vector<std::string> &argv)
        {
            m_arguments.insert(m_arguments.end(), argv.begin(), argv.end());
        }

        inline void setWorkingDirectory(const std::string &path)
        {
            m_workingDirectory = path;
        }

        inline void setDetached(bool detached = true)
        {
            m_detached = detached;
        }

        /**
         * Sets a callback function to be called for each line of output captured from the process.
         * The callback is called on the same thread as run().
         * @param callback The callback function.
         */
        inline void setOutputCallback(std::function<void(const std::string &)> callback)
        {
            m_outputCallback = callback;
        }

        /**
         * Sets a callback function to be called for each line of error output captured from the process.
         */
        inline void setErrorCallback(std::function<void(const std::string &)> callback)
        {
            m_errorCallback = callback;
        }

        /**
         * Sets environment variables for the new process in the form KEY=VALUE,
         * this function overwrites any previously set environment variables.
         * @param env A vector of environment variable strings.
         */
        inline void setEnvironment(const std::vector<std::string> &env)
        {
            m_environment = env;
            if (!env.empty())
                m_hasCustomEnvironment = true;
        }

        /**
         * Adds a single environment variable in the form KEY=VALUE
         * @param envVar The environment variable string.
         */
        inline void pushEnvironmentVariable(const std::string &envVar)
        {
            m_environment.push_back(envVar);
            m_hasCustomEnvironment = true;
        }

        inline int getExitCode() const
        {
            return m_exitCode;
        }

        /**
         * Runs the configured process and waits for it to finish.
         * If output/error callbacks are set, output/error streams are read in separate threads.
         * @return 0 on success, -1 on error (exceptions are thrown on errors).
         */
        int run();

        /**
         * Starts the configured process without waiting for it to finish.
         * In detached mode the process is fully detached from the parent.
         * In non-detached mode the process is a child of the parent and
         * should be waited on later to avoid zombie processes. (see waitForExit())
         * If output/error callbacks are set, output/error streams are read in separate threads.
         * If after calling this function the programs reads from out/err streams,
         * the callbacks won't receive the read lines.
         * @return 0 on success, -1 on error (exceptions are thrown on errors).
         */
        int start();

        /**
         * Waits for the started process to exit and retrieves its exit code.
         * Should be called only for non-detached processes started with start().
         * @return The exit code of the process, or -1 on error (exceptions are thrown on errors).
         */
        int waitForExit();

        bool running() const
        {
            return m_running;
        }

        bool outputAvailable() const
        {
            return m_stdoutBuf && (m_stdoutBuf->available() || m_stdoutBuf->hasData());
        }

        bool errorAvailable() const
        {
            return m_stderrBuf && (m_stderrBuf->available() || m_stderrBuf->hasData());
        }

    public:
        /**
         * Standard input stream of the process.
         * Can be used to write to the process's stdin.
         * Before start() is called, the stream is not connected.
         */
        std::ostream in = std::ostream(nullptr);
        /**
         * Standard output stream of the process.
         * Can be used to read from the process's stdout if output capture is enabled.
         * Before start() is called, the stream is not connected.
         */
        std::istream out = std::istream(nullptr);
        /**
         * Standard error stream of the process.
         * Can be used to read from the process's stderr if output capture is enabled.
         * Before start() is called, the stream is not connected.
         */
        std::istream err = std::istream(nullptr);

    private:
        std::vector<std::string> buildArgv(const std::string &cmd) const;

        char *const *buildArgvArray(const std::vector<std::string> &argv) const;
        void freeArgvArray(char *const *argv) const;

        void monitorProcess();
        void onProcessExit();

        void closePipes();
        void closePipe(int pipeFd[2], bool openFlags[2], int endsToClose = 2);

    private:
        std::filesystem::path m_exePath;
        std::vector<std::string> m_arguments;
        std::string m_workingDirectory;
        std::vector<std::string> m_environment;
        bool m_hasCustomEnvironment = false;
        bool m_detached = false;
        OutputLineCallback m_outputCallback = nullptr;
        OutputLineCallback m_errorCallback = nullptr;
        int m_exitCode = -1;
        int m_stdOutPipe[2];
        bool m_stdOutPipeOpen[2] = {false, false};
        int m_stdErrPipe[2];
        bool m_stdErrPipeOpen[2] = {false, false};
        int m_stdInPipe[2];
        bool m_stdInPipeOpen[2] = {false, false};
        bool m_running = false;
        int m_pid = -1;
        std::thread m_monitorThread;

        fd_streambuf* m_stdinBuf = nullptr;
        fd_streambuf* m_stdoutBuf = nullptr;
        fd_streambuf* m_stderrBuf = nullptr;
    };
};