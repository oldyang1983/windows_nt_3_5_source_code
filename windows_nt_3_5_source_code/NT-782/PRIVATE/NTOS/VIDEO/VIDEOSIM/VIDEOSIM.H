/*++

Copyright (c) 1992-1994  Microsoft Corporation

Module Name:

    videosim.h

Abstract:

    definitions for the simulation driver.

Environment:

    Kernel mode

Revision History:

--*/

//
// Define device extension structure. This is device dependant/private
// information.
//

typedef struct _HW_DEVICE_EXTENSION {
    PVOID VideoRamBase;
    ULONG VideoRamLength;
    ULONG FrameLength;
    ULONG CurrentModeNumber;
    HANDLE SectionHandle;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


extern VIDEO_MODE_INFORMATION SimModes[];
extern ULONG SimNumModes;

extern ULONG DeviceCount;
