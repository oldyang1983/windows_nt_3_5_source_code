/*++

Copyright (c) 1993  Microsoft Corporation

Module Name:

    timer.c

Abstract:

    This module contains code which implements the receive and send timeouts
    for each connection.

Author:

    Colin Watson (ColinW) 21-Feb-1993

Environment:

    Kernel mode

Revision History:

--*/

#include "procs.h"

//
//  The debug trace level
//

#define Dbg                             (DEBUG_TRACE_TIMER)

LARGE_INTEGER DueTime;
KDPC NwDpc;                               // DPC object for timeouts.
KTIMER Timer;                           // kernel timer for this request.
ULONG ScavengerTickCount;

#ifdef NWDBG
BOOLEAN DisableTimer = FALSE;
#endif

//
//  When we want to stop the timer, set TimerStop to TRUE. When the timer
//  is stopped TimerStopped will be set to the signalled state.
//

BOOLEAN TimerStop;
KEVENT TimerStopped;

VOID
TimerDPC(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text( PAGE, StartTimer )
#pragma alloc_text( PAGE, StopTimer )

#endif


VOID
StartTimer(
    VOID
    )
/*++

Routine Description:

    This routine starts the timer ticking.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PAGED_CODE();

    //
    //  We need 18.21 ticks per second
    //

    DueTime = LiNeg(LiXDiv(LiNMul( 100000, MILLISECONDS ),1821));

    //
    // This is the first connection with timeouts specified.
    // Set up the timer so that every 500 milliseconds we scan all the
    // connections for timed out receive and sends.
    //

    TimerStop = FALSE;

    KeInitializeEvent( &TimerStopped, SynchronizationEvent, FALSE );
    KeInitializeDpc( &NwDpc, TimerDPC, NULL );
    KeInitializeTimer( &Timer );

    (VOID)KeSetTimer(&Timer, DueTime, &NwDpc);
}


VOID
StopTimer(
    VOID
    )
/*++

Routine Description:

    This routine stops the timer.  It blocks until the timer has stopped.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PAGED_CODE();

    if (TimerStop == FALSE) {
        TimerStop = TRUE;

        KeWaitForSingleObject (&TimerStopped, Executive, KernelMode, FALSE, NULL);
    }
}


VOID
TimerDPC(
    IN PKDPC Dpc,
    IN PVOID Context,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
/*++

Routine Description:

    This routine is called to search for timed out send and receive
    requests.  This routine is called at DPC level.

Arguments:

    Dpc - Unused.
    Context - Unused.
    SystemArgument1 - Unused.
    SystemArgument2 - Unused.

Return Value:

    None.

--*/

{
    PLIST_ENTRY ScbQueueEntry;
    PLIST_ENTRY NextScbQueueEntry;
    PLIST_ENTRY IrpContextEntry;
    PLIST_ENTRY NextIrpContextEntry;
    SHORT RetryCount;
    PIRP_CONTEXT pIrpContext;

    //
    //  For each Server see if there is a timeout to process.
    //

    if ( TimerStop ) {
        KeSetEvent( &TimerStopped, 0, FALSE );
        return;
    }

#ifdef NWDBG
    if ( DisableTimer ) {
        //
        //  Reset the timer to run for another tick.
        //

        (VOID)KeSetTimer ( &Timer, DueTime, &NwDpc);

        return;
    }
#endif

    //DebugTrace(+1, Dbg, "TimerDpc....\n", 0);

    //
    //  Scan through the Scb's looking timed out requests.
    //

    KeAcquireSpinLockAtDpcLevel( &ScbSpinLock );

    for (ScbQueueEntry = ScbQueue.Flink ;
         ScbQueueEntry != &ScbQueue ;
         ScbQueueEntry = NextScbQueueEntry ) {

        PNONPAGED_SCB pNpScb = CONTAINING_RECORD(ScbQueueEntry,
                                                    NONPAGED_SCB,
                                                    ScbLinks);

        NwReferenceScb( pNpScb );
        KeReleaseSpinLockFromDpcLevel( &ScbSpinLock );

        //
        //  Acquire the Scb specific spin lock to protect access
        //  the the Scb fields.
        //

        KeAcquireSpinLockAtDpcLevel( &pNpScb->NpScbSpinLock );

        //
        //  Obtain a pointer to the next SCB in the SCB list before
        //  dereferencing the current one.
        //

        NextScbQueueEntry = pNpScb->ScbLinks.Flink;

        //
        //  Look at the first request on the queue only (since it is
        //  the only active request).
        //

        if ( ( !IsListEmpty( &pNpScb->Requests )) &&
             ( !pNpScb->Sending ) &&
             ( pNpScb->OkToReceive ) &&
             ( --pNpScb->TimeOut <= 0 ) ) {

             PIRP_CONTEXT pIrpContext;

            //
            //  This request has timed out.  Try to retransmit the request.
            //

            pIrpContext = CONTAINING_RECORD(
                              pNpScb->Requests.Flink,
                              IRP_CONTEXT,
                              NextRequest);

            pNpScb->TimeOut = pNpScb->MaxTimeOut;

            //
            //  Check the retry count while we own the spin lock.
            //

            RetryCount = --pNpScb->RetryCount;
            NwDereferenceScb( pNpScb );

            //
            //  Set OkToReceive to FALSE, so that if we receive a response
            //  right now, our receive handler won't handle the response
            //  and cause IRP context to be freed.
            //

            pNpScb->OkToReceive = FALSE;
            KeReleaseSpinLockFromDpcLevel( &pNpScb->NpScbSpinLock );

            if ( pIrpContext->pOriginalIrp->Cancel ) {

                //
                //  This IRP has been cancelled.  Call the callback routine.
                //
                //  BUGBUG - This will cause a timeout error.
                //

                DebugTrace(+0, Dbg, "Timer cancel IRP %X\n", pIrpContext->pOriginalIrp );
                pIrpContext->pEx( pIrpContext, 0, NULL );
#if 0
                NwDequeueIrpContext( pIrpContext, FALSE );
                NwCompleteRequest( pIrpContext, STATUS_CANCELLED );
                KickQueue( pNpScb );
#endif

            } else if ( RetryCount >= 0) {

                //
                //  We're not out of retries.  Resend the request packet.
                //
                //  First adjust the send timeout up.  Adjust the timeout
                //  more slowly on a close by server.
                //

                if ( pNpScb->SendTimeout < pNpScb->MaxTimeOut ) {
                    if ( pNpScb->TickCount <= 4 ) {
                        pNpScb->SendTimeout++;
                    } else {
                        pNpScb->SendTimeout = pNpScb->SendTimeout * 3 / 2;
                        if ( pNpScb->SendTimeout > pNpScb->MaxTimeOut ) {
                            pNpScb->SendTimeout = pNpScb->MaxTimeOut;
                        }
                    }
                }

                pNpScb->TimeOut = pNpScb->SendTimeout;
                DebugTrace(+0, Dbg, "Adjusting send timeout to: %d\n", pNpScb->TimeOut );

                if ( pIrpContext->TimeoutRoutine != NULL ) {

                    pIrpContext->TimeoutRoutine( pIrpContext );

                } else {

                    DebugTrace(+0, Dbg, "Resending Packet, retry %x\n", RetryCount+1);
                    PreparePacket( pIrpContext, pIrpContext->pOriginalIrp, pIrpContext->TxMdl );

                    SetFlag( pIrpContext->Flags, IRP_FLAG_RETRY_SEND );
                    SendNow( pIrpContext );
                }

                Stats.FailedSessions++;

            } else {

                ASSERT( pIrpContext->pEx != NULL );

                //
                //  We are out of retries.
                //

                if ( BooleanFlagOn( pIrpContext->Flags, IRP_FLAG_REROUTE_ATTEMPTED ) ||
                     ( NwAbsoluteTotalWaitTime != 0  &&
                       pNpScb->TotalWaitTime >= NwAbsoluteTotalWaitTime ) ) {


                    ClearFlag( pIrpContext->Flags, IRP_FLAG_RETRY_SEND );

                    //
                    //  He have already attempted to reroute the request.
                    //  Give up.
                    //

                    DebugTrace(+0, Dbg, "Abandon Exchange\n", 0 );

                    if ( pIrpContext->pNpScb != &NwPermanentNpScb ) {

                        //
                        //  Reset to the attaching state.  If the server
                        //  is dead, the next attempt to open a handle will
                        //  fail with a better error than unexpected network
                        //  error.
                        //

                        pIrpContext->pNpScb->State = SCB_STATE_ATTACHING;

                        Error(
                            EVENT_NWRDR_TIMEOUT,
                            STATUS_UNEXPECTED_NETWORK_ERROR,
                            NULL,
                            0,
                            1,
                            pNpScb->ServerName.Buffer );
                    }

                    pIrpContext->ResponseParameters.Error = ERROR_UNEXP_NET_ERR;
                    pIrpContext->pEx( pIrpContext, 0, NULL );

                } else {

                    //
                    //  Attempt to reroute the request.
                    //

                    SetFlag( pIrpContext->Flags, IRP_FLAG_REROUTE_ATTEMPTED );

                    if ( BooleanFlagOn(
                            pIrpContext->Flags,
                            IRP_FLAG_BURST_PACKET ) ) {
                        pIrpContext->PostProcessRoutine = NewRouteBurstRetry;
                    } else {
                        pIrpContext->PostProcessRoutine = NewRouteRetry;
                    }

                    NwPostToFsp( pIrpContext, FALSE );
                }
            }

        } else {

            //
            //  Nothing to do for this SCB.  Dereference this SCB and
            //  release the spin lock.
            //

            KeReleaseSpinLockFromDpcLevel( &pNpScb->NpScbSpinLock );
            NwDereferenceScb( pNpScb );
        }

        KeAcquireSpinLockAtDpcLevel( &ScbSpinLock );

    }

    KeReleaseSpinLockFromDpcLevel( &ScbSpinLock );

    //
    //  Now see if the scavenger routine needs to be run.
    //

    KeAcquireSpinLockAtDpcLevel( &NwScavengerSpinLock );

    NwScavengerTickCount++;
    if ( NwScavengerTickCount > NwScavengerTickRunCount ) {

        PWORK_QUEUE_ITEM WorkItem;

        WorkItem = ALLOCATE_POOL( NonPagedPool, sizeof( WORK_QUEUE_ITEM ) );

        //
        //  We are about to run the ScavengerRoutine.  Reset Scavenger
        //  tick count.  If we can't even allocate a work item, the
        //  system is in bad shape, so simply defer running the
        //  ScavengerRoutine.
        //

        if ( WorkItem != NULL ) {

            ExInitializeWorkItem( WorkItem, NwScavengerRoutine, WorkItem );
            ExQueueWorkItem( WorkItem, DelayedWorkQueue );
            NwScavengerTickCount = 0;
        }
    }

    KeReleaseSpinLockFromDpcLevel( &NwScavengerSpinLock );

    //
    //  Scan the list of pending locks, looking for locks to retry.
    //

    KeAcquireSpinLockAtDpcLevel( &NwPendingLockSpinLock );

    for (IrpContextEntry = NwPendingLockList.Flink ;
         IrpContextEntry != &NwPendingLockList ;
         IrpContextEntry = NextIrpContextEntry ) {

        NextIrpContextEntry = IrpContextEntry->Flink;
        pIrpContext = CONTAINING_RECORD( IrpContextEntry, IRP_CONTEXT, NextRequest );

        if ( --pIrpContext->Specific.Lock.Key <= 0 ) {

            //
            //  Remove the IRP Context from the queue and reattempt the lock.
            //  Set the SEQUENCE_NO_REQUIRED flag so that the packet gets
            //  renumbered.
            //

            RemoveEntryList( &pIrpContext->NextRequest );
            SetFlag( pIrpContext->Flags,  IRP_FLAG_SEQUENCE_NO_REQUIRED );
            PrepareAndSendPacket( pIrpContext );
        }

    }

    KeReleaseSpinLockFromDpcLevel( &NwPendingLockSpinLock );

    //
    //  Reset the timer to run for another tick.
    //

    (VOID)KeSetTimer ( &Timer, DueTime, &NwDpc);

    //DebugTrace(-1, Dbg, "TimerDpc\n", 0);
    return;

    UNREFERENCED_PARAMETER (Dpc);
    UNREFERENCED_PARAMETER (Context);
    UNREFERENCED_PARAMETER (SystemArgument1);
    UNREFERENCED_PARAMETER (SystemArgument2);

}


