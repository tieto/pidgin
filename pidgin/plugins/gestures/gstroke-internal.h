/* This file is to be used internally by the libgstroke implementation.
   It should not be installed or used elsewhere.

   See the file COPYING for distribution information.
*/

#ifndef _GSTROKE_INTERNAL_H_
#define _GSTROKE_INTERNAL_H_

/* metrics for stroke, they are used while processing a stroke, this
   structure should be stored in local widget storage */
struct gstroke_metrics {
  GSList *pointList;     /* point list */
  gint min_x;
  gint min_y;
  gint max_x;
  gint max_y;
  gint point_count;
};

#define GSTROKE_METRICS "gstroke_metrics"

/* translate stroke to sequence */
gint _gstroke_trans (gchar *sequence, struct gstroke_metrics *metrics);
gint _gstroke_canonical (gchar* sequence, struct gstroke_metrics *metrics);

/* record point in stroke */
void _gstroke_record (gint x, gint y, struct gstroke_metrics *metrics);

/* initialize stroke functions */
void _gstroke_init (struct gstroke_metrics*);

/* structure for holding point data */
struct s_point {
  gint x;
  gint y;
};

typedef struct s_point *p_point;

#endif
