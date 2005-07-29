/*
 * txqueue.c
 *
 * Herein lies all the management routines for the transmit (Tx) queue.
 *
 */

#define FAIM_INTERNAL
#include <aim.h>

#ifndef _WIN32
#include <sys/socket.h>
#else
#include "win32dep.h"
#endif

/*
 * Allocate a new tx frame.
 *
 * This is more for looks than anything else.
 *
 * Right now, that is.  If/when we implement a pool of transmit
 * frames, this will become the request-an-unused-frame part.
 *
 * framing = AIM_FRAMETYPE_OFT/FLAP
 * chan = channel for FLAP, hdrtype for OFT
 *
 */
faim_internal aim_frame_t *aim_tx_new(aim_session_t *sess, aim_conn_t *conn, fu8_t framing, fu16_t chan, int datalen)
{
	aim_frame_t *fr;

	if (!sess || !conn) {
		faimdprintf(sess, 0, "aim_tx_new: No session or no connection specified!\n");
		return NULL;
	}

	/* For sanity... */
	if ((conn->type == AIM_CONN_TYPE_RENDEZVOUS) || (conn->type == AIM_CONN_TYPE_LISTENER)) {
		if (framing != AIM_FRAMETYPE_OFT) {
			faimdprintf(sess, 0, "aim_tx_new: attempted to allocate inappropriate frame type for rendezvous connection\n");
			return NULL;
		}
	} else {
		if (framing != AIM_FRAMETYPE_FLAP) {
			faimdprintf(sess, 0, "aim_tx_new: attempted to allocate inappropriate frame type for FLAP connection\n");
			return NULL;
		}
	}

	if (!(fr = (aim_frame_t *)calloc(1, sizeof(aim_frame_t))))
		return NULL;

	fr->conn = conn;
	fr->hdrtype = framing;
	if (fr->hdrtype == AIM_FRAMETYPE_FLAP)
		fr->hdr.flap.channel = chan;
	else if (fr->hdrtype == AIM_FRAMETYPE_OFT)
		fr->hdr.rend.type = chan;
	else
		faimdprintf(sess, 0, "tx_new: unknown framing\n");

	if (datalen > 0) {
		fu8_t *data;

		if (!(data = (unsigned char *)malloc(datalen))) {
			aim_frame_destroy(fr);
			return NULL;
		}

		aim_bstream_init(&fr->data, data, datalen);
	}

	return fr;
}

/* 
 * This increments the tx command count, and returns the seqnum
 * that should be stamped on the next FLAP packet sent.  This is
 * normally called during the final step of packet preparation
 * before enqueuement (in aim_tx_enqueue()).
 */
static flap_seqnum_t aim_get_next_txseqnum(aim_conn_t *conn)
{
	flap_seqnum_t ret;

	ret = ++conn->seqnum;

	return ret;
}

/*
 * The overall purpose here is to enqueue the passed in command struct
 * into the outgoing (tx) queue.  Basically...
 *   1) Make a scope-irrelevant copy of the struct
 *   3) Mark as not-sent-yet
 *   4) Enqueue the struct into the list
 *   6) Return
 *
 * Note that this is only used when doing queue-based transmitting;
 * that is, when sess->tx_enqueue is set to &aim_tx_enqueue__queuebased.
 *
 */
static int aim_tx_enqueue__queuebased(aim_session_t *sess, aim_frame_t *fr)
{

	if (!fr->conn) {
		faimdprintf(sess, 1, "aim_tx_enqueue: WARNING: enqueueing packet with no connecetion\n");
		fr->conn = aim_getconn_type(sess, AIM_CONN_TYPE_BOS);
	}

	if (fr->hdrtype == AIM_FRAMETYPE_FLAP) {
		/* assign seqnum -- XXX should really not assign until hardxmit */
		fr->hdr.flap.seqnum = aim_get_next_txseqnum(fr->conn);
	}

	fr->handled = 0; /* not sent yet */

	/* see overhead note in aim_rxqueue counterpart */
	if (!sess->queue_outgoing)
		sess->queue_outgoing = fr;
	else {
		aim_frame_t *cur;
		for (cur = sess->queue_outgoing; cur->next; cur = cur->next);
		cur->next = fr;
	}

	return 0;
}

/*
 * Parallel to aim_tx_enqueue__queuebased, however, this bypasses
 * the whole queue mess when you want immediate writes to happen.
 *
 * Basically the same as its __queuebased couterpart, however
 * instead of doing a list append, it just calls aim_tx_sendframe()
 * right here. 
 * 
 */
static int aim_tx_enqueue__immediate(aim_session_t *sess, aim_frame_t *fr)
{
	int ret;

	if (!fr->conn) {
		faimdprintf(sess, 1, "aim_tx_enqueue: ERROR: packet has no connection\n");
		aim_frame_destroy(fr);
		return 0;
	}

	if (fr->hdrtype == AIM_FRAMETYPE_FLAP)
		fr->hdr.flap.seqnum = aim_get_next_txseqnum(fr->conn);

	fr->handled = 0; /* not sent yet */

	ret = aim_tx_sendframe(sess, fr);

	aim_frame_destroy(fr);

	return ret;
}

faim_export int aim_tx_setenqueue(aim_session_t *sess, int what, int (*func)(aim_session_t *, aim_frame_t *))
{

	if (what == AIM_TX_QUEUED)
		sess->tx_enqueue = &aim_tx_enqueue__queuebased;
	else if (what == AIM_TX_IMMEDIATE) 
		sess->tx_enqueue = &aim_tx_enqueue__immediate;
	else if (what == AIM_TX_USER) {
		if (!func)
			return -EINVAL;
		sess->tx_enqueue = func;
	} else
		return -EINVAL; /* unknown action */

	return 0;
}

faim_internal int aim_tx_enqueue(aim_session_t *sess, aim_frame_t *fr)
{

	/*
	 * If we want to send on a connection that is in progress, we have to force
	 * them to use the queue based version. Otherwise, use whatever they
	 * want.
	 */
	if (fr && fr->conn &&
			(fr->conn->status & AIM_CONN_STATUS_INPROGRESS)) {
		return aim_tx_enqueue__queuebased(sess, fr);
	}

	return (*sess->tx_enqueue)(sess, fr);
}

static int aim_send(int fd, const void *buf, size_t count)
{
	int left, cur;

	for (cur = 0, left = count; left; ) {
		int ret;

		ret = send(fd, ((unsigned char *)buf)+cur, left, 0);

		if (ret == -1)
			return -1;
		else if (ret == 0)
			return cur;

		cur += ret;
		left -= ret;
	}

	return cur;
}

static int aim_bstream_send(aim_bstream_t *bs, aim_conn_t *conn, size_t count)
{
	int wrote = 0;

	if (!bs || !conn || (count < 0))
		return -EINVAL;

	/* Make sure we don't send past the end of the bs */
	if (count > aim_bstream_empty(bs))
		count = aim_bstream_empty(bs); /* truncate to remaining space */

	if (count) {
		/*
		 * I need to rewrite this. "Updating the UI" doesn't make sense. The program is
		 * blocked and the UI can't redraw. We're blocking all of Gaim. We need to set
		 * up an actual txqueue and a GAIM_INPUT_WRITE callback and only write when we
		 * can. Why is this file called txqueue anyway? Lets rename it to txblock.
		 */
		if ((conn->type == AIM_CONN_TYPE_RENDEZVOUS) &&
		    (conn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM)) {
			const char *sn = aim_odc_getsn(conn);
			aim_rxcallback_t userfunc;

			while (count - wrote > 1024) {
				int ret;

				ret = aim_send(conn->fd, bs->data + bs->offset + wrote, 1024);
				if (ret > 0)
					wrote += ret;
				if (ret < 0)
					return -1;
				if ((userfunc=aim_callhandler(conn->sessv, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER)))
					userfunc(conn->sessv, NULL, sn, count-wrote>1024 ? ((double)wrote / count) : 1);
			}
		}

		if (count - wrote) {
			wrote = wrote + aim_send(conn->fd, bs->data + bs->offset + wrote, count - wrote);
		}
	}

	bs->offset += wrote;

	return wrote;
}

static int sendframe_flap(aim_session_t *sess, aim_frame_t *fr)
{
	aim_bstream_t bs;
	fu8_t *bs_raw;
	int payloadlen, err = 0, bslen;

	payloadlen = aim_bstream_curpos(&fr->data);

	if (!(bs_raw = malloc(6 + payloadlen)))
		return -ENOMEM;

	aim_bstream_init(&bs, bs_raw, 6 + payloadlen);

	/* FLAP header */
	aimbs_put8(&bs, 0x2a);
	aimbs_put8(&bs, fr->hdr.flap.channel);
	aimbs_put16(&bs, fr->hdr.flap.seqnum);
	aimbs_put16(&bs, payloadlen);

	/* payload */
	aim_bstream_rewind(&fr->data);
	aimbs_putbs(&bs, &fr->data, payloadlen);

	bslen = aim_bstream_curpos(&bs);
	aim_bstream_rewind(&bs);
	if (aim_bstream_send(&bs, fr->conn, bslen) != bslen)
		err = -errno;

	free(bs_raw); /* XXX aim_bstream_free */

	fr->handled = 1;
	fr->conn->lastactivity = time(NULL);

	return err;
}

static int sendframe_rendezvous(aim_session_t *sess, aim_frame_t *fr)
{
	aim_bstream_t bs;
	fu8_t *bs_raw;
	int payloadlen, err = 0, bslen;

	payloadlen = aim_bstream_curpos(&fr->data);

	if (!(bs_raw = malloc(8 + payloadlen)))
		return -ENOMEM;

	aim_bstream_init(&bs, bs_raw, 8 + payloadlen);

	/* Rendezvous header */
	aimbs_putraw(&bs, fr->hdr.rend.magic, 4);
	aimbs_put16(&bs, fr->hdr.rend.hdrlen);
	aimbs_put16(&bs, fr->hdr.rend.type);

	/* payload */
	aim_bstream_rewind(&fr->data);
	aimbs_putbs(&bs, &fr->data, payloadlen);

	bslen = aim_bstream_curpos(&bs);
	aim_bstream_rewind(&bs);
	if (aim_bstream_send(&bs, fr->conn, bslen) != bslen)
		err = -errno;

	free(bs_raw); /* XXX aim_bstream_free */

	fr->handled = 1;
	fr->conn->lastactivity = time(NULL);

	return err;
}

faim_internal int aim_tx_sendframe(aim_session_t *sess, aim_frame_t *fr)
{
	if (fr->hdrtype == AIM_FRAMETYPE_FLAP)
		return sendframe_flap(sess, fr);
	else if (fr->hdrtype == AIM_FRAMETYPE_OFT)
		return sendframe_rendezvous(sess, fr);

	return -1;
}

faim_export int aim_tx_flushqueue(aim_session_t *sess)
{
	aim_frame_t *cur;

	for (cur = sess->queue_outgoing; cur; cur = cur->next) {

		if (cur->handled)
			continue; /* already been sent */

		if (cur->conn && (cur->conn->status & AIM_CONN_STATUS_INPROGRESS))
			continue;

		/*
		 * And now for the meager attempt to force transmit
		 * latency and avoid missed messages.
		 */
		if ((cur->conn->lastactivity + cur->conn->forcedlatency) >= time(NULL)) {
			/*
			 * XXX should be a break! we don't want to block the
			 * upper layers
			 *
			 * XXX or better, just do this right.
			 *
			 */
			sleep((cur->conn->lastactivity + cur->conn->forcedlatency) - time(NULL));
		}

		/* XXX this should call the custom "queuing" function!! */
		aim_tx_sendframe(sess, cur);
	}

	/* purge sent commands from queue */
	aim_tx_purgequeue(sess);

	return 0;
}

/*
 * This is responsible for removing sent commands from the transmit 
 * queue. This is not a required operation, but it of course helps
 * reduce memory footprint at run time!  
 */
faim_export void aim_tx_purgequeue(aim_session_t *sess)
{
	aim_frame_t *cur, **prev;

	for (prev = &sess->queue_outgoing; (cur = *prev); ) {
		if (cur->handled) {
			*prev = cur->next;
			aim_frame_destroy(cur);
		} else
			prev = &cur->next;
	}

	return;
}

/**
 * Get rid of packets waiting for tx on a dying conn.  For now this 
 * simply marks all packets as sent and lets them disappear without 
 * warning.
 *
 * @param sess A session.
 * @param conn Connection that's dying.
 */
faim_internal void aim_tx_cleanqueue(aim_session_t *sess, aim_conn_t *conn)
{
	aim_frame_t *cur;

	for (cur = sess->queue_outgoing; cur; cur = cur->next) {
		if (cur->conn == conn)
			cur->handled = 1;
	}

	return;
}
