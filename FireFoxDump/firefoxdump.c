#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <sqlite3.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <objbase.h>

PUCHAR ReadFileContents(_In_ PWCHAR FileName, _Out_ PULONG Size) {
    HANDLE FileHandle;
    ULONG FileSize;
    ULONG BytesRead;
    BOOL Result;
    PUCHAR Buffer = 0;

    FileHandle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (FileHandle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        FileSize = GetFileSize(FileHandle, 0);
        if (FileSize == 0) {
            break;
        }
        Buffer = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, FileSize);
        if (Buffer == 0) {
            break;
        }
        Result = ReadFile(FileHandle, Buffer, FileSize, &BytesRead, 0);
        if (Result == FALSE || BytesRead != FileSize) {
            HeapFree(GetProcessHeap(), 0, Buffer);
            Buffer = 0;
        } else {
            *Size = FileSize;
        }
    } while (FALSE);
    CloseHandle(FileHandle);
    return Buffer;
}

BOOLEAN SetProfilePathAsWorkingDirectory(_In_ PCHAR ProfilePath, _In_ PWCHAR FirefoxPath, _In_ ULONG FirefoxPathSize, _In_ BOOLEAN IsRelative) {
    PCHAR LineEnd = strchr(ProfilePath, '\n');
    *LineEnd = UNICODE_NULL;

    wprintf(L"\nProfile: %hs\n", ProfilePath + sizeof("Path"));

    for (PCHAR Char = ProfilePath + sizeof("Path"); Char < LineEnd; Char++) {
        if (*Char == '/') {
            *Char = '\\';
        }
    }

    WCHAR Buffer[MAX_PATH];
    PWCHAR NewDirectory = FirefoxPath;

    if (IsRelative == TRUE) {
        mbstowcs(FirefoxPath + FirefoxPathSize, ProfilePath + sizeof("Path"), MAX_PATH - FirefoxPathSize);
        *(FirefoxPath + FirefoxPathSize + (LineEnd - ProfilePath) - sizeof("Path=")) = L'\\';
    } else {
        mbstowcs(Buffer, ProfilePath + sizeof("Path"), MAX_PATH);
        wcscat(Buffer, L"\\");
        NewDirectory = Buffer;
    }

    BOOLEAN Result = SetCurrentDirectoryW(NewDirectory);
    *LineEnd = '\n';
    return Result;
}

CONST CHAR CookiesSqlQuery[] = "SELECT host, path, isSecure, name, datetime(expiry / 1000, 'unixepoch', 'localtime') AS expiry, value FROM moz_cookies;";

INT CookiesSqlCallback(PVOID NotUsed, INT Argc, PCHAR* Argv, PCHAR* ColumnName) {
    for (UCHAR Index = 0; Index < Argc; Index++) {
        wprintf(L"- %hS : %hS\n", ColumnName[Index], Argv[Index]);
    }
    wprintf(L"\n");
    return 0;
}

VOID DumpMozillaCookies() {
    sqlite3* CookiesDatabase;
    ULONG Result;

    Result = sqlite3_open_v2("cookies.sqlite", &CookiesDatabase, SQLITE_OPEN_READONLY, 0);
    if (Result != SQLITE_OK) {
        return;
    }

    PCHAR ErrorMessage;
    Result = sqlite3_exec(CookiesDatabase, CookiesSqlQuery, CookiesSqlCallback, 0, &ErrorMessage);
    if (Result != SQLITE_OK) {
        sqlite3_free(ErrorMessage);
    }

    sqlite3_close_v2(CookiesDatabase);
}

INT main() {
    PWSTR AppDataPath;
    HRESULT Result;
    PCHAR ProfilePath;
    PCHAR Buffer;
    ULONG BufferSize;
    WCHAR Path[MAX_PATH * 2];
    ULONG PathSize;
    BOOLEAN IsRelativePath;

    Result = SHGetKnownFolderPath(&FOLDERID_RoamingAppData, KF_FLAG_DONT_UNEXPAND, 0, &AppDataPath);
    if (FAILED(Result)) {
        return Result;
    }

    wcscpy(Path, AppDataPath);
    wcscat(Path, L"\\Mozilla\\Firefox\\profiles.ini");
    PathSize = wcslen(Path) - (sizeof(L"profiles.ini") / sizeof(WCHAR)) + 1;
    CoTaskMemFree(AppDataPath);

    Buffer = (PCHAR)ReadFileContents(Path, &BufferSize);
    if (Buffer == 0) {
        return -1;
    }

    *(Path + PathSize) = UNICODE_NULL;

    for (ProfilePath = strstr(Buffer, "Path="); ProfilePath != 0; ProfilePath = strstr(ProfilePath + 1, "Path=")) {
        IsRelativePath = *(ProfilePath - 3) - '0';
        if (SetProfilePathAsWorkingDirectory(ProfilePath, Path, PathSize, IsRelativePath) == FALSE) {
            continue;
        }

        DumpMozillaCookies();
    }

    HeapFree(GetProcessHeap(), 0, Buffer);
    return 0;
}
