
// Folcolor(tm) (c) 2020 Kevin Weatherman
// MIT license https://opensource.org/licenses/MIT
#include "StdAfx.h"
#include "resource.h"
#include "FolderColorize.h"

extern WCHAR myPathGlobal[MAX_PATH];
extern int iconOffsetGlobal;


// Icon index to color label
static const LPCSTR nameTable[COLOR_ICON_COUNT] =
{
	"红色",
	"粉色",
	"紫色",
	"蓝色",
	"青色",
	"深青",
	"绿色",
	"黄绿",
	"黄色",
	"橙色",
	"棕色",
	"灰色",
	"灰蓝",
	"黑色",
};


// Return TRUE if system has our installed registry entry
BOOL HasInstallRegistry()
{
	HKEY rootKey = NULL;
	LSTATUS lStatus = RegOpenKeyExA(HKEY_CLASSES_ROOT, REGISTRY_PATH, 0, KEY_READ, &rootKey);
	if (lStatus == ERROR_SUCCESS)
	{
		RegCloseKey(rootKey);
		return TRUE;
	}
	return FALSE;
}


// Write our shell registry
static void InstallRegistry()
{
	// Delete our existing key if it's already there
	DeleteRegistryPath(HKEY_CLASSES_ROOT, REGISTRY_PATH);

	LSTATUS lStatus;
	#define CREATE_KEY(_root, _path, _outkey) \
		lStatus = RegCreateKeyExA(_root, _path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &_outkey, NULL); \
		if (lStatus != ERROR_SUCCESS) CRITICAL_API_FAIL(RegCreateKeyExA, lStatus);

	// Root: HKEY_CLASSES_ROOT\Directory\shell\Folcolor
	HKEY rootKey = NULL;
	CREATE_KEY(HKEY_CLASSES_ROOT, REGISTRY_PATH, rootKey);

	// ASCII copy installation path as icon source path
	char srcPath[MAX_PATH];
	size_t dontCare;
	errno_t en = wcstombs_s(&dontCare, srcPath, sizeof(srcPath), myPathGlobal, sizeof(srcPath) - 1);
	if (en != 0)
		CRITICAL_API_ERRNO(wcstombs_s, en);

	int len;
	DWORD dwValue;
	char buffer[sizeof(COMMAND_ICON "16") + sizeof(COMMAND_FOLDER) + MAX_PATH];
	#define WRITE_STRING(_key, _name, _str, _size) lStatus = RegSetValueEx(_key, _name, 0, REG_SZ, (PBYTE) (_str), (DWORD) (_size)); if (lStatus != ERROR_SUCCESS) CRITICAL_API_FAIL(RegSetValueEx, lStatus);
	#define WRITE_LITERAL(_key, _name, _literal) WRITE_STRING(_key, _name, _literal, sizeof(_literal))
	#define WRITE_DWORD(_key, _name, _value) dwValue = DWORD (_value); lStatus = RegSetValueEx(_key, _name, 0, REG_DWORD, (PBYTE) &dwValue, sizeof(DWORD)); if (lStatus != ERROR_SUCCESS) CRITICAL_API_FAIL(RegSetValueEx, lStatus);

	// "Icon"="C:\\Program Files (x86)\\Folcolor\\Folcolor.exe,4"
	#define WRITE_ICON(_key, _index) \
		len = sprintf_s(buffer, sizeof(buffer), "%s" TARGET_NAME ",%u", srcPath, (UINT) (_index)); \
		WRITE_STRING(_key, "Icon", buffer, (len + 1))
	// "(Default)"="C:\\Program Files (x86)\\Folcolor\\Folcolor.exe --index=1"
	#define WRITE_COMMAND(_key, _index) \
		len = sprintf_s(buffer, sizeof(buffer), "%s" TARGET_NAME " " COMMAND_ICON "%d " COMMAND_FOLDER "%%1", srcPath, (int) (_index)); \
		WRITE_STRING(_key, "", buffer, (len + 1))

	// Root command entry
	WRITE_LITERAL(rootKey, "MUIVerb", "着色");
	WRITE_LITERAL(rootKey, "SubCommands", "");
	len = sprintf_s(buffer, sizeof(buffer), "%s" TARGET_NAME, srcPath);
	WRITE_STRING(rootKey, "Icon", buffer, (len + 1));

	// "shell" sub-level
	// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell
	HKEY shellKey = NULL;
	CREATE_KEY(rootKey, "shell", shellKey);

		// Fill in folder icon color entries
		HKEY numberKey = NULL, commandKey = NULL;
		for (UINT i = 0; i < COLOR_ICON_COUNT; i++)
		{
			// ---------------------------------------------------------------------------
			// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\nn
			char num[8];
			sprintf_s(num, sizeof(num), "%02u", i);
			CREATE_KEY(shellKey, num, numberKey);
			WRITE_ICON(numberKey, (i + (UINT) iconOffsetGlobal))
			WRITE_STRING(numberKey, "MUIVerb", nameTable[i], strlen(nameTable[i])+1);

				// ---------------------------------------------------------------------------
				// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\nn\command
				CREATE_KEY(numberKey, "command", commandKey);
				WRITE_COMMAND(commandKey, i);
				RegCloseKey(commandKey);
				commandKey = NULL;
				// ---------------------------------------------------------------------------

			RegCloseKey(numberKey);
			numberKey = NULL;
			// ---------------------------------------------------------------------------
		}

		// ---------------------------------------------------------------------------
		// "Restore Default" entry
		// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\00
		CREATE_KEY(shellKey, "14", numberKey);
		#define DEFAULT_FOLDER_ICON_GROUP "%SystemRoot%\\system32\\shell32.dll,4"
		WRITE_STRING(numberKey, "Icon", DEFAULT_FOLDER_ICON_GROUP, sizeof(DEFAULT_FOLDER_ICON_GROUP));
		#undef DEFAULT_FOLDER_ICON_GROUP
		WRITE_LITERAL(numberKey, "MUIVerb", "复原");
		// Line separator
		WRITE_DWORD(numberKey, "CommandFlags", 0x20);
			// ---------------------------------------------------------------------------
			// Command component
			// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\00\command
			CREATE_KEY(numberKey, "command", commandKey);
			WRITE_COMMAND(commandKey, COLOR_ICON_COUNT);
			RegCloseKey(commandKey);
			commandKey = NULL;
			// ---------------------------------------------------------------------------
		RegCloseKey(numberKey);
		numberKey = NULL;

		// ---------------------------------------------------------------------------
		// "Launch Folcolor" entry
		// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\nn
		CREATE_KEY(shellKey, "15", numberKey);
		len = sprintf_s(buffer, sizeof(buffer), "%s" TARGET_NAME, srcPath);
		WRITE_STRING(numberKey, "Icon", buffer, (len + 1));
		WRITE_LITERAL(numberKey, "MUIVerb", "卸载");
		// Add UAC overlay to the icon to reflect needing administrator elevation
		WRITE_LITERAL(numberKey, "HasLUAShield", "");
		// Line separator
		WRITE_DWORD(numberKey, "CommandFlags", 0x20);
			// ---------------------------------------------------------------------------
			// Command component
			// HKEY_CLASSES_ROOT\Directory\shell\Folcolor\shell\nn\command
			CREATE_KEY(numberKey, "command", commandKey);
			len = sprintf_s(buffer, sizeof(buffer), "%s" TARGET_NAME, srcPath);
			WRITE_STRING(commandKey, "", buffer, (len + 1))
			RegCloseKey(commandKey);
			commandKey = NULL;
			// ---------------------------------------------------------------------------
		RegCloseKey(numberKey);
		numberKey = NULL;

		// ---------------------------------------------------------------------------

	RegCloseKey(shellKey);
	shellKey = NULL;

	RegCloseKey(rootKey);
	rootKey = NULL;
}


#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

BOOL IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    // 创建管理员组的 SID
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        
        // 检查令牌是否包含管理员组
        if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroup);
    }
    return isAdmin;
}

void RequestAdminPrivileges() {
    if (!IsRunAsAdmin()) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        // 使用 ShellExecute 以管理员权限重启进程
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (ShellExecuteExW(&sei)) {
            exit(0); // 退出当前非管理员进程
        }
    }
}


// Install ourself
void Install(){
	// ------------------------------------------------------------------------
	RequestAdminPrivileges();

	// Create installation folder
	if (!CreateDirectoryW(myPathGlobal, NULL))
		CRITICAL_API_FAIL(CreateDirectoryW, GetLastError());

	// Copy ourself there
	// ------------------------------------------------------------------------
	WCHAR myPath[MAX_PATH];
	if(!GetModuleFileNameW(NULL, myPath, _countof(myPath)))
		CRITICAL_API_FAIL(GetModuleFileNameW, GetLastError());

	WCHAR myName[_MAX_FNAME];
	if (!GetModuleBaseNameW(GetCurrentProcess(), NULL, myName, _countof(myName)))
		CRITICAL_API_FAIL(GetModuleBaseNameW, GetLastError());
	WCHAR targetPath[MAX_PATH];
	if(_snwprintf_s(targetPath, _countof(targetPath), _countof(targetPath)-1, L"%s%s", myPathGlobal, myName) < 1)
		CRITICAL("路径长度错误!");

	if(!CopyFileW(myPath, targetPath, FALSE))
		CRITICAL_API_FAIL(CopyFileW, GetLastError());
	// ------------------------------------------------------------------------

	InstallRegistry();

	// Without this the app icon might not show up in explorer right away
	ResetWindowsIconCache();
}


// Uninstall ourself
// Returns 0 = Needs manual uninstall step, 1 = complete
int Uninstall() {
	RequestAdminPrivileges();

	// Remove our registry key
	DeleteRegistryPath(HKEY_CLASSES_ROOT, REGISTRY_PATH);
	ResetWindowsIconCache();

	// Double check the path to avoid a disaster
	if (wcsstr(myPathGlobal, INSTALL_FOLDER)) {
		// Copy of our path without ending backslash and to be double terminated
		WCHAR myPathCopy[MAX_PATH + 1];
		ZeroMemory(myPathCopy, sizeof(myPathCopy));
		if (wcsncpy_s(myPathCopy, MAX_PATH, myPathGlobal, wcslen(myPathGlobal) - 1) == 0)
		{
			// No, recursively delete our installation folder
			// Note: Might fail (return = 2) while debug stepping, but then fine in release for what ever reason..
			SHFILEOPSTRUCTW nfo =
			{
				NULL,
				FO_DELETE,
				myPathCopy,
				NULL,
				(FOF_NO_UI | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO),
				FALSE,
				NULL,
				NULL
			};
			SHFileOperationW(&nfo);
		}
	}

	// Return 0 if installation folder is still there
	DWORD attr = GetFileAttributesW(myPathGlobal);
	return !((attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY));
}
