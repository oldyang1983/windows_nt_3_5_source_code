/*

Copyright (c) 1992  Microsoft Corporation

Module Name:

	fsp_fork.c

Abstract:

	This module contains the entry points for the AFP fork APIs queued to
	the FSP. These are all callable from FSP Only.

Author:

	Jameel Hyder (microsoft!jameelh)


Revision History:
	25 Apr 1992		Initial Version

Notes:	Tab stop: 4
--*/

#define	FILENUM	FILE_FSP_FORK

#include <afp.h>
#include <gendisp.h>
#include <fdparm.h>
#include <pathmap.h>
#include <atalkio.h>
#include <forkio.h>

#ifdef ALLOC_PRAGMA
// #pragma alloc_text( PAGE, AfpFspDispOpenFork)		// Do not page this out for perf.
// #pragma alloc_text( PAGE, AfpFspDispCloseFork)		// Do not page this out for perf.
#pragma alloc_text( PAGE, AfpFspDispGetForkParms)
#pragma alloc_text( PAGE, AfpFspDispSetForkParms)
// #pragma alloc_text( PAGE, AfpFspDispRead)			// Do not page this out for perf.
// #pragma alloc_text( PAGE, AfpFspDispWrite)           // Do not page this out for perf.
#pragma alloc_text( PAGE, AfpFspDispByteRangeLock)
#pragma alloc_text( PAGE, AfpFspDispFlushFork)
#endif

/***	AfpFspDispOpenFork
 *
 *	This is the worker routine for the AfpOpenFork API.
 *
 *	The request packet is represented below.
 *
 *	sda_AfpSubFunc	BYTE		Resource/Data Flag
 *	sda_ReqBlock	PCONNDESC	pConnDesc
 *	sda_ReqBlock	DWORD		ParentDirId
 *	sda_ReqBlock	DWORD		Bitmap
 *	sda_ReqBlock	DWORD		AccessMode
 *	sda_Name1		ANSI_STRING	Path
 */
AFPSTATUS
AfpFspDispOpenFork(
	IN	PSDA	pSda
)
{
	DWORD			Bitmap, BitmapI;
	AFPSTATUS		RetCode = AFP_ERR_NONE, Status = AFP_ERR_PARAM;
	FILEDIRPARM		FDParm;
	PVOLDESC		pVolDesc;
	PATHMAPENTITY	PME;
	PCONNDESC		pConnDesc;
	POPENFORKENTRY	pOpenForkEntry = NULL;
	BOOLEAN			Resource, CleanupLock = False;
	BYTE			OpenMode = 0;
	UNICODE_STRING	ParentPath;
	struct _RequestPacket
	{
		 PCONNDESC	_pConnDesc;
		 DWORD		_ParentDirId;
		 DWORD		_Bitmap;
		 DWORD		_AccessMode;
	};
	struct _ResponsePacket
	{
		BYTE		__Bitmap[2];
		BYTE		__OForkRefNum[2];
	};
#ifdef	DEBUG
	static PBYTE	OpenDeny[] = { "None", "Read", "Write", "ReadWrite" };
#endif

	PAGED_CODE( );

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispOpenFork: Entered - Session %ld\n", pSda->sda_SessionId));

	pConnDesc = pReqPkt->_pConnDesc;

	ASSERT(VALID_CONNDESC(pConnDesc));

	pVolDesc = pConnDesc->cds_pVolDesc;

	ASSERT(VALID_VOLDESC(pVolDesc));

	Bitmap = pReqPkt->_Bitmap;

	Resource = ((pSda->sda_AfpSubFunc & FORK_RSRC) == FORK_RSRC) ? True : False;

	if ((Resource && (Bitmap & FILE_BITMAP_DATALEN))  ||
		(!Resource && (Bitmap & FILE_BITMAP_RESCLEN)))
	{
		return AFP_ERR_BITMAP;
	}

	do
	{
		AfpInitializeFDParms(&FDParm);
		AfpInitializePME(&PME, 0, NULL);

		// We will use the PME.pme_Handle for open fork handle
		OpenMode = (BYTE)(pReqPkt->_AccessMode & FORK_OPEN_MASK);

		// Validate volume type and open modes
		if (!IS_CONN_NTFS(pConnDesc))
		{
			// Resource fork only supported on NTFS
			if (Resource)
			{
				Status = AFP_ERR_OBJECT_NOT_FOUND;
				break;
			}
			if (OpenMode & FORK_OPEN_WRITE)
			{
				Status = AFP_ERR_VOLUME_LOCKED;
				break;
			}
		}
		else if ((OpenMode & FORK_OPEN_WRITE) && IS_VOLUME_RO(pVolDesc))
		{
			Status = AFP_ERR_VOLUME_LOCKED;
			break;
		}

		BitmapI = FILE_BITMAP_FILENUM		|
				  FD_BITMAP_PARENT_DIRID	|
				  FD_INTERNAL_BITMAP_RETURN_PMEPATHS;

		// Encode the open access into the bitmap for pathmap
		// to use when opening the fork.
		if (Resource)
		{
			BitmapI |= FD_INTERNAL_BITMAP_OPENFORK_RESC;
		}
		if (OpenMode & FORK_OPEN_READ)
		{
			BitmapI |= FD_INTERNAL_BITMAP_OPENACCESS_READ;
		}
		if (OpenMode & FORK_OPEN_WRITE)
		{
			BitmapI |= FD_INTERNAL_BITMAP_OPENACCESS_WRITE;
		}

		// Encode the deny mode into the bitmap for pathmap
		// to use when opening the fork.
		BitmapI |= ((pReqPkt->_AccessMode >> FORK_DENY_SHIFT) &
					FORK_DENY_MASK) <<
					FD_INTERNAL_BITMAP_DENYMODE_SHIFT;

		DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
				("AfpFspDispOpenFork: OpenMode %s, DenyMode %s\n",
				OpenDeny[(pReqPkt->_AccessMode & FORK_OPEN_MASK)],
				OpenDeny[(pReqPkt->_AccessMode >> FORK_DENY_SHIFT) & FORK_DENY_MASK]));

		//
		// Don't allow an FpExchangeFiles to occur while we are referencing
		// the DFE FileId -- we want to make sure we put the right ID into
		// the OpenForkDesc!!
		//
		AfpSwmrTakeWriteAccess(&pVolDesc->vds_ExchangeFilesLock);
		CleanupLock = True;
		if ((Status = AfpMapAfpPathForLookup(pConnDesc,
											 pReqPkt->_ParentDirId,
											 &pSda->sda_Name1,
											 pSda->sda_PathType,
											 DFE_FILE,
											 Bitmap | BitmapI |
											 // Need these for drop folder
											 // checking
											 FILE_BITMAP_DATALEN |
											 FILE_BITMAP_RESCLEN,
											 &PME,
											 &FDParm)) != AFP_ERR_NONE)
		{
			DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
				("AfpFspDispOpenFork: AfpMapAfpPathForLookup %lx\n", Status));

			// If we got a DENY_CONFLICT error, then we still need the parameters
			// Do an open for nothing with no deny modes to get the parameters.
			PME.pme_Handle.fsh_FileHandle = NULL;
			if (Status == AFP_ERR_DENY_CONFLICT)
			{
				AFPSTATUS	xxStatus;

				// Free up any path-buffer allocated
				if (PME.pme_FullPath.Buffer != NULL)
				{
					DBGPRINT(DBG_COMP_FORKS, DBG_LEVEL_INFO,
							("AfpFspDispOpenFork: (DenyConflict) Freeing path buffer %lx\n",
							PME.pme_FullPath.Buffer));
					AfpFreeMemory(PME.pme_FullPath.Buffer);
				}
				AfpInitializePME(&PME, 0, NULL);

				BitmapI = FILE_BITMAP_FILENUM		|
							FD_BITMAP_PARENT_DIRID	|
							FD_INTERNAL_BITMAP_RETURN_PMEPATHS;
				if (Resource)
				{
					BitmapI |= FD_INTERNAL_BITMAP_OPENFORK_RESC;
				}
				xxStatus = AfpMapAfpPathForLookup(pConnDesc,
												 pReqPkt->_ParentDirId,
												 &pSda->sda_Name1,
												 pSda->sda_PathType,
												 DFE_FILE,
												 Bitmap | BitmapI,
												 &PME,
												 &FDParm);
				if (!NT_SUCCESS(xxStatus))
				{
					PME.pme_Handle.fsh_FileHandle = NULL;
					Status = xxStatus;
					break;
				}
			}
			else break;
		}

		if (Status == AFP_ERR_NONE)
		{
			Status = AfpForkOpen(pSda,
								 pConnDesc,
								 &PME,
								 &FDParm,
								 pReqPkt->_AccessMode,
								 Resource,
								 &pOpenForkEntry,
								 &CleanupLock);	
		}

		// At this point we have either successfully opened the fork,
		// encountered a DENY_CONFLICT or some other error.
		if ((Status != AFP_ERR_NONE) &&
			(Status != AFP_ERR_DENY_CONFLICT))
			break;

		// Do drop folder sanity check if someone tries to open for Write only
		if ((Status == AFP_ERR_NONE) &&
			(OpenMode == FORK_OPEN_WRITE) &&
			((FDParm._fdp_RescForkLen != 0) ||
			 (FDParm._fdp_DataForkLen != 0)))
		{
			ASSERT (VALID_OPENFORKENTRY(pOpenForkEntry));

			// If either fork is not empty, and one of them is being
			// opened for write, the user must also have READ access
			// to the parent directory.
			ParentPath = pOpenForkEntry->ofe_pOpenForkDesc->ofd_FilePath;
			// adjust the length to not include the filename
			ParentPath.Length -= pOpenForkEntry->ofe_pOpenForkDesc->ofd_FileName.Length;
			if (ParentPath.Length > 0)
			{
				ParentPath.Length -= sizeof(WCHAR);
			}

			AfpSwmrTakeWriteAccess(&pVolDesc->vds_IdDbAccessLock);
			Status = AfpCheckParentPermissions(pConnDesc,
								               FDParm._fdp_ParentId,
											   &ParentPath,
											   DIR_ACCESS_READ,
											   NULL);
			AfpSwmrReleaseAccess(&pVolDesc->vds_IdDbAccessLock);
			//
			// We are no longer referencing the FileId or path kept
			// in the OpenForkDesc.  Ok for FpExchangeFiles to resume.
			//
			AfpSwmrReleaseAccess(&pVolDesc->vds_ExchangeFilesLock);
			CleanupLock = False;

			if (Status != AFP_ERR_NONE)
			{
				AfpForkClose(pOpenForkEntry);
				AfpForkDereference(pOpenForkEntry);

				// set this to null so it wont be upgraded/deref'd
				// in cleanup below
				pOpenForkEntry = NULL;

				// Set handle to null since it was closed in AfpForkClose
				// and we wont want it to be closed in cleanup below
				PME.pme_Handle.fsh_FileHandle = NULL;
				break;
			}
		}
		else
		{
			AfpSwmrReleaseAccess(&pVolDesc->vds_ExchangeFilesLock);
			CleanupLock = False;
		}

		if (RetCode == AFP_ERR_NONE)
		{
			pSda->sda_ReplySize = SIZE_RESPPKT +
						EVENALIGN(AfpGetFileDirParmsReplyLength(&FDParm, Bitmap));

			if ((RetCode = AfpAllocReplyBuf(pSda)) != AFP_ERR_NONE)
			{
				if (pOpenForkEntry != NULL)
					AfpForkClose(pOpenForkEntry);
				break;
			}
			AfpPackFileDirParms(&FDParm,
								Bitmap,
								pSda->sda_ReplyBuf + SIZE_RESPPKT);
			PUTDWORD2SHORT(pRspPkt->__Bitmap, Bitmap);
			PUTDWORD2SHORT(pRspPkt->__OForkRefNum, (pOpenForkEntry == NULL) ?
									0 : pOpenForkEntry->ofe_OForkRefNum);
			if (Status == AFP_ERR_NONE)
			{
				INTERLOCKED_INCREMENT_LONG(&pConnDesc->cds_cOpenForks,
										   &pConnDesc->cds_ConnLock);
			}
		}
		else Status = RetCode;
	} while (False);


	if (CleanupLock)
	{
		AfpSwmrReleaseAccess(&pVolDesc->vds_ExchangeFilesLock);
	}

	if (pOpenForkEntry != NULL)
	{
		if (Status == AFP_ERR_NONE)
			AfpUpgradeHandle(&pOpenForkEntry->ofe_FileSysHandle);
		AfpForkDereference(pOpenForkEntry);
	}

	if (!NT_SUCCESS(Status))
	{
		if (PME.pme_Handle.fsh_FileHandle != NULL)
			AfpIoClose(&PME.pme_Handle);
	}

	if (PME.pme_FullPath.Buffer != NULL)
	{
		DBGPRINT(DBG_COMP_FORKS, DBG_LEVEL_INFO,
				("AfpFspDispOpenFork: Freeing path buffer %lx\n",
				PME.pme_FullPath.Buffer));
		AfpFreeMemory(PME.pme_FullPath.Buffer);
	}

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispOpenFork: Returning %ld\n", Status));

	return Status;
}



/***	AfpFspDispCloseFork
 *
 *	This is the worker routine for the AfpCloseFork API.
 *
 *	The request packet is represented below.
 *
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 */
AFPSTATUS
AfpFspDispCloseFork(
	IN	PSDA	pSda
)
{
	struct _RequestPacket
	{
		 POPENFORKENTRY	_pOpenForkEntry;
	};

	PAGED_CODE( );

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispCloseFork: Entered - Session %ld, Fork %ld\n",
			pSda->sda_SessionId, pReqPkt->_pOpenForkEntry->ofe_ForkId));

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	AfpForkClose(pReqPkt->_pOpenForkEntry);

	return AFP_ERR_NONE;
}



/***	AfpFspDispGetForkParms
 *
 *	This is the worker routine for the AfpGetForkParms API.
 *
 *	The request packet is represented below.
 *
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 *	sda_ReqBlock	DWORD			Bitmap
 */
AFPSTATUS
AfpFspDispGetForkParms(
	IN	PSDA	pSda
)
{
	FILEDIRPARM		FDParm;
	DWORD			Bitmap;
	AFPSTATUS		Status = AFP_ERR_PARAM;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
		DWORD  			_Bitmap;
	};
	struct _ResponsePacket
	{
		BYTE	__Bitmap[2];
	};

	PAGED_CODE( );

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispGetForkParms: Entered Session %ld, Fork %ld\n",
			pSda->sda_SessionId, pReqPkt->_pOpenForkEntry->ofe_ForkId));

	Bitmap = pReqPkt->_Bitmap;

	do
	{
		if ((RESCFORK(pReqPkt->_pOpenForkEntry) && (Bitmap & FILE_BITMAP_DATALEN)) ||
			(DATAFORK(pReqPkt->_pOpenForkEntry) && (Bitmap & FILE_BITMAP_RESCLEN)))
		{
			Status = AFP_ERR_BITMAP;
			break;
		}

		AfpInitializeFDParms(&FDParm);

		// Optimize for the most common case.
		if ((Bitmap & (FILE_BITMAP_DATALEN | FILE_BITMAP_RESCLEN)) != 0)
		{
			FORKOFFST	ForkLength;

			Status = AfpIoQuerySize(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
									&ForkLength);
		
			ASSERT(NT_SUCCESS(Status));

			if (Bitmap & FILE_BITMAP_DATALEN)
				 FDParm._fdp_DataForkLen = ForkLength.LowPart;
			else FDParm._fdp_RescForkLen = ForkLength.LowPart;
			FDParm._fdp_Flags = 0;		// Take out the directory flag
		}

		// If we need more stuff, go get it
		if (Bitmap & ~(FILE_BITMAP_DATALEN | FILE_BITMAP_RESCLEN))
		{
			CONNDESC		ConnDesc;
			POPENFORKDESC	pOpenForkDesc = pReqPkt->_pOpenForkEntry->ofe_pOpenForkDesc;

			// Since the following call requires a pConnDesc and we do not
			// really have one, manufacture it
			ConnDesc.cds_pSda = pSda;
			ConnDesc.cds_pVolDesc = pOpenForkDesc->ofd_pVolDesc;

			// Don't let FpExchangeFiles come in while we are accessing
			// the stored FileId and its corresponding DFE
			AfpSwmrTakeWriteAccess(&ConnDesc.cds_pVolDesc->vds_ExchangeFilesLock);

			Status = AfpMapAfpIdForLookup(&ConnDesc,
										  pOpenForkDesc->ofd_FileNumber,
										  DFE_FILE,
										  Bitmap & ~(FILE_BITMAP_DATALEN | FILE_BITMAP_RESCLEN),
										  NULL,
										  &FDParm);
            AfpSwmrReleaseAccess(&ConnDesc.cds_pVolDesc->vds_ExchangeFilesLock);
			if (Status != AFP_ERR_NONE)
			{
				break;
			}
		}

		if (Status == AFP_ERR_NONE)
		{
			pSda->sda_ReplySize = SIZE_RESPPKT +
					EVENALIGN(AfpGetFileDirParmsReplyLength(&FDParm, Bitmap));

			if ((Status = AfpAllocReplyBuf(pSda)) == AFP_ERR_NONE)
			{
				AfpPackFileDirParms(&FDParm, Bitmap, pSda->sda_ReplyBuf + SIZE_RESPPKT);
				PUTDWORD2SHORT(&pRspPkt->__Bitmap, Bitmap);
			}
		}
	}  while (False);

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispGetForkParms: Returning %ld\n", Status));

	return Status;
}



/***	AfpFspDispSetForkParms
 *
 *	This is the worker routine for the AfpSetForkParms API.
 *  Only thing that can be set with this API is the fork length.
 *
 *	The request packet is represented below.
 *
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 *	sda_ReqBlock	DWORD			Bitmap
 *	sda_ReqBlock	LONG			ForkLength
 *
 *  LOCKS: vds_IdDbAccessLock (SWMR, WRITE)
 */
AFPSTATUS
AfpFspDispSetForkParms(
	IN	PSDA	pSda
)
{
	AFPSTATUS		Status;
	DWORD			Bitmap;
	BOOLEAN			SetSize = False;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
		DWORD  			_Bitmap;
		LONG			_ForkLength;
	};

	PAGED_CODE( );

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispSetForkParms: Entered Session %ld Fork %ld, Length %ld\n",
			pSda->sda_SessionId, pReqPkt->_pOpenForkEntry->ofe_ForkId,
			pReqPkt->_ForkLength));

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	Bitmap = pReqPkt->_Bitmap;

	do
	{
		if ((RESCFORK(pReqPkt->_pOpenForkEntry) &&
				(pReqPkt->_Bitmap & FILE_BITMAP_DATALEN)) ||
			(DATAFORK(pReqPkt->_pOpenForkEntry) &&
				(pReqPkt->_Bitmap & FILE_BITMAP_RESCLEN)))
		{
			Status = AFP_ERR_BITMAP;
			break;
		}

		if (!(pReqPkt->_pOpenForkEntry->ofe_OpenMode & FORK_OPEN_WRITE))
		{
			Status = AFP_ERR_ACCESS_DENIED;
			break;
		}
		else if (pReqPkt->_ForkLength >= 0)
		{
			FORKSIZE	OldSize;

			// We don't try to catch our own changes for setting
			// forksize because we don't know how many times the mac
			// will set the size before closing the handle.  Since
			// a notification will only come in once the handle is
			// closed, we may pile up a whole bunch of our changes
			// in the list, but only one of them will get satisfied.
			//
			// We also do not want to attempt a change if the current length
			// is same as what it is being set to (this happens a lot,
			// unfortunately). Catch this red-handed.

			Status = AfpIoQuerySize(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
								   &OldSize);
			ASSERT (NT_SUCCESS(Status));
			if (!(((LONG)(OldSize.LowPart) == pReqPkt->_ForkLength) &&
				  (OldSize.HighPart == 0)))
			{
				SetSize = True;
				Status = AfpIoSetSize(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
								      pReqPkt->_ForkLength);
			}

			// Update the Dfe view of the fork length.  Don't update the cached
			// modified time even though it does change on NTFS immediately
			// (LastWriteTime for setting length of data fork, ChangeTime for
			// setting length of resource fork).  We will let the
			// change notify update the modified time when the handle is closed.
			// Appleshare 3.0 and 4.0 do not reflect a changed modified time for
			// changing fork length until the fork is closed (or flushed).
			if (NT_SUCCESS(Status) && SetSize)
			{
				PVOLDESC		pVolDesc;
				PDFENTRY		pDfEntry;
				POPENFORKDESC	pOpenForkDesc;

				pOpenForkDesc = pReqPkt->_pOpenForkEntry->ofe_pOpenForkDesc;
				pVolDesc = pOpenForkDesc->ofd_pVolDesc;

				// Don't let FpExchangeFiles come in while we are accessing
				// the stored FileId and its corresponding DFE
				AfpSwmrTakeWriteAccess(&pVolDesc->vds_ExchangeFilesLock);

				AfpSwmrTakeWriteAccess(&pVolDesc->vds_IdDbAccessLock);

				if ((pDfEntry = AfpFindEntryByAfpId(pVolDesc,
													pOpenForkDesc->ofd_FileNumber,
													DFE_FILE)) != NULL)
				{
					if (RESCFORK(pReqPkt->_pOpenForkEntry))
					{
						// If a FlushFork occurs on resource fork, it should
						// update the modified time to the ChangeTime
						pReqPkt->_pOpenForkEntry->ofe_Flags |= OPEN_FORK_WRITTEN;

						pDfEntry->dfe_RescLen = pReqPkt->_ForkLength;
					}
					else
				    {
						pDfEntry->dfe_DataLen = pReqPkt->_ForkLength;
                    }
				}

				ASSERT (VALID_DFE(pDfEntry));

				AfpSwmrReleaseAccess(&pVolDesc->vds_IdDbAccessLock);
				AfpSwmrReleaseAccess(&pVolDesc->vds_ExchangeFilesLock);
			}

		}
		else Status = AFP_ERR_PARAM;
	} while (False);

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispSetForkParms: Returning %ld\n", Status));

	return (Status);
}


/***	AfpFspDispRead
 *
 *	This routine implements the AfpRead API.
 *
 *	The request packet is represented below.
 *
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 *	sda_ReqBlock	LONG			Offset
 *	sda_ReqBlock	LONG			Size
 *	sda_ReqBlock	DWORD			NewLine Mask
 *	sda_ReqBlock	DWORD			NewLine Char
 */
AFPSTATUS
AfpFspDispRead(
	IN	PSDA	pSda
)
{
	AFPSTATUS			Status;
	FORKOFFST			LOffset;
	FORKSIZE			LSize;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
		LONG			_Offset;
		LONG			_Size;
		DWORD			_NlMask;
		DWORD			_NlChar;
	};

	PAGED_CODE( );

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispRead: Entered, Session %ld Fork %ld, Offset %ld, Size %ld\n",
			pSda->sda_SessionId, pReqPkt->_pOpenForkEntry->ofe_ForkId,
			pReqPkt->_Offset, pReqPkt->_Size));

	if ((pReqPkt->_Size < 0) ||
		(pReqPkt->_Offset < 0))
		return AFP_ERR_PARAM;

	if (!(pReqPkt->_pOpenForkEntry->ofe_OpenMode & FORK_OPEN_READ))
	{
		DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_WARN,
				("AfpFspDispRead: AfpRead on a Fork not opened for read\n"));
		return AFP_ERR_ACCESS_DENIED;
	}

	if (pReqPkt->_Size >= 0)
	{
		if (pReqPkt->_Size > ASP_QUANTUM)
			pReqPkt->_Size = ASP_QUANTUM;

		Status = AFP_ERR_NONE;
		if (pReqPkt->_Size > 0)
		{
			pSda->sda_ReadStatus = AFP_ERR_NONE;
			LOffset = RtlConvertLongToLargeInteger(pReqPkt->_Offset);
			LSize = RtlConvertLongToLargeInteger(pReqPkt->_Size);
			if ((pReqPkt->_pOpenForkEntry->ofe_pOpenForkDesc->ofd_UseCount == 1) ||
				(Status = AfpForkLockOperation( pSda,
												pReqPkt->_pOpenForkEntry,
												&LOffset,
												&LSize,
												IOCHECK,
												False)) == AFP_ERR_NONE)
			{
				ASSERT (LSize.HighPart == 0);
				ASSERT ((LONG)(LOffset.LowPart) == pReqPkt->_Offset);
				if ((LONG)(LSize.LowPart) != pReqPkt->_Size)
					pSda->sda_ReadStatus = AFP_ERR_LOCK;

				Status = AFP_ERR_MISC;
				if ((pSda->sda_IOBuf = AfpIOAllocBuffer((USHORT)LSize.LowPart)) != NULL)
				{
					pSda->sda_ReplySize = (USHORT)LSize.LowPart;
					Status = AfpIoForkRead( pSda,
											pReqPkt->_pOpenForkEntry,
											LOffset.LowPart,
											LSize.LowPart,
											(BYTE)pReqPkt->_NlMask,
											(BYTE)pReqPkt->_NlChar);
				}
			}
		}
	}

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispRead: Returning %ld\n", Status));

	return Status;
}


/***	AfpFspDispWrite
 *
 *	This routine implements the AfpWrite API.
 *
 *	The request packet is represented below.
 *
 *	sda_AfpSubFunc	BYTE			EndFlag
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 *	sda_ReqBlock	LONG			Offset
 *	sda_ReqBlock	LONG			Size
 */
AFPSTATUS
AfpFspDispWrite(
	IN	PSDA			pSda
)
{
	FORKOFFST		LOffset;
	FORKSIZE		LSize;
	AFPSTATUS		Status = AFP_ERR_NONE;
	BOOLEAN			EndFlag, FreeWriteBuf = True;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
		LONG			_Offset;
		LONG			_Size;
	};
	struct _ResponsePacket
	{
		BYTE	__RealOffset[4];
	};

	PAGED_CODE( );

	EndFlag = (pSda->sda_AfpSubFunc & AFP_END_FLAG) ? True : False;

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispWrite: Entered, Session %ld, Fork %ld, Offset %ld, Size %ld %sRelative\n",
			pSda->sda_SessionId, pReqPkt->_pOpenForkEntry->ofe_ForkId,
			pReqPkt->_Offset, pReqPkt->_Size, EndFlag ? "End" : "Begin"));

	do
	{
		if ((pReqPkt->_Size < 0) ||
			(!EndFlag && (pReqPkt->_Offset < 0)))
		{
			Status = AFP_ERR_PARAM;
			break;
		}

		ASSERT((pReqPkt->_pOpenForkEntry->ofe_OpenMode & FORK_OPEN_WRITE) &&
			   ((pReqPkt->_Size == 0) ||
			   ((pReqPkt->_Size > 0) && (pSda->sda_IOBuf != NULL))));

		if (pReqPkt->_Size > ASP_QUANTUM)
			pReqPkt->_Size = ASP_QUANTUM;

		// Check if we have a lock conflict and also convert the offset &
		// size to absolute values if end relative
		LOffset = RtlConvertLongToLargeInteger(pReqPkt->_Offset);
		LSize = RtlConvertLongToLargeInteger(pReqPkt->_Size);

		if (pReqPkt->_Size == 0)
		{
			if (!(EndFlag && (pReqPkt->_Offset < 0)))
			{
				break;
			}
		}

		// Skip lock-check if this is the only instance of the open fork and I/O is
		// not end-relative.
		if ((!EndFlag &&
			(pReqPkt->_pOpenForkEntry->ofe_pOpenForkDesc->ofd_UseCount == 1)) ||
			(Status = AfpForkLockOperation( pSda,
											pReqPkt->_pOpenForkEntry,
											&LOffset,
											&LSize,
											IOCHECK,
											EndFlag)) == AFP_ERR_NONE)
		{
			ASSERT (LOffset.HighPart == 0);
			if ((LONG)(LSize.LowPart) != pReqPkt->_Size)
			{
				Status = AFP_ERR_LOCK;
			}
			else if (LSize.LowPart > 0)
			{
				// Assume write will succeed, set flag for FlushFork.
				// This is a one way flag, i.e. only set, never cleared
				pReqPkt->_pOpenForkEntry->ofe_Flags |= OPEN_FORK_WRITTEN;

				if ((Status = AfpIoForkWrite(pSda,
											 pReqPkt->_pOpenForkEntry,
											 (LONG)(LOffset.LowPart),
											 (LONG)(LSize.LowPart))) == AFP_ERR_EXTENDED)
					FreeWriteBuf = False;
			}
		}
	} while (False);

	if (FreeWriteBuf)
		AfpFreeIOBuffer(pSda);

	if (Status == AFP_ERR_NONE)
	{
		pSda->sda_ReplySize = SIZE_RESPPKT;
		if ((Status = AfpAllocReplyBuf(pSda)) == AFP_ERR_NONE)
		{
			PUTDWORD2DWORD(pRspPkt->__RealOffset, LOffset.LowPart+LSize.LowPart);
		}
	}

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispWrite: Returning %ld\n", Status));

	return Status;
}



/***	AfpFspDispByteRangeLock
 *
 *	This routine implements the AfpByteRangeLock API.
 *	We go ahead and call the file system to do the actual locking/unlocking.
 *
 *	The request packet is represented below.
 *
 *	sda_SubFunc		BYTE			Start/End Flag AND Lock/Unlock Flag
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 *	sda_ReqBlock	LONG			Offset
 *	sda_ReqBlock	LONG			Length
 */
AFPSTATUS
AfpFspDispByteRangeLock(
	IN	PSDA	pSda
)
{
	BOOLEAN			EndFlag;
	LOCKOP			Lock;
	LONG			Offset;
	AFPSTATUS		Status = AFP_ERR_PARAM;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
		LONG			_Offset;
		union
		{
		  LONG			_Length;
		  struct
		  {
			FORKOFFST	_LOffset;	// This is a converted value
			FORKSIZE	_LSize;		// This is a converted value
		  };
		};
	};
	struct _ResponsePacket
	{
		BYTE	__RangeStart[4];
	};

	ASSERT (sizeof(struct _RequestPacket) <= (MAX_REQ_ENTRIES)*sizeof(DWORD));

	PAGED_CODE( );

	Lock = (pSda->sda_AfpSubFunc & AFP_UNLOCK_FLAG) ? UNLOCK : LOCK;
	EndFlag = (pSda->sda_AfpSubFunc & AFP_END_FLAG) ? True : False;

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispByteRangeLock: %sLock - Session %ld, Fork %ld Offset %ld Len %ld %sRelative\n",
			(Lock == LOCK) ? "":"Un", pSda->sda_SessionId,
			pReqPkt->_pOpenForkEntry->ofe_ForkId,
			pReqPkt->_Offset, pReqPkt->_Length, EndFlag ? "End" : "Begin"));

	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	if ((EndFlag && (Lock == UNLOCK))		||
		(pReqPkt->_Length == 0)				||
		(!EndFlag && (pReqPkt->_Offset < 0))||
		((pReqPkt->_Length < 0) && (pReqPkt->_Length != MAXULONG)))
		NOTHING;
	else
	{
		if (pReqPkt->_Length == MAXULONG)
			pReqPkt->_Length = MAXLONG;
	
		// NOTE: The order below is important because of overlaying the union above
		pReqPkt->_LSize = RtlConvertLongToLargeInteger(pReqPkt->_Length);
		pReqPkt->_LOffset = RtlConvertLongToLargeInteger(Offset = pReqPkt->_Offset);
	
		Status = AfpForkLockOperation(pSda,
									  pReqPkt->_pOpenForkEntry,
									  &pReqPkt->_LOffset,
									  &pReqPkt->_LSize,
									  Lock,
									  EndFlag);
	
		if (Status == AFP_ERR_NONE)
		{
			ASSERT (pReqPkt->_LOffset.HighPart == 0);
			ASSERT (EndFlag ||
					((LONG)(pReqPkt->_LOffset.LowPart) == Offset));
			pSda->sda_ReplySize = SIZE_RESPPKT;
			if ((Status = AfpAllocReplyBuf(pSda)) == AFP_ERR_NONE)
				PUTDWORD2DWORD(pRspPkt->__RangeStart, pReqPkt->_LOffset.LowPart);
		}
	}

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
			("AfpFspDispByteRangeLock: Returning %ld\n", Status));

	return Status;
}


/***	AfpFspDispFlushFork
 *
 *	This routine implements the AfpFlushFork API. We don't actually do a
 *  real flush, we just query for the current forklength and modified time
 *  for this open fork handle and update our cached data.  Note if 2
 *  different handles to the same file are flushed, we may end up with
 *  different information for each flush.
 *
 *	The request packet is represented below.
 *
 *	sda_ReqBlock	POPENFORKENTRY	pOpenForkEntry
 */
AFPSTATUS
AfpFspDispFlushFork(
	IN	PSDA	pSda
)
{
	FORKOFFST	ForkLength;
	DWORD		Status;
	struct _RequestPacket
	{
		POPENFORKENTRY	_pOpenForkEntry;
	};

	PAGED_CODE( );

	DBGPRINT(DBG_COMP_AFPAPI_FORK, DBG_LEVEL_INFO,
										("AfpFspDispFlushFork: Entered\n"));
	
	ASSERT(VALID_OPENFORKENTRY(pReqPkt->_pOpenForkEntry));

	do
	{
		Status = AfpIoQuerySize(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
								&ForkLength);
	
		ASSERT(NT_SUCCESS(Status));

		if (NT_SUCCESS(Status))
		{
			PVOLDESC		pVolDesc;
			PDFENTRY		pDfEntry;
			POPENFORKDESC	pOpenForkDesc;

			pOpenForkDesc = pReqPkt->_pOpenForkEntry->ofe_pOpenForkDesc;
			pVolDesc = pOpenForkDesc->ofd_pVolDesc;

			// Don't let FpExchangeFiles come in while we are accessing
			// the stored FileId and its corresponding DFE
			AfpSwmrTakeWriteAccess(&pVolDesc->vds_ExchangeFilesLock);

			AfpSwmrTakeWriteAccess(&pVolDesc->vds_IdDbAccessLock);

			if ((pDfEntry = AfpFindEntryByAfpId(pVolDesc,
												pOpenForkDesc->ofd_FileNumber,
												DFE_FILE)) != NULL)
			{
				if (RESCFORK(pReqPkt->_pOpenForkEntry) &&
					(pReqPkt->_pOpenForkEntry->ofe_Flags & OPEN_FORK_WRITTEN))
				{
					AfpIoChangeNTModTime(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
										 // Don't freeze the NTFS ChangeTime
										 // to its current value since someone
										 // may write more data to this resource
										 // fork handle.  										 // update the time if we know mac wrote
										 False,
										 &pDfEntry->dfe_LastModTime);
				}
				else
			    {
					AfpIoQueryTimesnAttr(&pReqPkt->_pOpenForkEntry->ofe_FileSysHandle,
										NULL,
										&pDfEntry->dfe_LastModTime,
										NULL);
                }

				if (RESCFORK(pReqPkt->_pOpenForkEntry))
					 pDfEntry->dfe_RescLen = ForkLength.LowPart;
				else pDfEntry->dfe_DataLen = ForkLength.LowPart;
			}

			ASSERT (VALID_DFE(pDfEntry));

			AfpSwmrReleaseAccess(&pVolDesc->vds_IdDbAccessLock);
			AfpSwmrReleaseAccess(&pVolDesc->vds_ExchangeFilesLock);
		}

	} while (False);

	// Always return success
	return AFP_ERR_NONE;
}


