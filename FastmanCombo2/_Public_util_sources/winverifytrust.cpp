#include "winverifytrust.h"
#include <windows.h>
#include <stdio.h>
#include <wintrust.h>
#include <softpub.h>
#include <mscat.h>
#include <QDebug>
#include <QMap>
#include <QFile>

#pragma comment(lib, "wintrust.lib")


void PrintError(_In_ DWORD Status)
{
#if 0
    char szbuf[128];
    sprintf(szbuf, "Error: 0x%08x (%d)\n", Status, Status);
    qDebug() << szbuf;
#endif
}

// vs2010 编译器需定义如下结构, vs2012 或许不需要
#ifndef WSS_VERIFY_SPECIFIC
#define WSS_VERIFY_SPECIFIC 0x00000001
#define WSS_GET_SECONDARY_SIG_COUNT 0x00000002

typedef struct _CERT_STRONG_SIGN_SERIALIZED_INFO {
  DWORD  dwFlags;
  LPWSTR pwszCNGSignHashAlgids;
  LPWSTR pwszCNGPubKeyMinBitLengths;
} CERT_STRONG_SIGN_SERIALIZED_INFO, *PCERT_STRONG_SIGN_SERIALIZED_INFO;

typedef struct _CERT_STRONG_SIGN_PARA {
  DWORD cbSize;
  DWORD dwInfoChoice;
  union {
    void                              *pvInfo;
    PCERT_STRONG_SIGN_SERIALIZED_INFO pSerializedInfo;
    LPSTR                             pszOID;
  } DUMMYUNIONNAME;
} CERT_STRONG_SIGN_PARA, *PCERT_STRONG_SIGN_PARA;typedef const CERT_STRONG_SIGN_PARA *PCCERT_STRONG_SIGN_PARA;

typedef struct WINTRUST_SIGNATURE_SETTINGS_ {
  DWORD                  cbStruct;
  DWORD                  dwIndex;
  DWORD                  dwFlags;
  DWORD                  cSecondarySigs;
  DWORD                  dwVerifiedSigIndex;
  PCERT_STRONG_SIGN_PARA pCryptoPolicy;
} WINTRUST_SIGNATURE_SETTINGS, *PWINTRUST_SIGNATURE_SETTINGS;

typedef struct {
  DWORD                       cbStruct;
  LPVOID                      pPolicyCallbackData;
  LPVOID                      pSIPClientData;
  DWORD                       dwUIChoice;
  DWORD                       fdwRevocationChecks;
  DWORD                       dwUnionChoice;
  union {
    struct WINTRUST_FILE_INFO_  *pFile;
    struct WINTRUST_CATALOG_INFO_  *pCatalog;
    struct WINTRUST_BLOB_INFO_  *pBlob;
    struct WINTRUST_SGNR_INFO_  *pSgnr;
    struct WINTRUST_CERT_INFO_  *pCert;
  };
  DWORD                       dwStateAction;
  HANDLE                      hWVTStateData;
  WCHAR                       *pwszURLReference;
  DWORD                       dwProvFlags;
  DWORD                       dwUIContext;
  WINTRUST_SIGNATURE_SETTINGS *pSignatureSettings;
} WINTRUST_DATA_2012, *PWINTRUST_DATA_2012;
#endif
//----------------------------------------------------------------------------
//
//  VerifyEmbeddedSignatures
//  Verifies all embedded signatures of a file
//
//----------------------------------------------------------------------------
DWORD VerifyEmbeddedSignatures(_In_ PCWSTR FileName,
                               _In_ HANDLE FileHandle,
                               _In_ bool UseStrongSigPolicy)
{
    DWORD Error = ERROR_SUCCESS;
    bool WintrustCalled = false;
    GUID GenericActionId = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    // vs2010以上如编译报错, WINTRUST_DATA_2012 改为 WINTRUST_DATA
    WINTRUST_DATA_2012 WintrustData = {};
    WINTRUST_FILE_INFO FileInfo = {};
    WINTRUST_SIGNATURE_SETTINGS SignatureSettings = {};
    CERT_STRONG_SIGN_PARA StrongSigPolicy = {};

    // Setup data structures for calling WinVerifyTrust
    WintrustData.cbStruct = sizeof(WINTRUST_DATA);
    WintrustData.dwStateAction  = WTD_STATEACTION_VERIFY;
    WintrustData.dwUIChoice = WTD_UI_NONE;
    WintrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    WintrustData.dwUnionChoice = WTD_CHOICE_FILE;

    FileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO_);
    FileInfo.hFile = FileHandle;
    FileInfo.pcwszFilePath = FileName;
    WintrustData.pFile = &FileInfo;

    //
    // First verify the primary signature (index 0) to determine how many secondary signatures
    // are present. We use WSS_VERIFY_SPECIFIC and dwIndex to do this, also setting
    // WSS_GET_SECONDARY_SIG_COUNT to have the number of secondary signatures returned.
    //
    SignatureSettings.cbStruct = sizeof(WINTRUST_SIGNATURE_SETTINGS);
    SignatureSettings.dwFlags = WSS_GET_SECONDARY_SIG_COUNT | WSS_VERIFY_SPECIFIC;
    SignatureSettings.dwIndex = 0;
    WintrustData.pSignatureSettings = &SignatureSettings;

#if 0
    if (UseStrongSigPolicy != false) {
        StrongSigPolicy.cbSize = sizeof(CERT_STRONG_SIGN_PARA);
        StrongSigPolicy.dwInfoChoice = CERT_STRONG_SIGN_OID_INFO_CHOICE;
        StrongSigPolicy.pszOID = szOID_CERT_STRONG_SIGN_OS_CURRENT;
        WintrustData.pSignatureSettings->pCryptoPolicy = &StrongSigPolicy;
    }
#endif

    //wprintf(L"Verifying primary signature... ");
    Error = WinVerifyTrust(NULL, &GenericActionId, &WintrustData);
    WintrustCalled = true;
    if (Error != ERROR_SUCCESS) {
        // TRUST_E_NOSIGNATURE
        PrintError(Error);
        goto Cleanup;
    }

    //wprintf(L"Success!\n");

    //wprintf(L"Found %d secondary signatures\n", WintrustData.pSignatureSettings->cSecondarySigs);

    // Now attempt to verify all secondary signatures that were found
    for(DWORD x = 1; x <= WintrustData.pSignatureSettings->cSecondarySigs; x++) {
        //wprintf(L"Verify secondary signature at index %d... ", x);

        // Need to clear the previous state data from the last call to WinVerifyTrust
        WintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        Error = WinVerifyTrust(NULL, &GenericActionId, &WintrustData);
        if (Error != ERROR_SUCCESS) {
            //No need to call WinVerifyTrust again
            WintrustCalled = false;
            PrintError(Error);
            goto Cleanup;
        }

        WintrustData.hWVTStateData = NULL;

        // Caller must reset dwStateAction as it may have been changed during the last call
        WintrustData.dwStateAction = WTD_STATEACTION_VERIFY;
        WintrustData.pSignatureSettings->dwIndex = x;
        Error = WinVerifyTrust(NULL, &GenericActionId, &WintrustData);
        if (Error != ERROR_SUCCESS) {
            PrintError(Error);
            goto Cleanup;
        }

        //wprintf(L"Success!\n");
    }

Cleanup:

    //
    // Caller must call WinVerifyTrust with WTD_STATEACTION_CLOSE to free memory
    // allocate by WinVerifyTrust
    //
    if (WintrustCalled != false) {
        WintrustData.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(NULL, &GenericActionId, &WintrustData);
    }

    return Error;
}

bool WinVerifyTrust::verifyFile(PCWSTR fileName)
{
    DWORD Error = ERROR_SUCCESS;
    HANDLE FileHandle = INVALID_HANDLE_VALUE;
    bool UseStrongSigPolicy = false;
    bool bVerify = false;

    if (fileName == NULL) {
        return false;
    }

    FileHandle = CreateFileW(fileName,
                             GENERIC_READ,
                             FILE_SHARE_READ,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             NULL);
    if (FileHandle == INVALID_HANDLE_VALUE) {
        Error = GetLastError();
        PrintError(Error);
        goto Cleanup;
    }

    Error = VerifyEmbeddedSignatures(fileName, FileHandle, UseStrongSigPolicy);
    if (Error == ERROR_SUCCESS) {
        bVerify = true;
    }

Cleanup:
    if (FileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(FileHandle);
    }

    return bVerify;
}

DWORD GetOffsetOfRVA(DWORD lRVA, DWORD lBase)
{
    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)lBase;
    IMAGE_NT_HEADERS32* pNTHeaders = (IMAGE_NT_HEADERS32*)((char*)lBase + pDosHeader->e_lfanew);
    WORD wSecNum = pNTHeaders->FileHeader.NumberOfSections;
    IMAGE_SECTION_HEADER* pSectionHeader = (IMAGE_SECTION_HEADER*)((char*)pNTHeaders + sizeof(IMAGE_NT_HEADERS));

    for (int i=0; i<wSecNum; i++, pSectionHeader++) {
        DWORD lSecRVA = pSectionHeader->VirtualAddress;
        if (lRVA>=lSecRVA && lRVA<(lSecRVA + pSectionHeader->SizeOfRawData)) {
            return lRVA-lSecRVA+(pSectionHeader->PointerToRawData);
        }
    }
    return -1;
}

DWORD GetOffsetOfRVA64(DWORD lRVA, DWORD lBase)
{
    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)lBase;
    IMAGE_NT_HEADERS64* pNTHeaders = (IMAGE_NT_HEADERS64*)((char*)lBase + pDosHeader->e_lfanew);
    WORD wSecNum = pNTHeaders->FileHeader.NumberOfSections;
    IMAGE_SECTION_HEADER* pSectionHeader = (IMAGE_SECTION_HEADER*)((char*)pNTHeaders + sizeof(IMAGE_NT_HEADERS64));

    for (int i=0; i<wSecNum; i++, pSectionHeader++) {
        DWORD lSecRVA = pSectionHeader->VirtualAddress;
        if (lRVA>=lSecRVA && lRVA<(lSecRVA + pSectionHeader->SizeOfRawData)) {
            return lRVA-lSecRVA+(pSectionHeader->PointerToRawData);
        }
    }
    return -1;
}

void AnalysePE(PCWSTR strPEName, QVector<QString> &iList)
{
    iList.clear();

    typedef DWORD (*GetRVA)(DWORD lRVA, DWORD lBase);
    GetRVA gr = GetOffsetOfRVA;

    HANDLE hFile = CreateFileW(strPEName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }
    HANDLE hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, 0);
    if (hFileMapping == INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return;
    }
    LPVOID lp = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);

    DWORD ulImAddress;
    IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor;
    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)lp;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        goto Cleanup;
    }
    IMAGE_NT_HEADERS* pNTHeaders = (IMAGE_NT_HEADERS*)((char*)lp + pDosHeader->e_lfanew);
    if (pNTHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        gr = GetOffsetOfRVA64;

        IMAGE_NT_HEADERS64 *pNTHeaders64 = (IMAGE_NT_HEADERS64*)((char*)lp + pDosHeader->e_lfanew);
        ulImAddress = pNTHeaders64->OptionalHeader.DataDirectory[1].VirtualAddress;
    }
    else {
        gr = GetOffsetOfRVA;
        ulImAddress = pNTHeaders->OptionalHeader.DataDirectory[1].VirtualAddress;
    }

    DWORD offset = gr(ulImAddress, (DWORD)lp);
    if (offset == -1) {
        goto Cleanup;
    }
    pImportDescriptor = (IMAGE_IMPORT_DESCRIPTOR*)((char*)lp + offset);

    for (int i=0; ;i++,pImportDescriptor++) {
        if (pImportDescriptor->OriginalFirstThunk==0&&pImportDescriptor->FirstThunk==0
            &&pImportDescriptor->ForwarderChain==0&&pImportDescriptor->Name==0
            &&pImportDescriptor->TimeDateStamp==0)
        {
            break;
        }

        offset = gr(pImportDescriptor->Name, (long)lp);
        if (offset != -1) {
            char* pchDllName = (char*)lp + gr(pImportDescriptor->Name, (long)lp);
            iList.push_back(pchDllName);
        }
    }

Cleanup:
    UnmapViewOfFile(lp);
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
}

QVector<QString> WinVerifyTrust::getPEImportList(PCWSTR filename)
{
    QVector<QString> importList;

    AnalysePE(filename, importList);

    return importList;
}

QString findFileFullPath(QString name)
{
    QString fullName;
    WCHAR szbuf[512];

    if (QFile::exists(name)) {
        return name;
    }

    GetSystemWindowsDirectoryW(szbuf, sizeof(szbuf));
    fullName = QString::fromStdWString(szbuf);

    fullName += QString::fromStdWString(L"/");
    QString n1 = fullName + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system32/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system32/drivers/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    n1 = fullName + QString::fromStdWString(L"system/") + name;
    if (QFile::exists(n1)) {
        return n1;
    }

    return name;
}

void findFileFullPath(QVector<QString> &fList)
{
    int c = fList.count();
    for (int i = 0; i < c; i++) {
        fList[i] = findFileFullPath(fList[i]);
    }
}

void getDependency_private(PCWSTR fn, QMap<QString,int> &map, int deep)
{
    // 递归结束
    if (deep <= 0) {
        return;
    }
    deep--;

    QVector<QString> iList = WinVerifyTrust::getPEImportList(fn);

    findFileFullPath(iList);

    int c = iList.count();
    for (int i = 0; i < c; i++) {
        map[iList[i]] = 1;

        // 递归
        getDependency_private(iList[i].toStdWString().data(), map, deep);
    }
}

QVector<QString> WinVerifyTrust::getPEDependencyFiles(PCWSTR filename, int deep)
{
    QVector<QString> dList;

    if (filename == NULL || deep <= 0) {
        return dList;
    }

    QString strFile = QString::fromStdWString(filename);
    if (QFile::exists(strFile) == false) {
        return dList;
    }
    if (   strFile.endsWith(".exe", Qt::CaseInsensitive) == false
        && strFile.endsWith(".sys", Qt::CaseInsensitive) == false
        && strFile.endsWith(".dll", Qt::CaseInsensitive) == false)
    {
        return dList;
    }

    QMap<QString,int> map;
    getDependency_private(filename, map, deep);

    QMap<QString,int>::iterator itor;
    for (itor = map.begin(); itor != map.end(); itor++) {
        dList.push_back(itor.key());
    }

    return dList;
}
