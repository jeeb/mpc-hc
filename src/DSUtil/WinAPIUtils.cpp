/*
 * (C) 2011-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "WinAPIUtils.h"
#include "SysVersion.h"

bool SetPrivilege(LPCTSTR privilege, bool bEnable)
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    SetThreadExecutionState(ES_CONTINUOUS);

    // Get a token for this process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    // Get the LUID for the privilege.
    LookupPrivilegeValue(nullptr, privilege, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1;  // one privilege to set
    tkp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

    // Set the privilege for this process.
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)nullptr, 0);

    return (GetLastError() == ERROR_SUCCESS);
}

CString GetHiveName(HKEY hive)
{
    switch ((ULONG_PTR)hive) {
        case (ULONG_PTR)HKEY_CLASSES_ROOT:
            return _T("HKEY_CLASSES_ROOT");
        case (ULONG_PTR)HKEY_CURRENT_USER:
            return _T("HKEY_CURRENT_USER");
        case (ULONG_PTR)HKEY_LOCAL_MACHINE:
            return _T("HKEY_LOCAL_MACHINE");
        case (ULONG_PTR)HKEY_USERS:
            return _T("HKEY_USERS");
        case (ULONG_PTR)HKEY_PERFORMANCE_DATA:
            return _T("HKEY_PERFORMANCE_DATA");
        case (ULONG_PTR)HKEY_CURRENT_CONFIG:
            return _T("HKEY_CURRENT_CONFIG");
        case (ULONG_PTR)HKEY_DYN_DATA:
            return _T("HKEY_DYN_DATA");
        case (ULONG_PTR)HKEY_PERFORMANCE_TEXT:
            return _T("HKEY_PERFORMANCE_TEXT");
        case (ULONG_PTR)HKEY_PERFORMANCE_NLSTEXT:
            return _T("HKEY_PERFORMANCE_NLSTEXT");
        default:
            return _T("");
    }
}

bool ExportRegistryKey(CStdioFile& file, HKEY hKeyRoot, CString keyName)
{
    // Registry functions don't set GetLastError(), so it needs to be set explicitly
    LSTATUS errorCode = ERROR_SUCCESS;

    HKEY hKey = nullptr;
    errorCode = RegOpenKeyEx(hKeyRoot, keyName, 0, KEY_READ, &hKey);
    if (errorCode != ERROR_SUCCESS) {
        SetLastError(errorCode);
        return false;
    }

    DWORD subKeysCount = 0, maxSubKeyLen = 0;
    DWORD valuesCount = 0, maxValueNameLen = 0, maxValueDataLen = 0;
    errorCode = RegQueryInfoKey(hKey, nullptr, nullptr, nullptr, &subKeysCount, &maxSubKeyLen, nullptr, &valuesCount, &maxValueNameLen, &maxValueDataLen, nullptr, nullptr);
    if (errorCode != ERROR_SUCCESS) {
        SetLastError(errorCode);
        return false;
    }
    maxSubKeyLen += 1;
    maxValueNameLen += 1;

    CString buffer;

    buffer.Format(_T("[%s\\%s]\n"), GetHiveName(hKeyRoot), keyName);
    file.WriteString(buffer);

    CString valueName;
    DWORD valueNameLen, valueDataLen, type;
    BYTE* data = DEBUG_NEW BYTE[maxValueDataLen];

    for (DWORD indexValue = 0; indexValue < valuesCount; indexValue++) {
        valueNameLen = maxValueNameLen;
        valueDataLen = maxValueDataLen;

        errorCode = RegEnumValue(hKey, indexValue, valueName.GetBuffer(maxValueNameLen), &valueNameLen, nullptr, &type, data, &valueDataLen);
        if (errorCode != ERROR_SUCCESS) {
            SetLastError(errorCode);
            return false;
        }

        switch (type) {
            case REG_SZ: {
                CString str((TCHAR*)data);
                str.Replace(_T("\\"), _T("\\\\"));
                str.Replace(_T("\""), _T("\\\""));
                buffer.Format(_T("\"%s\"=\"%s\"\n"), valueName, str);
                file.WriteString(buffer);
            }
            break;
            case REG_BINARY:
                buffer.Format(_T("\"%s\"=hex:%02x"), valueName, data[0]);
                file.WriteString(buffer);
                for (DWORD i = 1; i < valueDataLen; i++) {
                    buffer.Format(_T(",%02x"), data[i]);
                    file.WriteString(buffer);
                }
                file.WriteString(_T("\n"));
                break;
            case REG_DWORD:
                buffer.Format(_T("\"%s\"=dword:%08x\n"), valueName, *((DWORD*)data));
                file.WriteString(buffer);
                break;
            default: {
                CString msg;
                msg.Format(_T("The value \"%s\\%s\\%s\" has an unsupported type and has been ignored.\nPlease report this error to the developers."),
                           GetHiveName(hKeyRoot), keyName, valueName);
                AfxMessageBox(msg, MB_ICONERROR | MB_OK);
            }
            delete [] data;
            return false;
        }
    }

    delete [] data;

    file.WriteString(_T("\n"));

    CString subKeyName;
    DWORD subKeyLen;

    for (DWORD indexSubKey = 0; indexSubKey < subKeysCount; indexSubKey++) {
        subKeyLen = maxSubKeyLen;

        errorCode = RegEnumKeyEx(hKey, indexSubKey, subKeyName.GetBuffer(maxSubKeyLen), &subKeyLen, nullptr, nullptr, nullptr, nullptr);
        if (errorCode != ERROR_SUCCESS) {
            SetLastError(errorCode);
            return false;
        }

        buffer.Format(_T("%s\\%s"), keyName, subKeyName);

        if (!ExportRegistryKey(file, hKeyRoot, buffer)) {
            return false;
        }
    }

    errorCode = RegCloseKey(hKey);
    SetLastError(errorCode);
    return true;
}

void GetMessageFont(LOGFONT* lf)
{
    SecureZeroMemory(lf, sizeof(LOGFONT));
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    if (!SysVersion::IsVistaOrLater()) {
        ncm.cbSize -= sizeof(ncm.iPaddedBorderWidth);
    }

    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    *lf = ncm.lfMessageFont;
}

void GetStatusFont(LOGFONT* lf)
{
    SecureZeroMemory(lf, sizeof(LOGFONT));
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(NONCLIENTMETRICS);
    if (!SysVersion::IsVistaOrLater()) {
        ncm.cbSize -= sizeof(ncm.iPaddedBorderWidth);
    }

    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    *lf = ncm.lfStatusFont;
}

int CALLBACK EnumFontFamExProc(CONST LOGFONT* ignored0, CONST TEXTMETRIC* ignored1, DWORD ignored2, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(ignored0);
    UNREFERENCED_PARAMETER(ignored1);
    UNREFERENCED_PARAMETER(ignored2);
    *reinterpret_cast<bool*>(lParam) = true;
    return TRUE;
}

bool IsFontInstalled(LPCTSTR lpszFont)
{
    // Get the screen DC
    CDC dc;
    if (!dc.CreateCompatibleDC(nullptr)) {
        return false;
    }

    LOGFONTW lf;
    // Any character set will do
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = 0;
    // Set the facename to check for
    _tcscpy_s(lf.lfFaceName, lpszFont);
    bool bAccepted = false;
    // Enumerate fonts
    EnumFontFamiliesEx(dc, &lf, EnumFontFamExProc, reinterpret_cast<LPARAM>(&bAccepted), 0);

    return bAccepted;
}

bool ExploreToFile(LPCTSTR path)
{
    bool success = false;
    HRESULT res = CoInitialize(nullptr);

    if (res == S_OK || res == S_FALSE) {
        PIDLIST_ABSOLUTE pidl;

        if (SHParseDisplayName(path, nullptr, &pidl, 0, nullptr) == S_OK) {
            success = SUCCEEDED(SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0));
            CoTaskMemFree(pidl);
        }

        CoUninitialize();
    }

    return success;
}

bool FileExists(LPCTSTR fileName)
{
    return (INVALID_FILE_ATTRIBUTES != ::GetFileAttributes(fileName));
}

CString GetProgramPath(bool bWithExecutableName /*= false*/)
{
    // this function can handle paths over the MAX_PATH limitation
    CString path;
    LPTSTR szPath = path.GetBufferSetLength(32767);
    if (!szPath) {
        goto exit;
    }
    DWORD dwLength = ::GetModuleFileName(nullptr, szPath, 32767);
    if (!dwLength) {
        path.Empty();
        goto exit;
    }
    path.Truncate(dwLength);
    if (dwLength && !bWithExecutableName) {
        DWORD i = dwLength - 6;// the last five characters (_T("x.exe")) are skipped in this loop
        while (szPath[i] != _T('\\')) {
            --i;
        }
        path.Truncate(i + 1);// truncate string right after the _T('\\')
    }
exit:
    return path;
}
