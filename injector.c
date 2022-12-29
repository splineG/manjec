#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: injector.exe <process_name> <dll_path>\n");
        return 1;
    }

    // Get the target process name and DLL path
    char* processName = argv[1];
    char* dllPath = argv[2];

    // Get the process ID of the target process
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe))
        {
            do
            {
                if (lstrcmp(pe.szExeFile, processName) == 0)
                {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }

    if (pid == 0) {
        printf("Failed to find process with name '%s'\n", processName);
        return 1;
    }

    // Open the target process
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        printf("Failed to open process (error %d)\n", GetLastError());
        return 1;
    }

    // Allocate memory for the DLL path in the target process
    LPVOID remoteDllPath = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    if (remoteDllPath == NULL) {
        printf("Failed to allocate memory in target process (error %d)\n", GetLastError());
        return 1;
    }

    // Write the DLL path to the allocated memory in the target process
    if (!WriteProcessMemory(hProcess, remoteDllPath, dllPath, strlen(dllPath) + 1, NULL)) {
        printf("Failed to write to target process memory (error %d)\n", GetLastError());
        return 1;
    }

    // Get the address of the LoadLibrary function
    LPTHREAD_START_ROUTINE loadLibraryAddr = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (loadLibraryAddr == NULL) {
        printf("Failed to get LoadLibraryA address (error %d)\n", GetLastError());
        return 1;
    }

    // Create a new thread in the target process to load the DLL
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, remoteDllPath, 0, NULL);
    if (hThread == NULL) {
        printf("Failed to create remote thread (error %d)\n", GetLastError());
        return 1;
    }

    // Wait for the thread to finish
    WaitForSingleObject(hThread, INFINITE);

    // Free the allocated memory in the target process
    VirtualFreeEx(hProcess, remoteDllPath, strlen(dllPath) + 1, MEM_RELEASE);

    // Close handles
    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}
