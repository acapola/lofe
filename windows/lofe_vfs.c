//compiles with Visual Studio Express 2015 for Windows Desktop.
//Make sure to start it as Adminitrator to be able to use the debugger
//tested on Windows 7 64 bit

#include <windows.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include "dokan/dokan.h"
#include "dokan/fileinfo.h"

#include "lofe_internals.h"



BOOL g_UseStdErr;
BOOL g_DebugMode;

static void DbgPrint(LPCWSTR format, ...)
{
	if (g_DebugMode) {
		WCHAR buffer[512];
		va_list argp;
		va_start(argp, format);
		vswprintf_s(buffer, sizeof(buffer) / sizeof(WCHAR), format, argp);
		va_end(argp);
		if (g_UseStdErr) {
			fwprintf(stderr, buffer);
		}
		else {
			OutputDebugStringW(buffer);
		}
	}
}

static WCHAR RootDirectory[MAX_PATH] = L"C:";
static WCHAR MountPoint[MAX_PATH] = L"M:";


static void
GetFilePath(
	PWCHAR	filePath,
	ULONG	numberOfElements,
	LPCWSTR FileName)
{
	RtlZeroMemory(filePath, numberOfElements * sizeof(WCHAR));
	wcsncpy_s(filePath, numberOfElements, RootDirectory, wcslen(RootDirectory));
	wcsncat_s(filePath, numberOfElements, FileName, wcslen(FileName));
}

int lofe_write_header(lofe_file_handle_t h, lofe_header_t header) {
	LARGE_INTEGER distanceToMove;
	DWORD NumberOfBytesWritten;
	distanceToMove.QuadPart = 0;
	if (!SetFilePointerEx(h, distanceToMove, NULL, FILE_BEGIN)) {
		DbgPrint(L"\tseek error, offset = %d, error = %d\n", 0, GetLastError());
		return -1;
	}
	if (!WriteFile(h, &header, sizeof(header), &NumberOfBytesWritten, NULL)) {
		return -1;
	}
	if (sizeof(header)!=NumberOfBytesWritten) {
		return -1;
	}
	return 0;
}

int lofe_read_header(lofe_file_handle_t h, lofe_header_t *header) {
	LARGE_INTEGER distanceToMove;
	DWORD ReadLength;
	distanceToMove.QuadPart = 0;
	if (!SetFilePointerEx(h, distanceToMove, NULL, FILE_BEGIN)) {
		return -1;
	}
	if (!ReadFile(h, header, sizeof(lofe_header_t), &ReadLength, NULL)) {
		return -1;
	}
	return 0;
}

static int read_header2(LPCWSTR native_path, lofe_header_t *header) {
	DWORD ReadLength;

	HANDLE handle = CreateFile(
		native_path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		return -1;
	}
	
	if (!ReadFile(handle, header, sizeof(lofe_header_t), &ReadLength, NULL)) {
		return -1;
	}

	CloseHandle(handle);
	return 0;
}


static int isRealFile(DWORD dwFileAttributes) {
	if (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;
	if (dwFileAttributes & FILE_ATTRIBUTE_DEVICE) return 0;
	if (dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL) return 0;
	//we have a real file
	return 1;
}


static int fixFileSize(WIN32_FIND_DATAW *findData) {
	lofe_header_t header;

	if (isRealFile(findData->dwFileAttributes)) {
		WCHAR				filePath[MAX_PATH];
		GetFilePath(filePath, MAX_PATH, L"\\");
		wcscat_s(filePath, MAX_PATH, findData->cFileName);

		DbgPrint(L"\tread_header2\n");
		if (0 != read_header2(filePath, &header)) {
			DbgPrint(L"\tread_header2 failed: error code = %d\n\n", GetLastError());
			return -1;
		}
		findData->nFileSizeHigh = (uint32_t)(header.content_len >> 32);
		findData->nFileSizeLow = (uint32_t)header.content_len;
	}
	return 0;
}

static int fixFileSize2(LPCWSTR	filePath, LPBY_HANDLE_FILE_INFORMATION findData) {
	lofe_header_t header;

	if (isRealFile(findData->dwFileAttributes)) {
		DbgPrint(L"\tread_header2\n");
		if (0 != read_header2(filePath, &header)) {
			DbgPrint(L"\tread_header2 failed: error code = %d\n\n", GetLastError());
			return -1;
		}
		findData->nFileSizeHigh = header.content_len >> 32;
		findData->nFileSizeLow = (uint32_t)header.content_len;
	}
	return 0;
}


int lofe_read_block(lofe_file_handle_t h, int8_t*buf, off_t offset, const lofe_header_t * const header) {
	LARGE_INTEGER distanceToMove;
	DWORD ReadLength;
	int8_t enc_buf[BLOCK_SIZE];
	if (offset%BLOCK_SIZE) {
		printf("ERROR: lofe_read_block, unaligned offset: %X\n", offset);
		return -EINVAL;
	}
	distanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(h, distanceToMove, NULL, FILE_BEGIN)) {
		DbgPrint(L"\tseek error, offset = %d, error = %d\n", offset, GetLastError());
		return -1;
	}
	if (!ReadFile(h, buf, BLOCK_SIZE, &ReadLength, NULL)) {
		DbgPrint(L"\tlofe_read_block error = %u, buffer length = %d, read length = %d\n\n",
			GetLastError(), BLOCK_SIZE, ReadLength);
		return -1;
	}
	else {
		DbgPrint(L"\tlofe_read_block %d, offset %d\n\n", ReadLength, offset);
	}
	if (ReadLength != BLOCK_SIZE) {
		printf("ERROR: lofe_read_block, could not read all the requested bytes\n");
		return -EIO;
	}
	lofe_decrypt_block(buf, enc_buf, header->iv, offset);
	return BLOCK_SIZE;
}

int lofe_write_block(lofe_file_handle_t h, const int8_t * const buf, off_t offset, const lofe_header_t * const header) {
	LARGE_INTEGER distanceToMove;
	DWORD NumberOfBytesWritten;
	int8_t enc_buf[BLOCK_SIZE];
	if (offset%BLOCK_SIZE) {
		printf("ERROR: lofe_write_block, unaligned offset: %X\n", offset);
		return -EINVAL;
	}
	lofe_encrypt_block(enc_buf, buf, header->iv, offset);
	distanceToMove.QuadPart = offset;
	if (!SetFilePointerEx(h, distanceToMove, NULL, FILE_BEGIN)) {
		DbgPrint(L"\tseek error, offset = %d, error = %d\n", offset, GetLastError());
		return -1;
	}
	if (!WriteFile(h, buf, BLOCK_SIZE, &NumberOfBytesWritten, NULL)) {
		DbgPrint(L"\twrite error = %u, buffer length = %d, write length = %d\n",
			GetLastError(), BLOCK_SIZE, NumberOfBytesWritten);
		return -1;
	}
	else {
		DbgPrint(L"\twrite %d, offset %d\n\n", NumberOfBytesWritten, offset);
	}
	if (NumberOfBytesWritten != BLOCK_SIZE) {
		printf("ERROR: lofe_write_block, could not write all the requested bytes\n");
		return -EIO;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////


static void
PrintUserName(PDOKAN_FILE_INFO	DokanFileInfo)
{
	HANDLE	handle;
	UCHAR buffer[1024];
	DWORD returnLength;
	WCHAR accountName[256];
	WCHAR domainName[256];
	DWORD accountLength = sizeof(accountName) / sizeof(WCHAR);
	DWORD domainLength = sizeof(domainName) / sizeof(WCHAR);
	PTOKEN_USER tokenUser;
	SID_NAME_USE snu;

	handle = DokanOpenRequestorToken(DokanFileInfo);
	if (handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"  DokanOpenRequestorToken failed\n");
		return;
	}

	if (!GetTokenInformation(handle, TokenUser, buffer, sizeof(buffer), &returnLength)) {
		DbgPrint(L"  GetTokenInformaiton failed: %d\n", GetLastError());
		CloseHandle(handle);
		return;
	}

	CloseHandle(handle);

	tokenUser = (PTOKEN_USER)buffer;
	if (!LookupAccountSid(NULL, tokenUser->User.Sid, accountName,
			&accountLength, domainName, &domainLength, &snu)) {
		DbgPrint(L"  LookupAccountSid failed: %d\n", GetLastError());
		return;
	}

	DbgPrint(L"  AccountName: %s, DomainName: %s\n", accountName, domainName);
}

#define LofeCheckFlag(val, flag) if (val&flag) { DbgPrint(L"\t" L#flag L"\n"); }

static int DOKAN_CALLBACK
LofeCreateFile(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	HANDLE handle;
	DWORD fileAttr;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"CreateFile : %s\n", filePath);

	PrintUserName(DokanFileInfo);

	if (CreationDisposition == CREATE_NEW)
		DbgPrint(L"\tCREATE_NEW\n");
	if (CreationDisposition == OPEN_ALWAYS)
		DbgPrint(L"\tOPEN_ALWAYS\n");
	if (CreationDisposition == CREATE_ALWAYS)
		DbgPrint(L"\tCREATE_ALWAYS\n");
	if (CreationDisposition == OPEN_EXISTING)
		DbgPrint(L"\tOPEN_EXISTING\n");
	if (CreationDisposition == TRUNCATE_EXISTING)
		DbgPrint(L"\tTRUNCATE_EXISTING\n");

	/*
	if (ShareMode == 0 && AccessMode & FILE_WRITE_DATA)
		ShareMode = FILE_SHARE_WRITE;
	else if (ShareMode == 0)
		ShareMode = FILE_SHARE_READ;
	*/

	DbgPrint(L"\tShareMode = 0x%x\n", ShareMode);

	LofeCheckFlag(ShareMode, FILE_SHARE_READ);
	LofeCheckFlag(ShareMode, FILE_SHARE_WRITE);
	LofeCheckFlag(ShareMode, FILE_SHARE_DELETE);

	DbgPrint(L"\tAccessMode = 0x%x\n", AccessMode);

	LofeCheckFlag(AccessMode, GENERIC_READ);
	LofeCheckFlag(AccessMode, GENERIC_WRITE);
	LofeCheckFlag(AccessMode, GENERIC_EXECUTE);
	
	LofeCheckFlag(AccessMode, DELETE);
	LofeCheckFlag(AccessMode, FILE_READ_DATA);
	LofeCheckFlag(AccessMode, FILE_READ_ATTRIBUTES);
	LofeCheckFlag(AccessMode, FILE_READ_EA);
	LofeCheckFlag(AccessMode, READ_CONTROL);
	LofeCheckFlag(AccessMode, FILE_WRITE_DATA);
	LofeCheckFlag(AccessMode, FILE_WRITE_ATTRIBUTES);
	LofeCheckFlag(AccessMode, FILE_WRITE_EA);
	LofeCheckFlag(AccessMode, FILE_APPEND_DATA);
	LofeCheckFlag(AccessMode, WRITE_DAC);
	LofeCheckFlag(AccessMode, WRITE_OWNER);
	LofeCheckFlag(AccessMode, SYNCHRONIZE);
	LofeCheckFlag(AccessMode, FILE_EXECUTE);
	LofeCheckFlag(AccessMode, STANDARD_RIGHTS_READ);
	LofeCheckFlag(AccessMode, STANDARD_RIGHTS_WRITE);
	LofeCheckFlag(AccessMode, STANDARD_RIGHTS_EXECUTE);

	// When filePath is a directory, needs to change the flag so that the file can be opened.
	fileAttr = GetFileAttributes(filePath);
	if (fileAttr && fileAttr & FILE_ATTRIBUTE_DIRECTORY) {
		FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
		//AccessMode = 0;
	}
	DbgPrint(L"\tFlagsAndAttributes = 0x%x\n", FlagsAndAttributes);

	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ARCHIVE);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ENCRYPTED);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_HIDDEN);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NORMAL);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_OFFLINE);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_READONLY);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_SYSTEM);
	LofeCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_TEMPORARY);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_WRITE_THROUGH);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_OVERLAPPED);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_NO_BUFFERING);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_RANDOM_ACCESS);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_SEQUENTIAL_SCAN);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_DELETE_ON_CLOSE);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_BACKUP_SEMANTICS);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_POSIX_SEMANTICS);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_REPARSE_POINT);
	LofeCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_NO_RECALL);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_ANONYMOUS);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_IDENTIFICATION);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_IMPERSONATION);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_DELEGATION);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_CONTEXT_TRACKING);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_EFFECTIVE_ONLY);
	LofeCheckFlag(FlagsAndAttributes, SECURITY_SQOS_PRESENT);

	handle = CreateFile(
		filePath,
		AccessMode,//GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
		ShareMode,
		NULL, // security attribute
		CreationDisposition,
		FlagsAndAttributes,// |FILE_FLAG_NO_BUFFERING,
		NULL); // template file handle

	if (handle == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1; // error codes are negated value of Windows System Error codes
	}

	DbgPrint(L"\n");

	// save the file handle in Context
	DokanFileInfo->Context = (ULONG64)handle;
	return 0;
}

static int DOKAN_CALLBACK
LofeReadFile(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	ULONG	offset = (ULONG)Offset;
	BOOL	opened = FALSE;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"ReadFile : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle, cleanuped?\n");
		handle = CreateFile(
			filePath,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			DbgPrint(L"\tCreateFile error : %d\n\n", GetLastError());
			return -1;
		}
		opened = TRUE;
	}
	
	*ReadLength = lofe_read(handle, Buffer, BufferLength, Offset);
	if (-1==*ReadLength) {
		DbgPrint(L"\tread error = %u, buffer length = %d, ReadLength %d\n\n",
			GetLastError(), BufferLength, *ReadLength);
		if (opened)
			CloseHandle(handle);
		return -1;

	} else {
		DbgPrint(L"\tread %d, offset %d\n\n", *ReadLength, offset);
	}

	if (opened)
		CloseHandle(handle);

	return 0;
}


static int DOKAN_CALLBACK
LofeWriteFile(
	LPCWSTR		FileName,
	LPCVOID		Buffer,
	DWORD		NumberOfBytesToWrite,
	LPDWORD		NumberOfBytesWritten,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	ULONG	offset = (ULONG)Offset;
	BOOL	opened = FALSE;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"WriteFile : %s, offset %I64d, length %d\n", filePath, Offset, NumberOfBytesToWrite);

	// reopen the file
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle, cleanuped?\n");
		handle = CreateFile(
			filePath,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			DbgPrint(L"\tCreateFile error : %d\n\n", GetLastError());
			return -1;
		}
		opened = TRUE;
	}

    LARGE_INTEGER distanceToMove;
    distanceToMove.QuadPart = Offset;

	if (DokanFileInfo->WriteToEndOfFile) {
        LARGE_INTEGER z;
        z.QuadPart = 0;
		if (!SetFilePointerEx(handle, z, NULL, FILE_END)) {
			DbgPrint(L"\tseek error, offset = EOF, error = %d\n", GetLastError());
			return -1;
		}
    }
    else if (!SetFilePointerEx(handle, distanceToMove, NULL, FILE_BEGIN)) {
		DbgPrint(L"\tseek error, offset = %d, error = %d\n", offset, GetLastError());
		return -1;
	}

		
	if (!WriteFile(handle, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, NULL)) {
		DbgPrint(L"\twrite error = %u, buffer length = %d, write length = %d\n",
			GetLastError(), NumberOfBytesToWrite, *NumberOfBytesWritten);
		return -1;

	} else {
		DbgPrint(L"\twrite %d, offset %d\n\n", *NumberOfBytesWritten, offset);
	}

	// close the file when it is reopened
	if (opened)
		CloseHandle(handle);

	return 0;
}



static int DOKAN_CALLBACK
LofeGetFileInformation(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	BOOL	opened = FALSE;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"GetFileInfo : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");

		// If CreateDirectory returned FILE_ALREADY_EXISTS and 
		// it is called with FILE_OPEN_IF, that handle must be opened.
		handle = CreateFile(filePath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return -1;
		opened = TRUE;
	}

	if (!GetFileInformationByHandle(handle,HandleFileInformation)) {
		DbgPrint(L"\terror code = %d\n", GetLastError());

		// FileName is a root directory
		// in this case, FindFirstFile can't get directory information
		if (wcslen(FileName) == 1) {
			DbgPrint(L"  root dir\n");
			HandleFileInformation->dwFileAttributes = GetFileAttributes(filePath);

		} else {
			WIN32_FIND_DATAW find;
			ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
			handle = FindFirstFile(filePath, &find);
			if (handle == INVALID_HANDLE_VALUE) {
				DbgPrint(L"\tFindFirstFile error code = %d\n\n", GetLastError());
				return -1;
			}
			HandleFileInformation->dwFileAttributes = find.dwFileAttributes;
			HandleFileInformation->ftCreationTime = find.ftCreationTime;
			HandleFileInformation->ftLastAccessTime = find.ftLastAccessTime;
			HandleFileInformation->ftLastWriteTime = find.ftLastWriteTime;
			HandleFileInformation->nFileSizeHigh = find.nFileSizeHigh;
			HandleFileInformation->nFileSizeLow = find.nFileSizeLow;
			FindClose(handle);
			fixFileSize2(filePath,HandleFileInformation);
			DbgPrint(L"\tFindFiles OK, file size = %d\n", find.nFileSizeLow);
		}
	} else {
		DbgPrint(L"\tHandleFileInformation->dwFileAttributes = 0x%08X\n", HandleFileInformation->dwFileAttributes);
		fixFileSize2(filePath, HandleFileInformation);
		DbgPrint(L"\tGetFileInformationByHandle success, file size = %d\n",
			HandleFileInformation->nFileSizeLow);
	}

	DbgPrint(L"\n");

	if (opened) {
		CloseHandle(handle);
	}

	return 0;
}


static int DOKAN_CALLBACK
LofeFindFiles(
	LPCWSTR				FileName,
	PFillFindData		FillFindData, // function pointer
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR				filePath[MAX_PATH];
	HANDLE				hFind;
	WIN32_FIND_DATAW	findData;
	DWORD				error;
	PWCHAR				yenStar = L"\\*";
	int count = 0;

	GetFilePath(filePath, MAX_PATH, FileName);

	wcscat_s(filePath, MAX_PATH, yenStar);
	DbgPrint(L"FindFiles :%s\n", filePath);

	hFind = FindFirstFile(filePath, &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid file handle. Error is %u\n\n", GetLastError());
		return -1;
	}
	fixFileSize(&findData);
	FillFindData(&findData, DokanFileInfo);
	count++;

	while (FindNextFile(hFind, &findData) != 0) {
		fixFileSize(&findData);
		FillFindData(&findData, DokanFileInfo);
		count++;
	}
	
	error = GetLastError();
	FindClose(hFind);

	if (error != ERROR_NO_MORE_FILES) {
		DbgPrint(L"\tFindNextFile error. Error is %u\n\n", error);
		return -1;
	}

	DbgPrint(L"\tFindFiles return %d entries in %s\n\n", count, filePath);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////


static int DOKAN_CALLBACK
LofeCreateDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	UNREFERENCED_PARAMETER(DokanFileInfo);

	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"CreateDirectory : %s\n", filePath);
	if (!CreateDirectory(filePath, NULL)) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1; // error codes are negated value of Windows System Error codes
	}
	return 0;
}


static int DOKAN_CALLBACK
LofeOpenDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	HANDLE handle;
	DWORD attr;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"OpenDirectory : %s\n", filePath);

	attr = GetFileAttributes(filePath);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}
	if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		return -1;
	}

	handle = CreateFile(
		filePath,
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}

	DbgPrint(L"\n");

	DokanFileInfo->Context = (ULONG64)handle;

	return 0;
}


static int DOKAN_CALLBACK
LofeCloseFile(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, MAX_PATH, FileName);

	if (DokanFileInfo->Context) {
		DbgPrint(L"CloseFile: %s\n", filePath);
		DbgPrint(L"\terror : not cleanuped file\n\n");
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;
	}
	else {
		//DbgPrint(L"Close: %s\n\tinvalid handle\n\n", filePath);
		DbgPrint(L"Close: %s\n\n", filePath);
		return 0;
	}

	//DbgPrint(L"\n");
	return 0;
}


static int DOKAN_CALLBACK
LofeCleanup(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, MAX_PATH, FileName);

	if (DokanFileInfo->Context) {
		DbgPrint(L"Cleanup: %s\n\n", filePath);
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;

		if (DokanFileInfo->DeleteOnClose) {
			DbgPrint(L"\tDeleteOnClose\n");
			if (DokanFileInfo->IsDirectory) {
				DbgPrint(L"  DeleteDirectory ");
				if (!RemoveDirectory(filePath)) {
					DbgPrint(L"error code = %d\n\n", GetLastError());
				}
				else {
					DbgPrint(L"success\n\n");
				}
			}
			else {
				DbgPrint(L"  DeleteFile ");
				if (DeleteFile(filePath) == 0) {
					DbgPrint(L" error code = %d\n\n", GetLastError());
				}
				else {
					DbgPrint(L"success\n\n");
				}
			}
		}

	}
	else {
		DbgPrint(L"Cleanup: %s\n\tinvalid handle\n\n", filePath);
		return -1;
	}

	return 0;
}

static int DOKAN_CALLBACK
LofeFlushFileBuffers(
	LPCWSTR		FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"FlushFileBuffers : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return 0;
	}

	if (FlushFileBuffers(handle)) {
		return 0;
	}
	else {
		DbgPrint(L"\tflush error code = %d\n", GetLastError());
		return -1;
	}

}



static int DOKAN_CALLBACK
LofeDeleteFile(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
    UNREFERENCED_PARAMETER(DokanFileInfo);

	WCHAR	filePath[MAX_PATH];
	//HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"DeleteFile %s\n", filePath);

	return 0;
}


static int DOKAN_CALLBACK
LofeDeleteDirectory(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
    UNREFERENCED_PARAMETER(DokanFileInfo);

	WCHAR	filePath[MAX_PATH];
	//HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	HANDLE	hFind;
	WIN32_FIND_DATAW	findData;
	size_t	fileLen;

	ZeroMemory(filePath, sizeof(filePath));
	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"DeleteDirectory %s\n", filePath);

	fileLen = wcslen(filePath);
	if (filePath[fileLen-1] != L'\\') {
		filePath[fileLen++] = L'\\';
	}
	filePath[fileLen] = L'*';

	hFind = FindFirstFile(filePath, &findData);
	while (hFind != INVALID_HANDLE_VALUE) {
		if (wcscmp(findData.cFileName, L"..") != 0 &&
			wcscmp(findData.cFileName, L".") != 0) {
			FindClose(hFind);
			DbgPrint(L"  Directory is not empty: %s\n", findData.cFileName);
			return -(int)ERROR_DIR_NOT_EMPTY;
		}
		if (!FindNextFile(hFind, &findData)) {
			break;
		}
	}
	FindClose(hFind);

	if (GetLastError() == ERROR_NO_MORE_FILES) {
		return 0;
	} else {
		return -1;
	}
}


static int DOKAN_CALLBACK
LofeMoveFile(
	LPCWSTR				FileName, // existing file name
	LPCWSTR				NewFileName,
	BOOL				ReplaceIfExisting,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR			filePath[MAX_PATH];
	WCHAR			newFilePath[MAX_PATH];
	BOOL			status;

	GetFilePath(filePath, MAX_PATH, FileName);
	GetFilePath(newFilePath, MAX_PATH, NewFileName);

	DbgPrint(L"MoveFile %s -> %s\n\n", filePath, newFilePath);

	if (DokanFileInfo->Context) {
		// should close? or rename at closing?
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;
	}

	if (ReplaceIfExisting)
		status = MoveFileEx(filePath, newFilePath, MOVEFILE_REPLACE_EXISTING);
	else
		status = MoveFile(filePath, newFilePath);

	if (status == FALSE) {
		DWORD error = GetLastError();
		DbgPrint(L"\tMoveFile failed status = %d, code = %d\n", status, error);
		return -(int)error;
	} else {
		return 0;
	}
}


static int DOKAN_CALLBACK
LofeLockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;
	LARGE_INTEGER offset;
	LARGE_INTEGER length;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"LockFile %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	length.QuadPart = Length;
	offset.QuadPart = ByteOffset;

	if (LockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
		DbgPrint(L"\tsuccess\n\n");
		return 0;
	} else {
		DbgPrint(L"\tfail\n\n");
		return -1;
	}
}


static int DOKAN_CALLBACK
LofeSetEndOfFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR			filePath[MAX_PATH];
	HANDLE			handle;
	LARGE_INTEGER	offset;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"SetEndOfFile %s, %I64d\n", filePath, ByteOffset);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	offset.QuadPart = ByteOffset;
	if (!SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)) {
		DbgPrint(L"\tSetFilePointer error: %d, offset = %I64d\n\n",
				GetLastError(), ByteOffset);
		return GetLastError() * -1;
	}

	if (!SetEndOfFile(handle)) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}

	return 0;
}


static int DOKAN_CALLBACK
LofeSetAllocationSize(
	LPCWSTR				FileName,
	LONGLONG			AllocSize,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR			filePath[MAX_PATH];
	HANDLE			handle;
	LARGE_INTEGER	fileSize;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"SetAllocationSize %s, %I64d\n", filePath, AllocSize);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	if (GetFileSizeEx(handle, &fileSize)) {
		if (AllocSize < fileSize.QuadPart) {
			fileSize.QuadPart = AllocSize;
			if (!SetFilePointerEx(handle, fileSize, NULL, FILE_BEGIN)) {
				DbgPrint(L"\tSetAllocationSize: SetFilePointer eror: %d, "
					L"offset = %I64d\n\n", GetLastError(), AllocSize);
				return GetLastError() * -1;
			}
			if (!SetEndOfFile(handle)) {
				DWORD error = GetLastError();
				DbgPrint(L"\terror code = %d\n\n", error);
				return error * -1;
			}
		}
	} else {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}
	return 0;
}


static int DOKAN_CALLBACK
LofeSetFileAttributes(
	LPCWSTR				FileName,
	DWORD				FileAttributes,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
    UNREFERENCED_PARAMETER(DokanFileInfo);

	WCHAR	filePath[MAX_PATH];
	
	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"SetFileAttributes %s\n", filePath);

	if (!SetFileAttributes(filePath, FileAttributes)) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}

	DbgPrint(L"\n");
	return 0;
}


static int DOKAN_CALLBACK
LofeSetFileTime(
	LPCWSTR				FileName,
	CONST FILETIME*		CreationTime,
	CONST FILETIME*		LastAccessTime,
	CONST FILETIME*		LastWriteTime,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"SetFileTime %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	if (!SetFileTime(handle, CreationTime, LastAccessTime, LastWriteTime)) {
		DWORD error = GetLastError();
		DbgPrint(L"\terror code = %d\n\n", error);
		return error * -1;
	}

	DbgPrint(L"\n");
	return 0;
}


static int DOKAN_CALLBACK
LofeUnlockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;
	LARGE_INTEGER	length;
	LARGE_INTEGER	offset;

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"UnlockFile %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	length.QuadPart = Length;
	offset.QuadPart = ByteOffset;

	if (UnlockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
		DbgPrint(L"\tsuccess\n\n");
		return 0;
	} else {
		DbgPrint(L"\tfail\n\n");
		return -1;
	}
}


static int DOKAN_CALLBACK
LofeGetFileSecurity(
	LPCWSTR					FileName,
	PSECURITY_INFORMATION	SecurityInformation,
	PSECURITY_DESCRIPTOR	SecurityDescriptor,
	ULONG				BufferLength,
	PULONG				LengthNeeded,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	HANDLE	handle;
	WCHAR	filePath[MAX_PATH];

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"GetFileSecurity %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	if (!GetUserObjectSecurity(handle, SecurityInformation, SecurityDescriptor,
			BufferLength, LengthNeeded)) {
		int error = GetLastError();
		if (error == ERROR_INSUFFICIENT_BUFFER) {
			DbgPrint(L"  GetUserObjectSecurity failed: ERROR_INSUFFICIENT_BUFFER\n");
			return error * -1;
		} else {
			DbgPrint(L"  GetUserObjectSecurity failed: %d\n", error);
			return -1;
		}
	}
	return 0;
}


static int DOKAN_CALLBACK
LofeSetFileSecurity(
	LPCWSTR					FileName,
	PSECURITY_INFORMATION	SecurityInformation,
	PSECURITY_DESCRIPTOR	SecurityDescriptor,
	ULONG				SecurityDescriptorLength,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	HANDLE	handle;
	WCHAR	filePath[MAX_PATH];

	UNREFERENCED_PARAMETER(SecurityDescriptorLength);

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"SetFileSecurity %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	if (!SetUserObjectSecurity(handle, SecurityInformation, SecurityDescriptor)) {
		int error = GetLastError();
		DbgPrint(L"  SetUserObjectSecurity failed: %d\n", error);
		return -1;
	}
	return 0;
}

static int DOKAN_CALLBACK
LofeGetVolumeInformation(
	LPWSTR		VolumeNameBuffer,
	DWORD		VolumeNameSize,
	LPDWORD		VolumeSerialNumber,
	LPDWORD		MaximumComponentLength,
	LPDWORD		FileSystemFlags,
	LPWSTR		FileSystemNameBuffer,
	DWORD		FileSystemNameSize,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
    UNREFERENCED_PARAMETER(DokanFileInfo);

	wcscpy_s(VolumeNameBuffer, VolumeNameSize, L"DOKAN");
	*VolumeSerialNumber = 0x19831116;
	*MaximumComponentLength = 256;
	*FileSystemFlags = FILE_CASE_SENSITIVE_SEARCH | 
						FILE_CASE_PRESERVED_NAMES | 
						FILE_SUPPORTS_REMOTE_STORAGE |
						FILE_UNICODE_ON_DISK |
						FILE_PERSISTENT_ACLS;

	wcscpy_s(FileSystemNameBuffer, FileSystemNameSize, L"Dokan");

	return 0;
}


static int DOKAN_CALLBACK
LofeUnmount(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
    UNREFERENCED_PARAMETER(DokanFileInfo);

	DbgPrint(L"Unmount\n");
	return 0;
}

/**
 * Avoid #include <winternl.h> which as conflict with FILE_INFORMATION_CLASS definition.
 * This only for LofeEnumerateNamedStreams. Link with ntdll.lib still required.
 * 
 * Not needed if you're not using NtQueryInformationFile!
 *
 * BEGIN
 */
typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	} DUMMYUNIONNAME;

	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

NTSYSCALLAPI NTSTATUS NTAPI 	NtQueryInformationFile(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _Out_writes_bytes_(Length) PVOID FileInformation, _In_ ULONG Length, _In_ FILE_INFORMATION_CLASS FileInformationClass);
/**
 * END
 */

static int DOKAN_CALLBACK
LofeEnumerateNamedStreams(
	LPCWSTR					FileName,
	PVOID*					EnumContext,
	LPWSTR					StreamName,
	PULONG					StreamNameLength,
	PLONGLONG				StreamSize,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	HANDLE	handle;
	WCHAR	filePath[MAX_PATH];

	GetFilePath(filePath, MAX_PATH, FileName);

	DbgPrint(L"EnumerateNamedStreams %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		DbgPrint(L"\tinvalid handle\n\n");
		return -1;
	}

	// As we are requested one by one, it would be better to use FindFirstStream / FindNextStream instead of requesting all streams each time
	// But this doesn't really matter on Lofe sample
	BYTE InfoBlock[64 * 1024];
	PFILE_STREAM_INFORMATION pStreamInfo = (PFILE_STREAM_INFORMATION)InfoBlock;
	IO_STATUS_BLOCK ioStatus;
	ZeroMemory(InfoBlock, sizeof(InfoBlock));

	NTSTATUS status = NtQueryInformationFile(handle, &ioStatus, InfoBlock, sizeof(InfoBlock), FileStreamInformation);
	if (status != STATUS_SUCCESS) {
		DbgPrint(L"\tNtQueryInformationFile failed with %d.\n", status);
		return -1;
	}

	if (pStreamInfo->StreamNameLength == 0) {
		DbgPrint(L"\tNo stream found.\n");
		return -1;
	}

	UINT index = (UINT)*EnumContext;
	DbgPrint(L"\tStream #%d requested.\n", index);
	
	for (UINT i = 0; i != index; ++i) {
		if (pStreamInfo->NextEntryOffset == 0) {
			DbgPrint(L"\tNo more stream.\n");
			return -1;
		}
		pStreamInfo = (PFILE_STREAM_INFORMATION) ((LPBYTE)pStreamInfo + pStreamInfo->NextEntryOffset);   // Next stream record
	}

	wcscpy_s(StreamName, SHRT_MAX + 1, pStreamInfo->StreamName);
	*StreamNameLength = pStreamInfo->StreamNameLength;
	*StreamSize = pStreamInfo->StreamSize.QuadPart;

	DbgPrint(L"\t Stream %ws\n", pStreamInfo->StreamName);

	// Remember next stream entry index
	*EnumContext = (PVOID)++index;

	return 0;
}


int __cdecl
dokan_wmain(ULONG argc, PWCHAR argv[])
{
	int status;
	ULONG command;
	PDOKAN_OPERATIONS dokanOperations =
			(PDOKAN_OPERATIONS)malloc(sizeof(DOKAN_OPERATIONS));
	if (dokanOperations == NULL) {
		return -1;
	}
	PDOKAN_OPTIONS dokanOptions =
			(PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));
	if (dokanOptions == NULL) {
		free(dokanOperations);
		return -1;
	}

	if (argc < 5) {
		fprintf(stderr, "Lofe.exe\n"
			"  /r RootDirectory (ex. /r c:\\test)\n"
			"  /l DriveLetter (ex. /l m)\n"
			"  /t ThreadCount (ex. /t 5)\n"
			"  /d (enable debug output)\n"
			"  /s (use stderr for output)\n"
			"  /n (use network drive)\n"
			"  /m (use removable drive)\n");
		return -1;
	}

	g_DebugMode = FALSE;
	g_UseStdErr = FALSE;

	ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));
	dokanOptions->Version = DOKAN_VERSION;
	dokanOptions->ThreadCount = 0; // use default

	for (command = 1; command < argc; command++) {
		switch (towlower(argv[command][1])) {
		case L'r':
			command++;
			wcscpy_s(RootDirectory, sizeof(RootDirectory)/sizeof(WCHAR), argv[command]);
			DbgPrint(L"RootDirectory: %ls\n", RootDirectory);
			break;
		case L'l':
			command++;
			wcscpy_s(MountPoint, sizeof(MountPoint)/sizeof(WCHAR), argv[command]);
			dokanOptions->MountPoint = MountPoint;
			break;
		case L't':
			command++;
			dokanOptions->ThreadCount = (USHORT)_wtoi(argv[command]);
			break;
		case L'd':
			g_DebugMode = TRUE;
			break;
		case L's':
			g_UseStdErr = TRUE;
			break;
		case L'n':
			dokanOptions->Options |= DOKAN_OPTION_NETWORK;
			break;
		case L'm':
			dokanOptions->Options |= DOKAN_OPTION_REMOVABLE;
			break;
		default:
			fwprintf(stderr, L"unknown command: %s\n", argv[command]);
			return -1;
		}
	}

	if (g_DebugMode) {
		dokanOptions->Options |= DOKAN_OPTION_DEBUG;
	}
	if (g_UseStdErr) {
		dokanOptions->Options |= DOKAN_OPTION_STDERR;
	}

	dokanOptions->Options |= DOKAN_OPTION_KEEP_ALIVE;
	dokanOptions->Options |= DOKAN_OPTION_ALT_STREAM;

	ZeroMemory(dokanOperations, sizeof(DOKAN_OPERATIONS));
	dokanOperations->CreateFile = LofeCreateFile;
	dokanOperations->OpenDirectory = LofeOpenDirectory;
	dokanOperations->CreateDirectory = LofeCreateDirectory;
	dokanOperations->Cleanup = LofeCleanup;
	dokanOperations->CloseFile = LofeCloseFile;
	dokanOperations->ReadFile = LofeReadFile;
	dokanOperations->WriteFile = LofeWriteFile;
	dokanOperations->FlushFileBuffers = LofeFlushFileBuffers;
	dokanOperations->GetFileInformation = LofeGetFileInformation;
	dokanOperations->FindFiles = LofeFindFiles;
	dokanOperations->FindFilesWithPattern = NULL;
	dokanOperations->SetFileAttributes = LofeSetFileAttributes;
	dokanOperations->SetFileTime = LofeSetFileTime;
	dokanOperations->DeleteFile = LofeDeleteFile;
	dokanOperations->DeleteDirectory = LofeDeleteDirectory;
	dokanOperations->MoveFile = LofeMoveFile;
	dokanOperations->SetEndOfFile = LofeSetEndOfFile;
	dokanOperations->SetAllocationSize = LofeSetAllocationSize;	
	dokanOperations->LockFile = LofeLockFile;
	dokanOperations->UnlockFile = LofeUnlockFile;
	dokanOperations->GetFileSecurity = LofeGetFileSecurity;
	dokanOperations->SetFileSecurity = LofeSetFileSecurity;
	dokanOperations->GetDiskFreeSpace = NULL;
	dokanOperations->GetVolumeInformation = LofeGetVolumeInformation;
	dokanOperations->Unmount = LofeUnmount;
	dokanOperations->EnumerateNamedStreams = LofeEnumerateNamedStreams;

	status = DokanMain(dokanOptions, dokanOperations);
	switch (status) {
	case DOKAN_SUCCESS:
		fprintf(stderr, "Success\n");
		break;
	case DOKAN_ERROR:
		fprintf(stderr, "Error\n");
		break;
	case DOKAN_DRIVE_LETTER_ERROR:
		fprintf(stderr, "Bad Drive letter\n");
		break;
	case DOKAN_DRIVER_INSTALL_ERROR:
		fprintf(stderr, "Can't install driver\n");
		break;
	case DOKAN_START_ERROR:
		fprintf(stderr, "Driver something wrong\n");
		break;
	case DOKAN_MOUNT_ERROR:
		fprintf(stderr, "Can't assign a drive letter\n");
		break;
	case DOKAN_MOUNT_POINT_ERROR:
		fprintf(stderr, "Mount point error\n");
		break;
	default:
		fprintf(stderr, "Unknown error: %d\n", status);
		break;
	}

	free(dokanOptions);
	free(dokanOperations);
	return 0;
}

int lofe_start_vfs(PWCHAR encrypted_files_path, PWCHAR mount_point) {
	PWCHAR fuse_argv[100];
	int argc = 2;
	PWCHAR argv[2];
	int fuse_argc = argc + 4;
	int i;
	int res;
	argv[0] = L"lofe";
	argv[1] = L"/d";

	//dokan command line arguments:    
	for (i = 0; i<argc; i++) fuse_argv[i] = argv[i];
	fuse_argv[argc + 0] = L"/r";
	fuse_argv[argc + 1] = encrypted_files_path;
	fuse_argv[argc + 2] = L"/l";
	fuse_argv[argc + 3] = mount_point;

	res = dokan_wmain(fuse_argc, fuse_argv);
	return res;
}

int __cdecl wmain(ULONG argc, PWCHAR argv[]) {
	uint8_t key_bytes[] = { 0xC5, 0xE6, 0x67, 0xEE, 0x10, 0x97, 0x19, 0x74, 0xDA, 0xC5, 0x52, 0x65, 0x26, 0x01, 0x77, 0x05 };
	uint8_t key_tweak_bytes[] = { 0x59, 0xA1, 0x24, 0xFC, 0x02, 0x55, 0x41, 0xB6, 0xDE, 0x38, 0x9A, 0x47, 0xEF, 0x25, 0x90, 0xDF };
	int res;
	if (argc<3) {
		printf("LOFE: Local On-the-Fly Encryption tool\n");
		printf("2015, Sebastien Riou, lofe@nimp.co.uk\n");
		printf("\n");
		printf("Usage:\n");
		printf("lofe <encrypted_files_path> <mount_point>\n");
		printf("\n");
		exit(-1);
	}
	PWCHAR encrypted_files_path = argv[1];
	PWCHAR mount_point = argv[2];

	int aes_self_test_result = aes_self_test128();
	if (aes_self_test_result) {
		printf("aes self test failed with error code: %d\n", aes_self_test_result);
		exit(-1);
	}

	//secret key
	lofe_load_key((int8_t*)key_bytes, (int8_t*)key_tweak_bytes);

	res = lofe_start_vfs(encrypted_files_path, mount_point);
	return res;
}
