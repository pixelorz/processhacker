/*
 * Process Hacker Toolchain -
 *   project setup
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

#include <setup.h>
#include <appsup.h>
#include "miniz\miniz.h"

PVOID GetZipResourceData(
    _In_ PULONG resourceLength
    )
{
    HRSRC resourceHandle = NULL;
    HGLOBAL resourceData;
    PVOID resourceBuffer = NULL;

    if (!(resourceHandle = FindResource(PhInstanceHandle, MAKEINTRESOURCE(IDR_BIN_DATA), RT_RCDATA)))
        goto CleanupExit;

    *resourceLength = SizeofResource(PhInstanceHandle, resourceHandle);

    if (!(resourceData = LoadResource(PhInstanceHandle, resourceHandle)))
        goto CleanupExit;

    if (!(resourceBuffer = LockResource(resourceData)))
        goto CleanupExit;

CleanupExit:

    if (resourceHandle)
        FreeResource(resourceHandle);

    return resourceBuffer;
}

BOOLEAN SetupExtractBuild(
    _In_ PPH_SETUP_CONTEXT Context
    )
{
    mz_bool status = MZ_FALSE;
    ULONG64 totalLength = 0;
    ULONG64 currentLength = 0;
    mz_zip_archive zip_archive = { 0 };
    PPH_STRING extractPath = NULL;
    SYSTEM_INFO info;

    GetNativeSystemInfo(&info);

#ifdef PH_BUILD_API
    ULONG resourceLength;
    PVOID resourceBuffer;

    if (!(resourceBuffer = GetZipResourceData(&resourceLength)))
        goto CleanupExit;

    if (!(status = mz_zip_reader_init_mem(&zip_archive, resourceBuffer, resourceLength, 0)))
        goto CleanupExit;
#else
    PPH_BYTES zipPathUtf8;

    if (PhIsNullOrEmptyString(Context->FilePath))
        goto CleanupExit;

    zipPathUtf8 = PhConvertUtf16ToUtf8(PhGetString(Context->FilePath));

    if (!(status = mz_zip_reader_init_file(&zip_archive, zipPathUtf8->Buffer, 0)))
        goto CleanupExit;

    PhDereferenceObject(zipPathUtf8);
#endif

    // Remove outdated files
    //for (ULONG i = 0; i < ARRAYSIZE(SetupRemoveFiles); i++)
    //    SetupDeleteDirectoryFile(SetupRemoveFiles[i].FileName);

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        mz_zip_archive_file_stat zipFileStat;

        if (!mz_zip_reader_file_stat(&zip_archive, i, &zipFileStat))
            continue;

        if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        {
            if (!strncmp(zipFileStat.m_filename, "x32\\", 4))
                continue;
        }
        else
        {
            if (!strncmp(zipFileStat.m_filename, "x64\\", 4))
                continue;
            if (!strncmp(zipFileStat.m_filename, "x86\\", 4))
                continue;
        }

        totalLength += zipFileStat.m_uncomp_size;
    }

    InterlockedExchange64(&ExtractTotalLength, totalLength);
    SendMessage(Context->ExtractPageHandle, WM_START_SETUP, 0, 0);

    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
    {
        IO_STATUS_BLOCK isb;
        HANDLE fileHandle;
        ULONG indexOfFileName = -1;
        PPH_STRING fileName;
        PPH_STRING fullSetupPath;
        PVOID buffer;
        mz_ulong zipFileCrc32 = 0;
        ULONG bufferLength = 0;
        mz_zip_archive_file_stat zipFileStat;

        if (!mz_zip_reader_file_stat(&zip_archive, i, &zipFileStat))
            continue;

        if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        {
            if (!strncmp(zipFileStat.m_filename, "x32\\", 4))
                continue;

            fileName = PhConvertUtf8ToUtf16(zipFileStat.m_filename);

            if (PhFindStringInString(fileName, 0, L"x64\\") != -1)
                PhMoveReference(&fileName, PhSubstring(fileName, 4, (fileName->Length / 2) - 4));
        }
        else
        {
            if (!strncmp(zipFileStat.m_filename, "x64\\", 4))
                continue;
            if (!strncmp(zipFileStat.m_filename, "x86\\", 4))
                continue;

            fileName = PhConvertUtf8ToUtf16(zipFileStat.m_filename);

            if (PhFindStringInString(fileName, 0, L"x32\\") != -1)
                PhMoveReference(&fileName, PhSubstring(fileName, 4, (fileName->Length / 2) - 4));
        }

        if (!(buffer = mz_zip_reader_extract_to_heap(
            &zip_archive,
            zipFileStat.m_file_index,
            &bufferLength,
            0
            )))
        {
            goto CleanupExit;
        }

        if ((zipFileCrc32 = mz_crc32(zipFileCrc32, buffer, bufferLength)) != zipFileStat.m_crc32)
            goto CleanupExit;

        extractPath = PhConcatStrings(3, PhGetString(Context->SetupInstallPath), L"\\", PhGetString(fileName));

        if (fullSetupPath = PhGetFullPath(extractPath->Buffer, &indexOfFileName))
        {
            PPH_STRING directoryPath;

            if (indexOfFileName == -1)
                goto CleanupExit;

            if (directoryPath = PhSubstring(fullSetupPath, 0, indexOfFileName))
            {
                if (!NT_SUCCESS(PhCreateDirectory(directoryPath)))
                {
                    PhDereferenceObject(directoryPath);
                    PhDereferenceObject(fullSetupPath);
                    goto CleanupExit;
                }

                PhDereferenceObject(directoryPath);
            }

            PhDereferenceObject(fullSetupPath);
        }

        // TODO: Backup file and restore if the setup fails.
        //{
        //    PPH_STRING backupFilePath;
        //
        //    backupFilePath = PhConcatStrings(
        //        4, 
        //        PhGetString(Context->SetupInstallPath), 
        //        L"\\", 
        //        PhGetString(fileName),
        //        L".bak"
        //        );
        //
        //    MoveFile(extractPath->Buffer, backupFilePath->Buffer);
        //
        //    PhDereferenceObject(backupFilePath);
        //}

        if (!NT_SUCCESS(PhCreateFileWin32(
            &fileHandle,
            PhGetString(extractPath),
            FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_OVERWRITE_IF,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            goto CleanupExit;
        }

        if (!NT_SUCCESS(NtWriteFile(
            fileHandle,
            NULL,
            NULL,
            NULL,
            &isb,
            buffer,
            bufferLength,
            NULL,
            NULL
            )))
        {
            goto CleanupExit;
        }

        if (isb.Information != bufferLength)
            goto CleanupExit;

        currentLength += bufferLength;

        InterlockedExchange64(&ExtractCurrentLength, currentLength);

        SendMessage(Context->ExtractPageHandle, WM_UPDATE_SETUP, 0, (LPARAM)PhGetBaseName(extractPath));

        NtClose(fileHandle);
        mz_free(buffer);
    }

    mz_zip_reader_end(&zip_archive);

    if (extractPath)
        PhDereferenceObject(extractPath);

    return TRUE;

CleanupExit:

    mz_zip_reader_end(&zip_archive);

    if (extractPath)
        PhDereferenceObject(extractPath);

    return FALSE;
}

