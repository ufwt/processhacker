/*
 * Process Hacker Plugins -
 *   Update Checker Plugin
 *
 * Copyright (C) 2011-2016 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "..\main.h"
#include <shlobj.h>

PPH_UPDATER_CONTEXT CreateUpdateContext(
    _In_ PPLUGIN_NODE Node,
    _In_ PLUGIN_ACTION Action
    )
{
    PPH_UPDATER_CONTEXT context;

    context = (PPH_UPDATER_CONTEXT)PhCreateAlloc(sizeof(PH_UPDATER_CONTEXT));
    memset(context, 0, sizeof(PH_UPDATER_CONTEXT));
    
    context->Action = Action;
    context->Node = Node;

    return context;
}

VOID FreeUpdateContext(
    _In_ _Post_invalid_ PPH_UPDATER_CONTEXT Context
    )
{
    PhDereferenceObject(Context);
}

VOID TaskDialogCreateIcons(
    _In_ PPH_UPDATER_CONTEXT Context
    )
{
    // Load the Process Hacker window icon
    Context->IconLargeHandle = PH_LOAD_SHARED_ICON_LARGE(PhInstanceHandle, MAKEINTRESOURCE(PHAPP_IDI_PROCESSHACKER));
    Context->IconSmallHandle = PH_LOAD_SHARED_ICON_SMALL(PhInstanceHandle, MAKEINTRESOURCE(PHAPP_IDI_PROCESSHACKER));

    // Set the TaskDialog window icons
    SendMessage(Context->DialogHandle, WM_SETICON, ICON_SMALL, (LPARAM)Context->IconSmallHandle);
    SendMessage(Context->DialogHandle, WM_SETICON, ICON_BIG, (LPARAM)Context->IconLargeHandle);
}

VOID TaskDialogLinkClicked(
    _In_ PPH_UPDATER_CONTEXT Context
    )
{
    //if (!PhIsNullOrEmptyString(Context->ReleaseNotesUrl))
    {
        // Launch the ReleaseNotes URL (if it exists) with the default browser
        //PhShellExecute(Context->DialogHandle, Context->ReleaseNotesUrl->Buffer, NULL);
    }
}

PPH_STRING UpdaterGetOpaqueXmlNodeText(
    _In_ mxml_node_t *xmlNode
    )
{
    if (xmlNode && xmlNode->child && xmlNode->child->type == MXML_OPAQUE && xmlNode->child->value.opaque)
    {
        return PhConvertUtf8ToUtf16(xmlNode->child->value.opaque);
    }

    return PhReferenceEmptyString();
}

PPH_STRING UpdateVersionString(
    VOID
    )
{
    ULONG majorVersion;
    ULONG minorVersion;
    ULONG revisionVersion;
    PPH_STRING currentVersion = NULL;
    PPH_STRING versionHeader = NULL;

    PhGetPhVersionNumbers(
        &majorVersion,
        &minorVersion,
        NULL,
        &revisionVersion
        );

    currentVersion = PhFormatString(
        L"%lu.%lu.%lu",
        majorVersion,
        minorVersion,
        revisionVersion
        );

    if (currentVersion)
    {
        versionHeader = PhConcatStrings2(L"ProcessHacker-Build: ", currentVersion->Buffer);
        PhDereferenceObject(currentVersion);
    }

    return versionHeader;
}

PPH_STRING UpdateWindowsString(
    VOID
    )
{
    static PH_STRINGREF keyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion");

    HANDLE keyHandle = NULL;
    PPH_STRING buildLabHeader = NULL;

    if (NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_READ,
        PH_KEY_LOCAL_MACHINE,
        &keyName,
        0
        )))
    {
        PPH_STRING buildLabString;

        if (buildLabString = PhQueryRegistryString(keyHandle, L"BuildLabEx"))
        {
            buildLabHeader = PhConcatStrings2(L"ProcessHacker-OsBuild: ", buildLabString->Buffer);
            PhDereferenceObject(buildLabString);
        }
        else if (buildLabString = PhQueryRegistryString(keyHandle, L"BuildLab"))
        {
            buildLabHeader = PhConcatStrings2(L"ProcessHacker-OsBuild: ", buildLabString->Buffer);
            PhDereferenceObject(buildLabString);
        }

        NtClose(keyHandle);
    }

    return buildLabHeader;
}

//BOOLEAN ParseVersionString(
//    _Inout_ PPH_UPDATER_CONTEXT Context
//    )
//{
//    PH_STRINGREF sr, majorPart, minorPart, revisionPart;
//    ULONG64 majorInteger = 0, minorInteger = 0, revisionInteger = 0;
//
//    //PhInitializeStringRef(&sr, Context->VersionString->Buffer);
//    PhInitializeStringRef(&revisionPart, Context->RevVersion->Buffer);
//
//    if (PhSplitStringRefAtChar(&sr, '.', &majorPart, &minorPart))
//    {
//        PhStringToInteger64(&majorPart, 10, &majorInteger);
//        PhStringToInteger64(&minorPart, 10, &minorInteger);
//        PhStringToInteger64(&revisionPart, 10, &revisionInteger);
//
//        //Context->MajorVersion = (ULONG)majorInteger;
//        //Context->MinorVersion = (ULONG)minorInteger;
//        //Context->RevisionVersion = (ULONG)revisionInteger;
//
//        return TRUE;
//    }
//
//    return FALSE;
//}

BOOLEAN ReadRequestString(
    _In_ HINTERNET Handle,
    _Out_ _Deref_post_z_cap_(*DataLength) PSTR *Data,
    _Out_ ULONG *DataLength
    )
{
    PSTR data;
    ULONG allocatedLength;
    ULONG dataLength;
    ULONG returnLength;
    BYTE buffer[PAGE_SIZE];

    allocatedLength = sizeof(buffer);
    data = (PSTR)PhAllocate(allocatedLength);
    dataLength = 0;

    // Zero the buffer
    memset(buffer, 0, PAGE_SIZE);

    while (WinHttpReadData(Handle, buffer, PAGE_SIZE, &returnLength))
    {
        if (returnLength == 0)
            break;

        if (allocatedLength < dataLength + returnLength)
        {
            allocatedLength *= 2;
            data = (PSTR)PhReAllocate(data, allocatedLength);
        }

        // Copy the returned buffer into our pointer
        memcpy(data + dataLength, buffer, returnLength);
        // Zero the returned buffer for the next loop
        //memset(buffer, 0, returnLength);

        dataLength += returnLength;
    }

    if (allocatedLength < dataLength + 1)
    {
        allocatedLength++;
        data = (PSTR)PhReAllocate(data, allocatedLength);
    }

    // Ensure that the buffer is null-terminated.
    data[dataLength] = 0;

    *DataLength = dataLength;
    *Data = data;

    return TRUE;
}

NTSTATUS UpdateDownloadThread(
    _In_ PVOID Parameter
    )
{
    BOOLEAN downloadSuccess = FALSE;
    BOOLEAN hashSuccess = FALSE;
    BOOLEAN signatureSuccess = FALSE;
    BOOLEAN updateSuccess = FALSE;
    HANDLE tempFileHandle = NULL;
    HINTERNET httpSessionHandle = NULL;
    HINTERNET httpConnectionHandle = NULL;
    HINTERNET httpRequestHandle = NULL;
    PPH_STRING downloadHostPath = NULL;
    PPH_STRING downloadUrlPath = NULL;
    PPH_STRING userAgentString = NULL;
    PPH_STRING fileDownloadUrl = NULL;
    PUPDATER_HASH_CONTEXT hashContext = NULL;
    ULONG indexOfFileName = -1;
    URL_COMPONENTS httpParts = { sizeof(URL_COMPONENTS) };
    LARGE_INTEGER timeNow;
    LARGE_INTEGER timeStart;
    ULONG64 timeTicks = 0;
    ULONG64 timeBitsPerSecond = 0;
    PPH_UPDATER_CONTEXT context = (PPH_UPDATER_CONTEXT)Parameter;

    SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)L"Initializing download request...");

    context->SetupFilePath = PhCreateCacheFile(PhaFormatString(
        L"%s.zip",
        PhGetStringOrEmpty(context->Node->InternalName)
        ));

    if (PhIsNullOrEmptyString(context->SetupFilePath))
        goto CleanupExit;

    if (!NT_SUCCESS(PhCreateFileWin32(
        &tempFileHandle,
        PhGetStringOrEmpty(context->SetupFilePath),
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_ATTRIBUTE_TEMPORARY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OVERWRITE_IF,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
        )))
    {
        goto CleanupExit;
    }

    fileDownloadUrl = PhFormatString(
        L"https://wj32.org/processhacker/plugins/download.php?id=%s&type=64",
        PhGetStringOrEmpty(context->Node->Id)
        );

    // Set lengths to non-zero enabling these params to be cracked.
    httpParts.dwSchemeLength = ULONG_MAX;
    httpParts.dwHostNameLength = ULONG_MAX;
    httpParts.dwUrlPathLength = ULONG_MAX;

    if (!WinHttpCrackUrl(
        PhGetString(fileDownloadUrl),
        0,
        0,
        &httpParts
        ))
    {
        PhDereferenceObject(fileDownloadUrl);
        goto CleanupExit;
    }

    PhDereferenceObject(fileDownloadUrl);

    // Create the Host string.
    if (PhIsNullOrEmptyString(downloadHostPath = PhCreateStringEx(
        httpParts.lpszHostName,
        httpParts.dwHostNameLength * sizeof(WCHAR)
        )))
    {
        goto CleanupExit;
    }

    // Create the remote path string.
    if (PhIsNullOrEmptyString(downloadUrlPath = PhCreateStringEx(
        httpParts.lpszUrlPath,
        httpParts.dwUrlPathLength * sizeof(WCHAR)
        )))
    {
        goto CleanupExit;
    }

    SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)L"Connecting...");

    if (!(httpSessionHandle = WinHttpOpen(
        PhGetString(userAgentString),
        WindowsVersion >= WINDOWS_8_1 ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
        )))
    {
        goto CleanupExit;
    }

    if (WindowsVersion >= WINDOWS_8_1)
    {
        WinHttpSetOption(
            httpSessionHandle,
            WINHTTP_OPTION_DECOMPRESSION,
            &(ULONG) { WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE },
            sizeof(ULONG)
            );
    }

    if (!(httpConnectionHandle = WinHttpConnect(
        httpSessionHandle,
        PhGetString(downloadHostPath),
        httpParts.nScheme == INTERNET_SCHEME_HTTP ? INTERNET_DEFAULT_HTTP_PORT : INTERNET_DEFAULT_HTTPS_PORT,
        0
        )))
    {
        goto CleanupExit;
    }

    if (!(httpRequestHandle = WinHttpOpenRequest(
        httpConnectionHandle,
        NULL,
        PhGetString(downloadUrlPath),
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_REFRESH | (httpParts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
        )))
    {
        goto CleanupExit;
    }

    WinHttpSetOption(
        httpRequestHandle, 
        WINHTTP_OPTION_DISABLE_FEATURE, 
        &(ULONG){ WINHTTP_DISABLE_KEEP_ALIVE }, 
        sizeof(ULONG)
        );

    SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)L"Sending download request...");

    if (!WinHttpSendRequest(
        httpRequestHandle,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH,
        0
        ))
    {
        goto CleanupExit;
    }

    SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)L"Waiting for response...");

    if (WinHttpReceiveResponse(httpRequestHandle, NULL))
    {
        ULONG bytesDownloaded = 0;
        ULONG downloadedBytes = 0;
        ULONG contentLengthSize = sizeof(ULONG);
        ULONG contentLength = 0;
        PPH_STRING status;
        IO_STATUS_BLOCK isb;
        BYTE buffer[PAGE_SIZE];

        status = PhFormatString(L"Downloading %s...", PhGetString(context->Node->Name));

        SendMessage(context->DialogHandle, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
        SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, (LPARAM)PhGetString(status));

        PhDereferenceObject(status);

        // Start the clock.
        PhQuerySystemTime(&timeStart);

        if (!WinHttpQueryHeaders(
            httpRequestHandle,
            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &contentLength,
            &contentLengthSize,
            0
            ))
        {
            goto CleanupExit;
        }

        // Initialize hash algorithm.
        if (!UpdaterInitializeHash(&hashContext))
            goto CleanupExit;

        // Zero the buffer.
        memset(buffer, 0, PAGE_SIZE);

        // Download the data.
        while (WinHttpReadData(httpRequestHandle, buffer, PAGE_SIZE, &bytesDownloaded))
        {
            // If we get zero bytes, the file was uploaded or there was an error
            if (bytesDownloaded == 0)
                break;

            // If the dialog was closed, just cleanup and exit
            //if (!UpdateDialogThreadHandle)
            //    __leave;

            // Update the hash of bytes we downloaded.
            UpdaterUpdateHash(hashContext, buffer, bytesDownloaded);

            // Write the downloaded bytes to disk.
            if (!NT_SUCCESS(NtWriteFile(
                tempFileHandle,
                NULL,
                NULL,
                NULL,
                &isb,
                buffer,
                bytesDownloaded,
                NULL,
                NULL
                )))
            {
                goto CleanupExit;
            }

            downloadedBytes += (DWORD)isb.Information;

            // Check the number of bytes written are the same we downloaded.
            if (bytesDownloaded != isb.Information)
                goto CleanupExit;

            // Query the current time
            PhQuerySystemTime(&timeNow);

            // Calculate the number of ticks
            timeTicks = (timeNow.QuadPart - timeStart.QuadPart) / PH_TICKS_PER_SEC;
            timeBitsPerSecond = downloadedBytes / __max(timeTicks, 1);

            // TODO: Update on timer callback.
            {
                FLOAT percent = ((FLOAT)downloadedBytes / contentLength * 100);
                PPH_STRING totalLength = PhFormatSize(contentLength, -1);
                PPH_STRING totalDownloaded = PhFormatSize(downloadedBytes, -1);
                PPH_STRING totalSpeed = PhFormatSize(timeBitsPerSecond, -1);

                PPH_STRING statusMessage = PhFormatString(
                    L"Downloaded: %s of %s (%.0f%%)\r\nSpeed: %s/s",
                    totalDownloaded->Buffer,
                    totalLength->Buffer,
                    percent,
                    totalSpeed->Buffer
                    );

                SendMessage(context->DialogHandle, TDM_SET_PROGRESS_BAR_POS, (WPARAM)percent, 0);
                SendMessage(context->DialogHandle, TDM_UPDATE_ELEMENT_TEXT, TDE_CONTENT, (LPARAM)statusMessage->Buffer);

                PhDereferenceObject(statusMessage);
                PhDereferenceObject(totalSpeed);
                PhDereferenceObject(totalLength);
                PhDereferenceObject(totalDownloaded);
            }
        }

        downloadSuccess = TRUE;

        if (UpdaterVerifyHash(hashContext, context->Node->SHA2_64))
        {
            hashSuccess = TRUE;
        }

        if (UpdaterVerifySignature(hashContext, context->Node->HASH_64))
        {
            signatureSuccess = TRUE;
        }
    }

CleanupExit:

    if (hashContext)
        UpdaterDestroyHash(hashContext);

    if (tempFileHandle)
        NtClose(tempFileHandle);

    if (httpRequestHandle)
        WinHttpCloseHandle(httpRequestHandle);

    if (httpConnectionHandle)
        WinHttpCloseHandle(httpConnectionHandle);

    if (httpSessionHandle)
        WinHttpCloseHandle(httpSessionHandle);

    PhClearReference(&downloadHostPath);
    PhClearReference(&downloadUrlPath);
    PhClearReference(&userAgentString);

    if (downloadSuccess && hashSuccess && signatureSuccess)
    {
        if (NT_SUCCESS(SetupExtractBuild(context)))
        {
            updateSuccess = TRUE;
        }
    }

    if (context->SetupFilePath)
    {
        PhDeleteCacheFile(context->SetupFilePath);
        PhDereferenceObject(context->SetupFilePath);
    }

    if (updateSuccess)
    {
        if (PhGetIntegerSetting(L"EnableWarnings"))
            ShowUninstallRestartDialog(context);
        else
            SendMessage(context->DialogHandle, WM_CLOSE, 0, 0);
    }
    else
    {
        ShowUpdateFailedDialog(context, FALSE, FALSE);
    }

    return STATUS_SUCCESS;
}

HRESULT CALLBACK TaskDialogBootstrapCallback(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ LONG_PTR dwRefData
    )
{
    PPH_UPDATER_CONTEXT context = (PPH_UPDATER_CONTEXT)dwRefData;

    switch (uMsg)
    {
    case TDN_CREATED:
        {
            context->DialogHandle = hwndDlg;

            TaskDialogCreateIcons(context);

            switch (context->Action)
            {
            case PLUGIN_ACTION_INSTALL:
                {
                    if (PhGetIntegerSetting(L"EnableWarnings"))
                        ShowAvailableDialog(context);
                    else
                        ShowProgressDialog(context);
                }
                break;
            case PLUGIN_ACTION_UNINSTALL:
                {
                    if (PhGetIntegerSetting(L"EnableWarnings"))
                        ShowPluginUninstallDialog(context);
                    else
                        ShowPluginUninstallWithoutPrompt(context);
                }
                break;
            case PLUGIN_ACTION_RESTART:
                {
                    if (PhGetIntegerSetting(L"EnableWarnings"))
                    {
                        ShowUninstallRestartDialog(context);
                    }
                    else
                    {
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    }
                }
                break;
            }
        }
        break;
    }

    return S_OK;
}

BOOLEAN ShowInitialDialog(
    _In_ HWND Parent,
    _In_ PVOID Context
    )
{   
    INT result = 0;
    TASKDIALOGCONFIG config = { sizeof(TASKDIALOGCONFIG) };
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_CAN_BE_MINIMIZED | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.pszContent = L"Initializing...";
    config.lpCallbackData = (LONG_PTR)Context;
    config.pfCallback = TaskDialogBootstrapCallback;
    config.hwndParent = Parent;

    // Start the TaskDialog bootstrap
    TaskDialogIndirect(&config, &result, NULL, NULL);

    return result == IDOK;
}

BOOLEAN ShowUpdateDialog(
    _In_ HWND Parent,
    _In_ PLUGIN_ACTION Action
    )
{
    BOOLEAN result;
    PH_AUTO_POOL autoPool;
    PPH_UPDATER_CONTEXT context;

    context = CreateUpdateContext(NULL, Action);

    PhInitializeAutoPool(&autoPool);

    result = ShowInitialDialog(Parent, context);

    FreeUpdateContext(context);
    PhDeleteAutoPool(&autoPool);

    return result;
}

BOOLEAN StartInitialCheck(
    _In_ HWND Parent,
    _In_ PPLUGIN_NODE Node,
    _In_ PLUGIN_ACTION Action
    )
{
    BOOLEAN result;
    PH_AUTO_POOL autoPool;
    PPH_UPDATER_CONTEXT context;

    context = CreateUpdateContext(Node, Action);

    PhInitializeAutoPool(&autoPool);

    result = ShowInitialDialog(Parent, context);

    FreeUpdateContext(context);
    PhDeleteAutoPool(&autoPool);

    return result;
}