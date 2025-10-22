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

        inline void setDetached(bool detached)
        {
            m_detached = detached;
        }

        inline void setInheritHandles(bool inherit)
        {
            m_inheritHandles = inherit;
        }

        inline void setOutputCapture(bool capture)
        {
            m_captureOutput = capture;
        }

        inline void setOutputCallback(std::function<void(const std::string &)> callback)
        {
            m_outputCallback = callback;
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
        bool m_detached = false;
        bool m_inheritHandles = false;
        bool m_captureOutput = false;
        OutputLineCallback m_outputCallback = nullptr;

        std::string m_capturedOutput;
        int m_exitCode = -1;
    };
};