#ifndef __CC_INTERFACE_H__
#define __CC_INTERFACE_H__

#include <gtk/gtk.h>
#include "crazychat.h"

/* XXX feature encapsulation: still in flux, not stable XXX */

//charlie
typedef enum {REMOTE, LOCAL} OUTPUT;

gboolean configure(GtkWidget *widget,
				GdkEventConfigure *event, void *data);

#ifdef DISABLE_MODELS
#define draw(a, b, c)  1
#define setupDrawlists(a)
#else
gboolean draw(GtkWidget *widget, GdkEventExpose *event,
			      void *data);

void setupDrawlists(OUTPUT output);
#endif

void init (GtkWidget *widget, void *data);

void setupLighting(GtkWidget *widget);

struct cc_features {
	guint8 head_size;
	guint8 left_eye_open, right_eye_open;		/*booleans*/
	guint8 mouth_open;				/*percentage*/
	guint8 head_x_rot, head_y_rot, head_z_rot;	/* head rotation */
	guint8 x, y;					/*center of head*/
	guint8 head_color, appendage_color, lid_color, right_iris_color, left_iris_color; //colors
	guint8 mode, kind;
};

struct output_instance {
	struct cc_features *features;
	struct cc_session *session;
	float past_y;
	OUTPUT my_output;
	GtkWidget *widget;
	GtkWidget *box;
};

struct input_instance {
	int timer_id;
	struct cc_features face;
	GtkWidget *widget;
	GtkWidget *box;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *model;
	GtkWidget *head, *appendage, *lid, *right_iris, *left_iris;
	struct output_instance output;
};

struct output_handlers {
	int (*configure)(GtkWidget *widget, GdkEventConfigure *event,
			void *data);
	void (*init) (GtkWidget *widget, void *data);
	gboolean (*draw) (GtkWidget *widget, GdkEventExpose *event, void *data);
};

struct cc_session {
	struct crazychat *cc;			/**< crazychat global data */
	char *name;				/**< session peer */
	guint32 peer_ip;			/**< peer's ip addr, nbo */
	guint16 peer_port;			/**< peer's port hbo */
	struct sockaddr_in peer;		/**< peer udp sock addr */
	CC_STATE state;				/**< connection state */
	int timer_id;				/**< glib timer callback id */
	int tcp_sock;				/**< tcp socket connection */
	int udp_sock;				/**< udp socket connection */
	struct cc_features features;		/**< network peer features */
	struct output_instance *output;		/**< output instance data */
	filter_bank *filter;			/**< filter instance */
};

struct crazychat {
	guint16 tcp_port;			/**< tcp port to bind on */
	guint16 udp_port;			/**< udp session data port */
	struct cc_session_node *sessions;	/**< list of sessions */
	struct input_instance *input_data;	/**< input instance data */
	gboolean features_state;		/**< features state on/off */
};

/* --- input feature interface --- */

#ifdef _DISABLE_QT_
#define init_input(a)				NULL
#define destroy_input(a)
#else

/**
 * Initializes the input subsystem.
 * @param cc		global crazychat data structure
 * @return		pointer to an input instance
 */
struct input_instance *init_input(struct crazychat *cc);

/**
 * Destroys the input subsystem.
 * @param instance	input instance
 */
void destroy_input(struct input_instance *instance);

#endif /* _DISABLE_QT_ */

/* --- output feature interface --- */

/**
 * Initializes an output instance.
 * @param features	pointer to features
 * @param session	pointer to the crazychat session
 * @return		pointer to the output instance
 */
struct output_instance *init_output(struct cc_features *features,
		struct cc_session *session);

/**
 * Destroys an output instance.
 * @param instance	output instance
 */
void destroy_output(struct output_instance *instance);

#endif
