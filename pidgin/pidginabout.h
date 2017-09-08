#ifndef PIDGIN_ABOUT_H
#define PIDGIN_ABOUT_H

#include <gtk/gtk.h>

#define PIDGIN_TYPE_ABOUT_DIALOG            (pidgin_about_dialog_get_type())
#define PIDGIN_ABOUT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PIDGIN_TYPE_ABOUT_DIALOG, PidginAboutDialog))
#define PIDGIN_ABOUT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PIDGIN_TYPE_ABOUT_DIALOG, PidginAboutDialogClass))
#define PIDGIN_IS_ABOUT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PIDGIN_TYPE_ABOUT_DIALOG))
#define PIDGIN_IS_ABOUT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PIDGIN_TYPE_ABOUT_DIALOG))
#define PIDGIN_ABOUT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PIDGIN_TYPE_ABOUT_DIALOG, PidginAboutDialogClass))

typedef struct _PidginAboutDialog        PidginAboutDialog;
typedef struct _PidginAboutDialogClass   PidginAboutDialogClass;
typedef struct _PidginAboutDialogPrivate PidginAboutDialogPrivate;

G_BEGIN_DECLS

GType pidgin_about_dialog_get_type(void);
GtkWidget *pidgin_about_dialog_new(void);

G_END_DECLS

#endif /* PIDGIN_ABOUT_H */

