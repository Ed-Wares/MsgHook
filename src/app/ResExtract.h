//This function will extract a binary resource.
//
// MSGHOOK_FILE         DLL            "MsgHook.dll"
// MSGHOOKCLI32_FILE    EXE            "MsgHookCli32.exe"

#include <windows.h>
#include <iostream>
#include <fstream>
#include <tchar.h>

// Function to extract a binary resource (like a DLL) from the executable and save it to disk.
bool ExtractResource(LPCSTR ResourceId, LPCSTR ResourceType, const char* dllPath) {

	std::ifstream f(dllPath);
	if (f.good()) {
		std::cout << "DLL file already exists. Skipping extraction." << std::endl;
		return true; // File already exists, no need to extract
	}
	f.close();
	
    std::cout << "Extraction." << std::endl;

	 // Find and load the resource from within this executable 
    HRSRC hRes = FindResourceA(NULL, ResourceId, ResourceType);
    if (!hRes) {
        std::cerr << "Error: Could not find the DLL resource." << std::endl;
        return false;
    }

    HGLOBAL hResLoad = LoadResource(NULL, hRes);
    if (!hResLoad) {
        std::cerr << "Error: Could not load the DLL resource." << std::endl;
        return false;
    }

    LPVOID pResData = LockResource(hResLoad);
    if (!pResData) {
        std::cerr << "Error: Could not lock the DLL resource." << std::endl;
        return false;
    }

    DWORD dwSize = SizeofResource(NULL, hRes);

    // Write the resource data to a new file on disk ---
    std::ofstream dllFile(dllPath, std::ios::binary);
    if (!dllFile.is_open()) {
        std::cerr << "Error: Could not create DLL file on disk." << std::endl;
        return false;
    }
    dllFile.write(static_cast<const char*>(pResData), dwSize);
    dllFile.close();
    std::cout << "Successfully extracted " << dllPath << " to the current directory." << std::endl;
	return true;
}