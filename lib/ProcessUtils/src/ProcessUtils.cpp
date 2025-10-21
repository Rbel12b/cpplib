#include <Rbel12b-cpplib/ProcessUtils/ProcessUtils.hpp>
#include <iostream>

namespace ProcessUtils
{
    int runCommand(const std::string &cmd)
    {
#ifdef _WIN32
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;

        // Get underlying OS handles from the C runtime FILE*
        HANDLE hStdOut = (HANDLE)_get_osfhandle(_fileno(stdout));
        HANDLE hStdErr = (HANDLE)_get_osfhandle(_fileno(stderr));

        // Duplicate as inheritable
        HANDLE hOutInherit = NULL, hErrInherit = NULL;
        DuplicateHandle(GetCurrentProcess(), hStdOut,
                        GetCurrentProcess(), &hOutInherit,
                        0, TRUE, DUPLICATE_SAME_ACCESS);
        DuplicateHandle(GetCurrentProcess(), hStdErr,
                        GetCurrentProcess(), &hErrInherit,
                        0, TRUE, DUPLICATE_SAME_ACCESS);

        si.hStdOutput = hOutInherit;
        si.hStdError = hErrInherit;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        // Mutable command buffer
        std::string fullCmd = "cmd /C \"" + cmd + "\"";
        char *cmdline = fullCmd.data();

        BOOL ok = CreateProcessA(
            nullptr,
            cmdline,
            nullptr,
            nullptr,
            TRUE, // allow handle inheritance
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        if (!ok)
        {
            if (hOutInherit)
                CloseHandle(hOutInherit);
            if (hErrInherit)
                CloseHandle(hErrInherit);
            return -1;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Close inherited handles (parent side copies)
        if (hOutInherit)
            CloseHandle(hOutInherit);
        if (hErrInherit)
            CloseHandle(hErrInherit);

        return static_cast<int>(exitCode);
#else
        return system(cmd.c_str());
#endif
    }
};