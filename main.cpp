//
// This is a convenience launcher for Grognak's Mod Manager.
// It searches for an installed python interpreter and tells it to run main.py.
//
// If python.exe is found in a PATH dir, that will be used.
// Otherwise the registry is checked.
//   [HKCU and HKLM]\SOFTWARE\Python\PythonCore
//   [HKCU and HKLM]\SOFTWARE\Wow6432Node\Python\PythonCore
//   Priority is given to Python 2.7+, then 3.x, then 2.6.
//

// It can be compiled with mingw gcc, and possibly Visual Studio.
// There are no dependencies, only the standard library and Windows API.

// You may need the following linker options:
//   -static-libstdc++ -static-libgcc

// Reminders to shrink filesize:
//   Strip debugging symbols from the exe.
//     With a project linker-related setting, or the GNU 'strip' command.
//   Tell the compiler to optimize a little.

// This WILL work on 64bit systems if compiled as a 32bit exe (Wow64 is accounted for).
// Compiling a 64bit exe is unnecessary, but might be compilable anyhow.

// Visual Studio 2010 needs special effort to support Win2000 through WinXP SP1.
//   http://www.misanthropicgeek.net/?p=925
//   http://mulder.googlecode.com/svn/!svn/bc/303/trunk/Utils/EncodePointerLib/


#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0500  // Win2000 API

// This app explicitly uses unicode funcs and wide chars everywhere.
#define UNICODE        // Windows headers: SomeFunc -> SomeFuncA or SomeFuncW.
#define _UNICODE       // MS C runtime headers: _tcslen -> strlen or wcslen.

#include <cstdlib>
#include <map>         // std::map
#include <vector>      // std::vector
#include <string>      // std::wstring
#include <iostream>    // std::wcout
#include <algorithm>   // std::sort

#include <windows.h>
#include <shellapi.h>  // Omitted by WIN32_LEAN_AND_MEAN.


#define MAX_REGKEY_NAME 255        // Actual unicode character limit.
#define MAX_REGVALUE_NAME 16383    // Actual unicode character limit.
#define MAX_REGVALUE_CONTENT 8192  // Arbitrary byte limit.

#define KEY_WOW64_64KEY 0x0100     // View the real registry (no 32bit redirection on 64bit OSs).
#define KEY_WOW64_32KEY 0x0200     // View the jailed 32bit registry (SOFTWARE\Wow6232Node on 64bit OSs).
                                   // These WOW64 flags have no effect on 32bit OSs.

LONG GetPythonDirsFromRegistry(std::map<std::wstring, std::wstring> &destMap, const HKEY &hHive, const std::wstring &coreRegPath, const REGSAM &accessFlags, const std::wstring &suffix);
LONG EnumerateSubkeys(std::vector<std::wstring> &destVector, const HKEY &hKey);
LONG GetStringRegValue(std::wstring &destStr, const HKEY &hKey, const std::wstring &valueName);
std::wstring JoinFilePath(const std::wstring &pathOne, const std::wstring &pathTwo);
std::vector<std::wstring> SplitString(const std::wstring &s, const wchar_t &sepChar);
BOOL IsWowJailed();
int GetBitness();
std::wstring FormatWinError(const int &dErr);


int main(int argc, char *argv[]) {
  HKEY hKey;
  BOOL bRes;
  int iRes;
  DWORD dRes;
  LONG lRes;

  std::vector<std::wstring> candidateDirPaths;
  std::vector<std::wstring> candidateExePaths;


  // Find the location of this executable.
  // Oversize the buffer by 1 for a spare NULL, in case the value wasn't terminated (XP).
  std::vector<wchar_t> selfExePathBuf(MAX_PATH + 1);
  std::fill(selfExePathBuf.begin(), selfExePathBuf.end(), *L"\0");  // Ensure it's all NULLs.
  dRes = GetModuleFileName(NULL, selfExePathBuf.data(), selfExePathBuf.size());
  if (dRes == 0) {
    DWORD dErr = GetLastError();
    std::wcout << std::endl;
    std::wcout << L"Error: GetModuleFileName failed (code " << dErr << L")." << std::endl;
    std::wcout << FormatWinError(dErr) << std::endl;

    std::wcout << std::endl;
    system("PAUSE");
    return EXIT_FAILURE;
  }
  std::wstring selfExePath(selfExePathBuf.data());  // Let buf's first NULL decide length.
  std::wstring selfDirPath = selfExePath.substr(0, selfExePath.find_last_of(L"\\"));
  std::wcout << L"Rooting at: " + selfDirPath << std::endl;


  std::wcout << L"Collecting Python candidates from PATH..." << std::endl;

  DWORD neededChars = GetEnvironmentVariableW(L"PATH", NULL, 0);
  std::vector<wchar_t> pathBuf(neededChars);
  DWORD pathBufCharCount = pathBuf.size();
  dRes = GetEnvironmentVariableW(L"PATH", pathBuf.data(), pathBufCharCount);
  if (dRes == 0) {
    DWORD dErr = GetLastError();
    std::wcout << std::endl;
    std::wcout << L"Error: GetEnvironmentVariableW failed (code " << dErr << L")." << std::endl;
    std::wcout << FormatWinError(dErr) << std::endl;

    std::wcout << std::endl;
    system("PAUSE");
    return EXIT_FAILURE;
  }
  std::wstring pathEnv(pathBuf.data());

  std::vector<std::wstring> envDirPaths = SplitString(pathEnv, L';');
  for (std::vector<std::wstring>::iterator it = envDirPaths.begin(); it != envDirPaths.end(); ++it) {
    if ((*it).empty()) continue;
    candidateDirPaths.push_back(*it);
  }


  // Search for HKCU and HKLM for regular and Wow64 (32bit Python on 64bit OS) entries.
  std::wcout << L"Collecting Python candidates from registry..." << std::endl;

  std::wstring coreRegPath(L"SOFTWARE\\Python\\PythonCore");
  std::map<std::wstring, std::wstring> pyRegDirPathMap;

  if (GetBitness() > 32 || IsWowJailed()) {
    // If this is a jailed 32bit app in a 64bit system, get both registry views.
    // If this is a 64bit app, assume Wow64 subsystem is available, and get both views.
    std::map<std::wstring, std::wstring> hkcuDirPathMap;
    lRes = GetPythonDirsFromRegistry(hkcuDirPathMap, HKEY_CURRENT_USER, coreRegPath, KEY_WOW64_64KEY, L" (HKCU 64bit)");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hkcuDirPathMap.begin(), hkcuDirPathMap.end());

    std::map<std::wstring, std::wstring> hklmDirPathMap;
    lRes = GetPythonDirsFromRegistry(hklmDirPathMap, HKEY_LOCAL_MACHINE, coreRegPath, KEY_WOW64_64KEY, L" (HKLM 64bit)");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hklmDirPathMap.begin(), hklmDirPathMap.end());

    std::map<std::wstring, std::wstring> hkcuWow64DirPathMap;
    lRes = GetPythonDirsFromRegistry(hkcuWow64DirPathMap, HKEY_CURRENT_USER, coreRegPath, KEY_WOW64_32KEY, L" (HKCU 32bit)");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hkcuWow64DirPathMap.begin(), hkcuWow64DirPathMap.end());

    std::map<std::wstring, std::wstring> hklmWow64DirPathMap;
    lRes = GetPythonDirsFromRegistry(hklmWow64DirPathMap, HKEY_LOCAL_MACHINE, coreRegPath, KEY_WOW64_32KEY, L" (HKLM 32bit)");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hklmWow64DirPathMap.begin(), hklmWow64DirPathMap.end());
  }
  else {
    // Get the default view of the registry.

    // Format a string from the bitness int.
    //   In C++11, you can use std::wstring x = std::to_wstring(myInt);
    std::vector<wchar_t> bitnessBuf(32);  // Arbitrarily char count.
    do {
      // The func may not terminate, so print up to size-1 chars, with a NULL at the end.
      std::fill(bitnessBuf.begin(), bitnessBuf.end(), *L"\0");  // Ensure it's all NULLs.
      iRes = _snwprintf(bitnessBuf.data(), bitnessBuf.size()-1, L"%dbit", GetBitness());
      if (iRes < 0) bitnessBuf.resize(bitnessBuf.size() * 2);
    } while (iRes < 0);  // Grow the buffer until it can hold the string.
    std::wstring bitStr =(bitnessBuf.data());

    std::map<std::wstring, std::wstring> hkcuWow64DirPathMap;
    lRes = GetPythonDirsFromRegistry(hkcuWow64DirPathMap, HKEY_CURRENT_USER, coreRegPath, 0, L" (HKCU " + bitStr + L")");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hkcuWow64DirPathMap.begin(), hkcuWow64DirPathMap.end());

    std::map<std::wstring, std::wstring> hklmWow64DirPathMap;
    lRes = GetPythonDirsFromRegistry(hklmWow64DirPathMap, HKEY_LOCAL_MACHINE, coreRegPath, 0, L" (HKLM " + bitStr + L")");
    if (lRes == ERROR_SUCCESS) pyRegDirPathMap.insert(hklmWow64DirPathMap.begin(), hklmWow64DirPathMap.end());
  }

  std::vector<std::wstring> py3xDirPaths;
  std::vector<std::wstring> py2xModernDirPaths;
  std::vector<std::wstring> py2xLegacyDirPaths;
  for (std::map<std::wstring, std::wstring>::iterator it = pyRegDirPathMap.begin(); it != pyRegDirPathMap.end(); ++it) {
    //std::wcout << L"DEBUG - RegDirPath: " + (*it).first << std::endl;

    if ((*it).first >= L"3.0") {
      py3xDirPaths.push_back((*it).second);
    }
    else if ((*it).first >= L"2.7") {
      py2xModernDirPaths.push_back((*it).second);
    }
    else if ((*it).first >= L"2.6") {
      py2xLegacyDirPaths.push_back((*it).second);
    }
    else {  // Ignore anything before 2.6.
      std::wcout << L"Ignoring Python " + (*it).first + L": Too old." << std::endl;
    }
  }
  // Add candidates in reverse (desc) order, giving priority to modern 2.x.
  candidateDirPaths.reserve(candidateDirPaths.size() + py2xModernDirPaths.size() + py3xDirPaths.size() + py2xLegacyDirPaths.size());
  candidateDirPaths.insert(candidateDirPaths.end(), py2xModernDirPaths.rbegin(), py2xModernDirPaths.rend());
  candidateDirPaths.insert(candidateDirPaths.end(), py3xDirPaths.rbegin(), py3xDirPaths.rend());
  candidateDirPaths.insert(candidateDirPaths.end(), py2xLegacyDirPaths.rbegin(), py2xLegacyDirPaths.rend());

  std::wcout << std::endl;


  // Check for python.exe in each candidate dir.
  for (std::vector<std::wstring>::iterator it = candidateDirPaths.begin(); it != candidateDirPaths.end(); ++it) {
    std::wstring candidateDirPath = *it;
    std::wstring candidateExePath = JoinFilePath(candidateDirPath, L"python.exe");

    DWORD dwAttrs = GetFileAttributesW(candidateExePath.c_str());
    if (dwAttrs == INVALID_FILE_ATTRIBUTES) {
      // Could call GetLastError(), but probably the file doesn't exist.
    }
    else if (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) {
      //std::wcout << (L"o.0 \"" + candidateExePath + L"\" is a directory!?") << std::endl;
    }
    else {
      std::wcout << L"Found \"" + candidateExePath + L"\"" << std::endl;
      candidateExePaths.push_back(candidateExePath);
    }
  }

  std::wcout << std::endl;

  if (candidateExePaths.empty()) {
    std::wcout << L"Error: No suitable Python installation was found." << std::endl;
    std::wcout << L"Download Python 2.x at http://www.python.org/getit/" << std::endl;
    std::wcout << std::endl;
    system("PAUSE");
    return EXIT_FAILURE;
  }


  std::wcout << L"Launching the Mod Manager..." << std::endl;

  std::wstring chosenExePath = candidateExePaths.front();
  std::wstring exeArgStr = L"main.py";
  //std::wstring exeArgStr = L"-c \"import time; print 'hi'; time.sleep(2)\"";
  std::wcout << chosenExePath + L" " + exeArgStr << std::endl;
  std::wcout << std::endl << std::endl;

  // Spawn the python process.
  //   Let it use this app's console.
  //   Suppress the default GUI messagebox on failure.
  //
  // Never trust ShellExec's return code or hInstApp. Use GetLastError.
  //   http://blogs.msdn.com/b/oldnewthing/archive/2012/10/18/10360604.aspx
  SHELLEXECUTEINFO execInfo;
  execInfo.cbSize = sizeof(SHELLEXECUTEINFO);
  execInfo.fMask = 0|SEE_MASK_NO_CONSOLE|SEE_MASK_FLAG_NO_UI;
  execInfo.hwnd = NULL;
  execInfo.lpVerb = L"open";
  execInfo.lpFile = chosenExePath.c_str();
  execInfo.lpParameters = exeArgStr.c_str();
  execInfo.lpDirectory = selfDirPath.c_str();
  execInfo.nShow = SW_SHOWNORMAL;
  execInfo.hInstApp = NULL;

  bRes = ShellExecuteEx(&execInfo);
  if (!bRes) {
    DWORD dErr = GetLastError();
    std::wcout << std::endl;
    std::wcout << L"Error: ShellExecuteEx failed (code " << dErr << L")." << std::endl;
    std::wcout << FormatWinError(dErr) << std::endl;

    std::wcout << std::endl;
    system("PAUSE");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
 }


// Gets paths of installed Python dirs, from the PythonCore key in a registry hive.
//
// They will be stored in destMap (by calling clear() and insert()).
//   Each map key is (regSubKeyName + suffix), and each map value is a dir path.
//   Registry subkeys are named 2.6, 2.7, etc.
// A result code is returned: ERROR_SUCCESS, or something from:
//   EnumerateSubkeys(custom func) or RegOpenKeyExW()
//
LONG GetPythonDirsFromRegistry(std::map<std::wstring, std::wstring> &destMap, const HKEY &hHive, const std::wstring &coreRegPath, const REGSAM &accessFlags, const std::wstring &suffix) {
  std::map<std::wstring, std::wstring> results;

  HKEY hKey;
  LONG lRes;
  lRes = RegOpenKeyExW(hHive, coreRegPath.c_str(), 0, KEY_READ|accessFlags, &hKey);
  if (lRes != ERROR_SUCCESS) return lRes;  // complain

  std::vector<std::wstring> subKeyNames(0);
  lRes = EnumerateSubkeys(subKeyNames, hKey);  // Asc order.
  RegCloseKey(hKey);
  if (lRes != ERROR_SUCCESS) return lRes;  // complain

  for (std::vector<std::wstring>::iterator it = subKeyNames.begin(); it != subKeyNames.end(); ++it) {
    std::wcout << L"DEBUG - RegSubkey: " + *it + suffix << std::endl;
    std::wstring candidateRegPath = coreRegPath + L"\\" + *it + L"\\InstallPath";

    lRes = RegOpenKeyExW(hHive, candidateRegPath.c_str(), 0, KEY_READ|accessFlags, &hKey);
    if (lRes != ERROR_SUCCESS) continue;  // complain
    std::wstring pyDirPath(L"");
    lRes = GetStringRegValue(pyDirPath, hKey, L"");
    RegCloseKey(hKey);
    if (lRes != ERROR_SUCCESS) continue;  // complain

    if (pyDirPath.empty()) continue;
    results.insert(std::pair<std::wstring, std::wstring>(*it + suffix, pyDirPath));
  }

  destMap.clear();
  destMap.insert(results.begin(), results.end());
  return ERROR_SUCCESS;
}


// Gets a list of subkey names immediately within a registry key.
//
// They will be sorted destVector in ascending order (by calling assign() on an existing object).
// A result code is returned: ERROR_SUCCESS, or something from:
//   RegQueryInfoKeyW() or RegEnumKeyExW().
//
LONG EnumerateSubkeys(std::vector<std::wstring> &destVector, const HKEY &hKey) {
  std::vector<std::wstring> results;

  DWORD subKeysCount;
  DWORD maxSubKeyLength;
  LONG lRes = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeysCount, &maxSubKeyLength, NULL, NULL, NULL, NULL, NULL, NULL);
  // if (lRes != ERROR_SUCCESS) // complain

  // Buffer to temporarily hold a subkey name.
  std::vector<wchar_t> nameBuf(maxSubKeyLength + 1);

  for (DWORD i=0; i < subKeysCount; i++) {
    DWORD nameBufCharCount = nameBuf.size();
    lRes = RegEnumKeyExW(hKey, i, nameBuf.data(), &nameBufCharCount, NULL, NULL, NULL, NULL);
    // if (lRes != ERROR_SUCCESS) // complain
    results.push_back( std::wstring(nameBuf.data(), nameBufCharCount) );
  }

  std::sort(results.begin(), results.end());

  destVector.assign(results.begin(), results.end());
  return ERROR_SUCCESS;
}


// Gets the named value within a registry key, or the default value if "".
//
// The value is stored in destStr (by calling assign() an existing string object).
// A result code is returned: ERROR_SUCCESS, or something from RegQueryValueEx().
//
LONG GetStringRegValue(std::wstring &destStr, const HKEY &hKey, const std::wstring &valueName) {
  // Registry values can be anything, so they're generic bytes, not characters.
  DWORD regType = REG_SZ;
  DWORD valueBufByteCount = 0;

  LONG lRes = RegQueryValueEx(hKey, valueName.c_str(), NULL, &regType, NULL, &valueBufByteCount);
  if (lRes != ERROR_SUCCESS) return lRes;  // complain
  // valueBufByteCount is now the required bytes for N chars, maybe including a NULL.

  // Oversize the buffer by 1 for a spare NULL, in case the value wasn't terminated.
  std::vector<wchar_t> valueBuf(valueBufByteCount/sizeof(wchar_t)+1);
  std::fill(valueBuf.begin(), valueBuf.end(), *L"\0");  // Ensure it's all NULLs.

  lRes = RegQueryValueEx(hKey, valueName.c_str(), NULL, &regType, (LPBYTE)valueBuf.data(), &valueBufByteCount);
  if (lRes != ERROR_SUCCESS) return lRes;  // complain
  std::wstring result = std::wstring(valueBuf.data());  // Let buf's first NULL decide length.

  destStr.assign(result);
  return ERROR_SUCCESS;
}


// Returns the concatenation of two file paths, adding a backslash if needed.
//
std::wstring JoinFilePath(const std::wstring &pathOne, const std::wstring &pathTwo) {
  std::wstring result(pathOne);
  if (!result.empty() && result.at(result.length()-1) != L'\\') result += L"\\";
  result += pathTwo;
  return result;
}


// Returns a list of substrings, tokenized on a separator character.
//
// Empty tokens are included.
//
std::vector<std::wstring> SplitString(const std::wstring &s, const wchar_t &sepChar) {
  std::vector<std::wstring> results;
  std::wstring::const_iterator leftIt = s.begin();

  // Handle leading empty chunks, by adding "" until leading seps are gone.
  for (; leftIt < s.end() && *leftIt == sepChar; ++leftIt) {
    results.push_back(std::wstring(L""));
  }
  // Send rightIt onward and drag leftIt along as separators are found.
  // If leftIt reached the end already, one more "" will be added.
  for (std::wstring::const_iterator rightIt = leftIt; rightIt <= s.end(); ++rightIt) {
    if (rightIt == s.end() || *rightIt == sepChar) {
      std::wstring tmpS(leftIt, rightIt);
      results.push_back(tmpS);
      if (rightIt < s.end()) leftIt = rightIt + 1;
    }
  }
  return results;
}


// Returns TRUE if this is a 32bit app jailed by Wow64 on a 64bit system.
//
// FALSE will still be returned if IsWow64Process() is not available on this OS.
//
BOOL IsWowJailed() {
  // Bah, no need for typedef BOOL (WINAPI *IW64PFP)(HANDLE, BOOL *); at the top.

  BOOL bRes = FALSE;
  BOOL (WINAPI *IW64P)(HANDLE, BOOL *);
  IW64P = (BOOL (WINAPI *)(HANDLE, BOOL *))GetProcAddress(GetModuleHandle(L"kernel32"), "IsWow64Process");
  if (IW64P != NULL) IW64P(GetCurrentProcess(), &bRes);

  return bRes;
}


// Returns 32 or 64 (or more?) depending on how this app was compiled.
int GetBitness() {
  return sizeof(void*) * 8;
}


std::wstring FormatWinError(const int &dErr) {
  std::wstring result;

  LPTSTR errorText = NULL;
  FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM
    |FORMAT_MESSAGE_ALLOCATE_BUFFER
    |FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    dErr,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<LPTSTR>(&errorText),
    0,
    NULL);
  if (errorText != NULL) {
    result = std::wstring(errorText);
    LocalFree(errorText);
    errorText = NULL;
  } else {
    std::wstring(L"No description for the error was found.");
  }

  return result;
}
