/*
   libgstroke - a GNOME stroke interface library
   Copyright (c) 1996,1997,1998,1999,2000,2001  Mark F. Willey, ETLA Technical

   See the file COPYING for distribution information.

   This file contains the stroke recognition algorithm.
*/

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "gstroke.h"
#include "gstroke-internal.h"


void
_gstroke_init (struct gstroke_metrics *metrics)
{
  if (metrics->pointList != NULL) {
    /* FIXME: does this free the data too?*/
    g_slist_free (metrics->pointList);
    metrics->pointList = NULL;
    metrics->point_count = 0;
  }
}

/* figure out which bin the point falls in */
static gint
_gstroke_bin (p_point point_p, gint bound_x_1, gint bound_x_2,
	    gint bound_y_1, gint bound_y_2)
{

  gint bin_num = 1;

  if (point_p->x > bound_x_1) bin_num += 1;
  if (point_p->x > bound_x_2) bin_num += 1;
  if (point_p->y > bound_y_1) bin_num += 3;
  if (point_p->y > bound_y_2) bin_num += 3;

  return bin_num;
}

gint
_gstroke_trans (gchar *sequence, struct gstroke_metrics *metrics)
{
  GSList *crt_elem;
  /* number of bins recorded in the stroke */
  guint sequence_count = 0;

  /* points-->sequence translation scratch variables */
  gint prev_bin = 0;
  gint current_bin = 0;
  gint bin_count = 0;

  /* flag indicating the start of a stroke - always count it in the sequence */
  gint first_bin = TRUE;

  /* bin boundary and size variables */
  gint delta_x, delta_y;
  gint bound_x_1, bound_x_2;
  gint bound_y_1, bound_y_2;


  /* determine size of grid */
  delta_x = metrics->max_x - metrics->min_x;
  delta_y = metrics->max_y - metrics->min_y;

  /* calculate bin boundary positions */
  bound_x_1 = metrics->min_x + (delta_x / 3);
  bound_x_2 = metrics->min_x + 2 * (delta_x / 3);

  bound_y_1 = metrics->min_y + (delta_y / 3);
  bound_y_2 = metrics->min_y + 2 * (delta_y / 3);

  if (delta_x > GSTROKE_SCALE_RATIO * delta_y) {
    bound_y_1 = (metrics->max_y + metrics->min_y - delta_x) / 2 + (delta_x / 3);
    bound_y_2 = (metrics->max_y + metrics->min_y - delta_x) / 2 + 2 * (delta_x / 3);
  } else if (delta_y > GSTROKE_SCALE_RATIO * delta_x) {
    bound_x_1 = (metrics->max_x + metrics->min_x - delta_y) / 2 + (delta_y / 3);
    bound_x_2 = (metrics->max_x + metrics->min_x - delta_y) / 2 + 2 * (delta_y / 3);
  }

#if 0
  printf ("DEBUG:: point count: %d\n", metrics->point_count);
  printf ("DEBUG:: metrics->min_x: %d\n", metrics->min_x);
  printf ("DEBUG:: metrics->max_x: %d\n", metrics->max_x);
  printf ("DEBUG:: metrics->min_y: %d\n", metrics->min_y);
  printf ("DEBUG:: metrics->max_y: %d\n", metrics->max_y);
  printf ("DEBUG:: delta_x: %d\n", delta_x);
  printf ("DEBUG:: delta_y: %d\n", delta_y);
  printf ("DEBUG:: bound_x_1: %d\n", bound_x_1);
  printf ("DEBUG:: bound_x_2: %d\n", bound_x_2);
  printf ("DEBUG:: bound_y_1: %d\n", bound_y_1);
  printf ("DEBUG:: bound_y_2: %d\n", bound_y_2);
#endif

  /*
    build string by placing points in bins, collapsing bins and
    discarding those with too few points...  */

  crt_elem = metrics->pointList;
  while (crt_elem != NULL)
    {
      /* figure out which bin the point falls in */

      /*printf ("X = %d Y = %d\n", ((p_point)crt_elem->data)->x,
	((p_point)crt_elem->data)->y); */


      current_bin = _gstroke_bin ((p_point)crt_elem->data, bound_x_1,
                                 bound_x_2, bound_y_1, bound_y_2);

      /* if this is the first point, consider it the previous bin, too. */
      if (prev_bin == 0)
	prev_bin = current_bin;

      /*printf ("DEBUG:: current bin: %d x=%d y = %d\n", current_bin,
	      ((p_point)crt_elem->data)->x,
	      ((p_point)crt_elem->data)->y); */

      if (prev_bin == current_bin)
	bin_count++;
      else {
	/* we are moving to a new bin -- consider adding to the sequence */
	if ((bin_count > (metrics->point_count * GSTROKE_BIN_COUNT_PERCENT))
	    || (first_bin == TRUE)) {

		/*
		gchar val = '0' + prev_bin;
		printf ("%c", val);fflush (stdout);
		g_string_append (&sequence, &val);
		*/

	  first_bin = FALSE;
	  sequence[sequence_count++] = '0' + prev_bin;
	  /*  printf ("DEBUG:: adding sequence: %d\n", prev_bin); */

	}

	/* restart counting points in the new bin */
	bin_count=0;
	prev_bin = current_bin;
      }

      /* move to next point, freeing current point from list */

      free (crt_elem->data);
      crt_elem = g_slist_next (crt_elem);
    }
  /* add the last run of points to the sequence */
  sequence[sequence_count++] = '0' + current_bin;
  /*  printf ("DEBUG:: adding final sequence: %d\n", current_bin);  */

  _gstroke_init (metrics);

  {
    /* FIXME: get rid of this block
	  gchar val = '0' + current_bin;
	  printf ("%c\n", val);fflush (stdout);
	  g_string_append (&sequence,  '\0');
	 */
    sequence[sequence_count] = '\0';
  }

  return TRUE;
}

/* my plan is to make a stroke training program where you can enter all of
the variations of slop that map to a canonical set of strokes.  When the
application calls gstroke_canonical, it gets one of the recognized strokes,
or "", if it's not a recognized variation.  I will probably use a hash
table.  Right now, it just passes the values through to gstroke_trans */
gint
_gstroke_canonical (gchar *sequence, struct gstroke_metrics *metrics)
{
  return _gstroke_trans (sequence, metrics);
}


void
_gstroke_record (gint x, gint y, struct gstroke_metrics *metrics)
{
  p_point new_point_p;
  gint delx, dely;
  float ix, iy;

  g_return_if_fail( metrics != NULL );

#if 0
  printf ("%d:%d ", x, y); fflush (stdout);
#endif

  if (metrics->point_count < GSTROKE_MAX_POINTS) {
    new_point_p = (p_point) g_malloc (sizeof (struct s_point));

    if (metrics->pointList == NULL) {

      /* first point in list - initialize metrics */
      metrics->min_x = 10000;
      metrics->min_y = 10000;
      metrics->max_x = -1;
      metrics->max_y = -1;

      metrics->pointList = (GSList*) g_malloc (sizeof (GSList));

      metrics->pointList->data = new_point_p;
      metrics->pointList->next = NULL;
      metrics->point_count = 0;

    } else {

#define LAST_POINT ((p_point)(g_slist_last (metrics->pointList)->data))

      /* interpolate between last and current point */
      delx = x - LAST_POINT->x;
      dely = y - LAST_POINT->y;

      if (abs(delx) > abs(dely)) {  /* step by the greatest delta direction */
	iy = LAST_POINT->y;

	/* go from the last point to the current, whatever direction it may be */
	for (ix = LAST_POINT->x; (delx > 0) ? (ix < x) : (ix > x); ix += (delx > 0) ? 1 : -1) {

	  /* step the other axis by the correct increment */
	  iy += fabs(((float) dely / (float) delx)) * (float) ((dely < 0) ? -1.0 : 1.0);

	  /* add the interpolated point */
	  new_point_p->x = ix;
	  new_point_p->y = iy;
	  metrics->pointList = g_slist_append (metrics->pointList, new_point_p);

	  /* update metrics */
	  if (((gint) ix) < metrics->min_x) metrics->min_x = (gint) ix;
	  if (((gint) ix) > metrics->max_x) metrics->max_x = (gint) ix;
	  if (((gint) iy) < metrics->min_y) metrics->min_y = (gint) iy;
	  if (((gint) iy) > metrics->max_y) metrics->max_y = (gint) iy;
	  metrics->point_count++;

	  new_point_p = (p_point) malloc (sizeof(struct s_point));
	}
      } else {  /* same thing, but for dely larger than delx case... */
	ix = LAST_POINT->x;

	/* go from the last point to the current, whatever direction it may be
	 */
	for (iy = LAST_POINT->y; (dely > 0) ? (iy < y) : (iy > y); iy += (dely > 0) ? 1 : -1) {

	  /* step the other axis by the correct increment */
	  ix += fabs(((float) delx / (float) dely)) * (float) ((delx < 0) ? -1.0 : 1.0);

	  /* add the interpolated point */
	  new_point_p->y = iy;
	  new_point_p->x = ix;
	  metrics->pointList = g_slist_append(metrics->pointList, new_point_p);

	  /* update metrics */
	  if (((gint) ix) < metrics->min_x) metrics->min_x = (gint) ix;
	  if (((gint) ix) > metrics->max_x) metrics->max_x = (gint) ix;
	  if (((gint) iy) < metrics->min_y) metrics->min_y = (gint) iy;
	  if (((gint) iy) > metrics->max_y) metrics->max_y = (gint) iy;
	  metrics->point_count++;

	  new_point_p = (p_point) malloc (sizeof(struct s_point));
	}
      }

      /* add the sampled point */
      metrics->pointList = g_slist_append(metrics->pointList, new_point_p);
    }

    /* record the sampled point values */
    new_point_p->x = x;
    new_point_p->y = y;

#if 0
    {
      GSList *crt = metrics->pointList;
      printf ("Record ");
      while (crt != NULL)
	{
	  printf ("(%d,%d)", ((p_point)crt->data)->x, ((p_point)crt->data)->y);
	  crt = g_slist_next (crt);
	}
      printf ("\n");
    }
#endif
  }
}
