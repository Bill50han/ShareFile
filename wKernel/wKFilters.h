#pragma once
#include <fltKernel.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h>
#include <dontuse.h>

//这里因为都是和windows内核交互的回调函数，所以没有建类，全是裸函数。

typedef struct _FILE_DISPOSITION_INFO {
    BOOLEAN DeleteFile;
} FILE_DISPOSITION_INFO, * PFILE_DISPOSITION_INFO;

extern PFLT_FILTER gFilterHandle;
extern PFLT_INSTANCE gFilterInstance;

extern const FLT_OPERATION_REGISTRATION Callbacks[];
extern const FLT_REGISTRATION FilterRegistration;

NTSTATUS SetupInstance(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
    _In_ DEVICE_TYPE VolumeDeviceType,
    _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType
);

FLT_PREOP_CALLBACK_STATUS PreCreateCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS PreSetInformationCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

FLT_PREOP_CALLBACK_STATUS PreWriteCallback(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

NTSTATUS UnloadCallback(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
