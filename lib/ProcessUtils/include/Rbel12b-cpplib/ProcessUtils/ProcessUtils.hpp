#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

namespace cpplib
{
    class Process
    {
    public:
        using OutputLineCallback = std::function<void(const std::string &)>;

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
         * Sets whether to capture the output of the process.
         * Does not work in detached mode.
         */
        inline void setOutputCapture(bool capture = true)
        {
            m_captureOutput = capture;
        }

        /**
         * Sets a callback function to be called for each line of output captured from the process.
         * The callback is called only if output capture is enabled, on the same thread as runCommand().
         * If echo output is enabled, lines are echoed before calling the callback.
         * If the callback is set getOutput() will return an empty string.
         * @param callback The callback function.
         */
        inline void setOutputCallback(std::function<void(const std::string &)> callback)
        {
            m_outputCallback = callback;
        }

        /**
         * Sets whether to echo output lines to stdout/stderr as they are captured.
         */
        inline void setEchoOutput(bool echo = true)
        {
            m_echoOutput = echo;
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

        inline std::string getOutput() const
        {
            return m_capturedOutput;
        }

        inline int getExitCode() const
        {
            return m_exitCode;
        }

        int runCommand();

    private:
        std::string buildCommandLine() const;
        std::vector<std::string> buildArgv(const std::string &cmd) const;

        char *const *buildArgvArray(const std::vector<std::string> &argv) const;
        void freeArgvArray(char *const *argv) const;

    private:
        std::filesystem::path m_exePath;
        std::vector<std::string> m_arguments;
        std::string m_workingDirectory;
        std::vector<std::string> m_environment;
        bool m_hasCustomEnvironment = false;
        bool m_detached = false;
        bool m_captureOutput = false;
        bool m_echoOutput = false;
        OutputLineCallback m_outputCallback = nullptr;

        std::string m_capturedOutput;
        int m_exitCode = -1;
    };
};