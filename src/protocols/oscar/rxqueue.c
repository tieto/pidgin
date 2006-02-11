/*
 * This file contains the management routines for the receive
 * (incoming packet) queue.  The actual packet handlers are in
 * rxhandlers.c.
 */

#define FAIM_INTERNAL
#include <aim.h>

#ifndef _WIN32
#include <sys/socket.h>
#endif

/*
 *
 */
faim_internal int aim_recv(int fd, void *buf, size_t count)
{
	int left, cur;

	for (cur = 0, left = count; left; ) {
		int ret;

		ret = recv(fd, ((unsigned char *)buf)+cur, left, 0);

		/* Of course EOF is an error, only morons disagree with that. */
		if (ret <= 0)
			return -1;

		cur += ret;
		left -= ret;
	}

	return cur;
}

/*
 * Read into a byte stream.  Will not read more than count, but may read
 * less if there is not enough room in the stream buffer.
 */
faim_internal int aim_bstream_recv(aim_bstream_t *bs, int fd, size_t count)
{
	int red = 0;

	if (!bs || (fd < 0))
		return -1;

	if (count > (bs->len - bs->offset))
		count = bs->len - bs->offset; /* truncate to remaining space */

	if (count) {

		red = aim_recv(fd, bs->data + bs->offset, count);

		if (red <= 0)
			return -1;
	}

	bs->offset += red;

	return red;
}

/**
 * Free an aim_frame_t
 *
 * @param frame The frame to free.
 * @return -1 on error; 0 on success.
 */
faim_internal void aim_frame_destroy(aim_frame_t *frame)
{

	free(frame->data.data); /* XXX aim_bstream_free */
	free(frame);

	return;
}

/*
 * Read a FLAP header from conn into fr, and return the number of
 * bytes in the payload.
 *
 * @return -1 on error, otherwise return the length of the payload.
 */
static int aim_get_command_flap(aim_session_t *sess, aim_conn_t *conn, aim_frame_t *fr)
{
	fu8_t hdr_raw[6];
	aim_bstream_t hdr;

	fr->hdrtype = AIM_FRAMETYPE_FLAP;

	/*
	 * Read FLAP header.  Six bytes total.
	 *
	 *   Byte # | Description
	 *   -------|-------------
	 *    0x00  | Always 0x2a
	 *    0x01  | Channel number, usually "2."  "1" is used during login,
	 *          |   4 is used during logoff.
	 *    0x02  | Sequence number, 2 bytes.
	 *    0x04  | Number of data bytes that follow, 2 bytes.
	 */
	aim_bstream_init(&hdr, hdr_raw, sizeof(hdr_raw));
	if (aim_bstream_recv(&hdr, conn->fd, 6) < 6) {
		aim_conn_close(conn);
		return -1;
	}

	aim_bstream_rewind(&hdr);

	/*
	 * This shouldn't happen unless the socket breaks, the server breaks,
	 * or we break.  We must handle it just in case.
	 */
	if (aimbs_get8(&hdr) != 0x2a) {
		gaim_debug_misc("oscar", "Invalid FLAP frame received on FLAP connection!");
		aim_conn_close(conn);
		return -1;
	}

	fr->hdr.flap.channel = aimbs_get8(&hdr);
	fr->hdr.flap.seqnum = aimbs_get16(&hdr);

	return aimbs_get16(&hdr);
}

/*
 * Read a rendezvous header from conn into fr, and return the number of 
 * bytes in the payload.
 *
 * @return -1 on error, otherwise return the length of the payload.
 */
static int aim_get_command_rendezvous(aim_session_t *sess, aim_conn_t *conn, aim_frame_t *fr)
{
	fu8_t hdr_raw[8];
	aim_bstream_t hdr;

	fr->hdrtype = AIM_FRAMETYPE_OFT;

	/*
	 * Read rendezvous header
	 */
	aim_bstream_init(&hdr, hdr_raw, sizeof(hdr_raw));
	if (aim_bstream_recv(&hdr, conn->fd, 8) < 8) {
		aim_conn_close(conn);
		return -1;
	}

	aim_bstream_rewind(&hdr);

	aimbs_getrawbuf(&hdr, fr->hdr.rend.magic, 4);
	fr->hdr.rend.hdrlen = aimbs_get16(&hdr);
	fr->hdr.rend.type = aimbs_get16(&hdr);

	return fr->hdr.rend.hdrlen - 8;
}

/*
 * Grab a single command sequence off the socket, and enqueue it in
 * the incoming event queue in a separate struct.
 *
 * @return 0 on success, otherwise return the error number.
 *         "Success" doesn't mean we have new data, it just means
 *         the connection isn't dead.
 */
faim_export int aim_get_command(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *fr;
	int payloadlen;

	if (!sess || !conn)
		return -EINVAL;

	if (conn->fd == -1)
		return -1; /* it's an aim_conn_close()'d connection */

	/* If stdin is closed, then zero becomes a valid fd
	if (conn->fd < 3)
		return -1;
	*/

	if (conn->status & AIM_CONN_STATUS_INPROGRESS)
		return aim_conn_completeconnect(sess, conn);

	if (!(fr = (aim_frame_t *)calloc(sizeof(aim_frame_t), 1)))
		return -ENOMEM;

	/*
	 * Rendezvous (client to client) connections do not speak FLAP, so this 
	 * function will break on them.
	 */
	if (conn->type == AIM_CONN_TYPE_RENDEZVOUS)
		payloadlen = aim_get_command_rendezvous(sess, conn, fr);
	else if (conn->type == AIM_CONN_TYPE_LISTENER) {
		gaim_debug_misc("oscar", "AIM_CONN_TYPE_LISTENER on fd %d\n", conn->fd);
		free(fr);
		return -1;
	} else
		payloadlen = aim_get_command_flap(sess, conn, fr);

	if (payloadlen < 0) {
		free(fr);
		return -1;
	}

	if (payloadlen > 0) {
		fu8_t *payload = NULL;

		if (!(payload = (fu8_t *) malloc(payloadlen))) {
			aim_frame_destroy(fr);
			return -1;
		}

		aim_bstream_init(&fr->data, payload, payloadlen);

		/* read the payload */
		if (aim_bstream_recv(&fr->data, conn->fd, payloadlen) < payloadlen) {
			aim_frame_destroy(fr); /* free's payload */
			aim_conn_close(conn);
			return -1;
		}
	} else
		aim_bstream_init(&fr->data, NULL, 0);

	aim_bstream_rewind(&fr->data);

	fr->conn = conn;

	/* Enqueue this puppy */
	fr->next = NULL;  /* this will always be at the bottom */
	if (sess->queue_incoming == NULL)
		sess->queue_incoming = fr;
	else {
		aim_frame_t *cur;
		for (cur = sess->queue_incoming; cur->next; cur = cur->next);
		cur->next = fr;
	}

	fr->conn->lastactivity = time(NULL);

	return 0;
}

/*
 * Purge receive queue of all handled commands (->handled==1).
 *
 */
faim_export void aim_purge_rxqueue(aim_session_t *sess)
{
	aim_frame_t *cur, **prev;

	for (prev = &sess->queue_incoming; (cur = *prev); ) {
		if (cur->handled) {
			*prev = cur->next;
			aim_frame_destroy(cur);
		} else
			prev = &cur->next;
	}

	return;
}

/*
 * Since aim_get_command will aim_conn_kill dead connections, we need
 * to clean up the rxqueue of unprocessed connections on that socket.
 *
 * XXX: this is something that was handled better in the old connection
 * handling method, but eh.
 */
faim_internal void aim_rxqueue_cleanbyconn(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *currx;

	for (currx = sess->queue_incoming; currx; currx = currx->next) {
		if ((!currx->handled) && (currx->conn == conn))
			currx->handled = 1;
	}

	return;
}
