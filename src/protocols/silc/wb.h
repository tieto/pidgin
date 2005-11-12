/*

  silcgaim.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2005 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef SILCGAIM_WB_H
#define SILCGAIM_WB_H

#include "silcgaim.h"
#include "whiteboard.h"

GaimWhiteboard *
silcgaim_wb_init(SilcGaim sg, SilcClientEntry client_entry);
GaimWhiteboard *
silcgaim_wb_init_ch(SilcGaim sg, SilcChannelEntry channel);
void silcgaim_wb_receive(SilcClient client, SilcClientConnection conn,
			 SilcClientEntry sender, SilcMessagePayload payload,
			 SilcMessageFlags flags, const unsigned char *message,
			 SilcUInt32 message_len);
void silcgaim_wb_receive_ch(SilcClient client, SilcClientConnection conn,
			    SilcClientEntry sender, SilcChannelEntry channel,
			    SilcMessagePayload payload,
			    SilcMessageFlags flags,
			    const unsigned char *message,
			    SilcUInt32 message_len);
void silcgaim_wb_start(GaimWhiteboard *wb);
void silcgaim_wb_end(GaimWhiteboard *wb);
void silcgaim_wb_get_dimensions(GaimWhiteboard *wb, int *width, int *height);
void silcgaim_wb_set_dimensions(GaimWhiteboard *wb, int width, int height);
void silcgaim_wb_get_brush(GaimWhiteboard *wb, int *size, int *color);
void silcgaim_wb_set_brush(GaimWhiteboard *wb, int size, int color);
void silcgaim_wb_send(GaimWhiteboard *wb, GList *draw_list);
void silcgaim_wb_clear(GaimWhiteboard *wb);

#endif /* SILCGAIM_WB_H */
