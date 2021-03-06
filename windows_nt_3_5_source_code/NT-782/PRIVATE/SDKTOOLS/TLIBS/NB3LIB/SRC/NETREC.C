/***    netrec.c - receive data on an open circuit
*
*       Returns number of chars received, or -1 and neterrno set on failure.
*
*       Copyright (c) Microsoft Corporation, 1986
*
*/

#include "internal.h"

long
netreceive( int lsn, char *buf, unsigned int nbytes )
{
  struct ncb      ncb;
  register struct ncb     *ncbp = &ncb;

  ncbp->ncb_com = NSESREC;
  ncbp->ncb_ret = 0;
  ncbp->ncb_done = 0;
  ncbp->ncb_lnum = 0;
#ifdef REALMODE
  ncbp->ncb_sig.lp_offset = 0;
  ncbp->ncb_sig.lp_seg = 0;
#else
  ncbp->ncb_sem = NULL;
#endif
  ncbp->ncb_lsn = (unsigned char) lsn;
  ncbp->ncb_len = (unsigned short) nbytes;
#ifdef REALMODE
  ncbp->ncb_bfr.lp_offset = (unsigned short)buf;
  ncbp->ncb_bfr.lp_seg = (unsigned short)getds();
#else
  ncbp->ncb_bfr = (char FAR *) buf;
#endif

  if ( passncb(ncbp) < 0 )
    {
      /* neterrno already set by passncb */
      if (neterrno != NRC_INCOMP)
        return( -1L );
    }

  return( (long) ncbp->ncb_len );
}
