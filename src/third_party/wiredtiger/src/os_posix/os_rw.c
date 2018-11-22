/*-
 * Copyright (c) 2014-2015 MongoDB, Inc.
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

#include "wt_internal.h"

#if defined (MSSD_BOUNDBASED)
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h> //for ioctl
#include <stdint.h> //for uint64_t
#include <inttypes.h> //for PRI64
#include <sys/types.h>
#include <linux/fs.h> //for FIBMAP
//#include "third_party/mssd/mssd.h" //for MSSD_MAP
#include "mssd.h"
extern FILE* my_fp6;
extern MSSD_MAP* mssd_map;
extern off_t* retval;

extern int my_coll_streamid1;
extern int my_coll_streamid2;

extern int my_index_streamid1;
extern int my_index_streamid2;

extern uint64_t count1;
extern uint64_t count2;
//extern int mssdmap_get_or_append(MSSD_MAP* m, const char* key, const off_t val, off_t* retval);
#if defined(SSDM_OP6_DEBUG)
extern struct timeval start;
#endif
#endif //MSSD_BOUNDBASED

/*
 * __wt_read --
 *	Read a chunk.
 */
int
__wt_read(
    WT_SESSION_IMPL *session, WT_FH *fh, wt_off_t offset, size_t len, void *buf)
{
	size_t chunk;
	ssize_t nr;
	uint8_t *addr;

	WT_STAT_FAST_CONN_INCR(session, read_io);

	WT_RET(__wt_verbose(session, WT_VERB_FILEOPS,
	    "%s: read %" WT_SIZET_FMT " bytes at offset %" PRIuMAX,
	    fh->name, len, (uintmax_t)offset));

	/* Assert direct I/O is aligned and a multiple of the alignment. */
	WT_ASSERT(session,
	    !fh->direct_io ||
	    S2C(session)->buffer_alignment == 0 ||
	    (!((uintptr_t)buf &
	    (uintptr_t)(S2C(session)->buffer_alignment - 1)) &&
	    len >= S2C(session)->buffer_alignment &&
	    len % S2C(session)->buffer_alignment == 0));

	/* Break reads larger than 1GB into 1GB chunks. */
	for (addr = buf; len > 0; addr += nr, len -= (size_t)nr, offset += nr) {
		chunk = WT_MIN(len, WT_GIGABYTE);
		if ((nr = pread(fh->fd, addr, chunk, offset)) <= 0)
			WT_RET_MSG(session, nr == 0 ? WT_ERROR : __wt_errno(),
			    "%s read error: failed to read %" WT_SIZET_FMT
			    " bytes at offset %" PRIuMAX,
			    fh->name, chunk, (uintmax_t)offset);
	}
	return (0);
}

/*
 * __wt_write --
 *	Write a chunk.
 */
int
__wt_write(WT_SESSION_IMPL *session,
    WT_FH *fh, wt_off_t offset, size_t len, const void *buf)
{
	size_t chunk;
	ssize_t nw;
	const uint8_t *addr;

#if defined (MSSD_BOUNDBASED)
	int my_ret;
	off_t dum_off=1024;
	//int stream_id;
#if	defined(SSDM_OP6_DEBUG)
	uint64_t off_tem;
	struct timeval now;
	double time_ms;
#endif
#endif //MSSD_BOUNDBASED

	WT_STAT_FAST_CONN_INCR(session, write_io);

	WT_RET(__wt_verbose(session, WT_VERB_FILEOPS,
	    "%s: write %" WT_SIZET_FMT " bytes at offset %" PRIuMAX,
	    fh->name, len, (uintmax_t)offset));

	/* Assert direct I/O is aligned and a multiple of the alignment. */
	WT_ASSERT(session,
	    !fh->direct_io ||
	    S2C(session)->buffer_alignment == 0 ||
	    (!((uintptr_t)buf &
	    (uintptr_t)(S2C(session)->buffer_alignment - 1)) &&
	    len >= S2C(session)->buffer_alignment &&
	    len % S2C(session)->buffer_alignment == 0));

#if defined (MSSD_BOUNDBASED)
/*Boundary-based stream mapping,
 * stream-id 1: others 
 * stream-id 2: journal
 * stream-id 4: oplog 
 * stream-id 5~6: collection 
 * stream-id 7~8: index 
 * Except collection, and index  other file types are already assigned
 * stream_id in __wt_open() function
 * */
	//set stream_id depended on data types
	//We skip stream mapping for WiredTiger local files
	if (strstr(fh->name, "local") == 0) {
		if(strstr(fh->name, "collection") != 0) {
			//use logical offset instead of physical offset
			//get offset boundary according to filename
			my_ret = mssdmap_get_or_append(mssd_map, fh->name, dum_off, retval);
			if (!(*retval)){
				fprintf(my_fp6, "====> retval is 0, something is wrong, check again!\n");
				fprintf(my_fp6, "key is %s dum_off: %jd ret_val: %jd, map size: %d \n", fh->name, dum_off, *retval, mssd_map->size);
			}
			if(offset < (*retval)){
				posix_fadvise(fh->fd, offset, my_coll_streamid1, 8); //POSIX_FADV_DONTNEED=8
			}
			else {
				posix_fadvise(fh->fd, offset, my_coll_streamid2, 8); //POSIX_FADV_DONTNEED=8
			}	
		}
		else if(strstr(fh->name, "index") != 0) {
			//use logical offset instead of physical offset

			//get offset boundary according to filename
			my_ret = mssdmap_get_or_append(mssd_map, fh->name, dum_off, retval);
			if (!(*retval)){
				fprintf(my_fp6, "os_rw====> retval is 0, something is wrong, check again!\n");
				fprintf(my_fp6, "key is %s dum_off: %jd ret_val: %jd, map size: %d \n", fh->name, dum_off, *retval, mssd_map->size);
			}
			if(offset < (*retval)){
				posix_fadvise(fh->fd, offset, my_index_streamid1, 8); //POSIX_FADV_DONTNEED=8
			}
			else {
				posix_fadvise(fh->fd, offset, my_index_streamid2, 8); //POSIX_FADV_DONTNEED=8
			}	
		}
	}//end if (strstr(fh->name, "local") == 0)
#endif //MSSD_BOUNDBASED

	/* Break writes larger than 1GB into 1GB chunks. */
	for (addr = buf; len > 0; addr += nw, len -= (size_t)nw, offset += nw) {
		chunk = WT_MIN(len, WT_GIGABYTE);
		if ((nw = pwrite(fh->fd, addr, chunk, offset)) < 0)
			WT_RET_MSG(session, __wt_errno(),
			    "%s write error: failed to write %" WT_SIZET_FMT
			    " bytes at offset %" PRIuMAX,
			    fh->name, chunk, (uintmax_t)offset);
	}
	return (0);
}
