#include "gooroom-usbctl-application.h"
#include "gooroom-usbctl-window.h"

struct _GooroomUsbctlApp
{
  GtkApplication parent;
};

G_DEFINE_TYPE(GooroomUsbctlApp, gooroom_usbctl_app, GTK_TYPE_APPLICATION);

static gboolean
gooroom_usbctl_verify_account (char *name, __uid_t uid)
{
  FILE *fp = NULL;
  char *file_contents = NULL;
  char **passwd_contents = NULL;
  char **passwd_field = NULL;
  char **user_field = NULL;
  char **gecos = NULL;
  int i = 0;
  guint cnt = 0;
  struct stat st;
  gboolean ret = FALSE;

  fp = fopen ("/etc/passwd", "r");
  if (!fp)
    return FALSE;

  fstat (fileno (fp), &st);
  file_contents = (char *) calloc (1, st.st_size + 1);
  fread (file_contents, st.st_size, 1, fp);
  file_contents[st.st_size] = '\0';
  fclose (fp);

  passwd_contents = g_strsplit (file_contents, "\n", -1);
  for (i=0; i<g_strv_length (passwd_contents); i++)
  {
    passwd_field = g_strsplit (passwd_contents[i], ",", -1);
    cnt = g_strv_length (passwd_field);
    if (passwd_field[0])
    {
      user_field = g_strsplit (passwd_field[0], ":", -1);
      if (!strcmp (name, user_field[0]) && (uid == atoi (user_field[2]) && cnt >= 5))
      {
        gecos = g_strsplit (passwd_field[4], ":", -1);
        if (!strcmp ("gooroom-account", gecos[0]))
          ret = TRUE;
        g_strfreev (gecos);
      }
      g_strfreev (user_field);
    }
    g_strfreev (passwd_field);
    passwd_field = NULL;
  }
  g_strfreev (passwd_contents);

  if (file_contents)
  {
    free (file_contents);
    file_contents = NULL;
  }

  return ret;
}

static void
gooroom_usbctl_app_init (GooroomUsbctlApp *app)
{
}

static void
gooroom_usbctl_app_startup (GApplication *app)
{
  GtkWidget *dialog = NULL;
  GtkWidget *content = NULL;
  struct passwd *pw = getpwuid (getuid ());

  G_APPLICATION_CLASS (gooroom_usbctl_app_parent_class)->startup (app);

  if (!pw || !gooroom_usbctl_verify_account (pw->pw_name, pw->pw_uid))
  {
    dialog = gtk_dialog_new_with_buttons (_("Gooroom USB Control"),
                                          NULL,
                                          GTK_DIALOG_MODAL,
                                          _("OK"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
    gtk_container_add (GTK_CONTAINER (content), gtk_label_new (_("\nLocal-account can not use Gooroom USB Control.\n")));
    gtk_window_set_default_size (GTK_WINDOW (dialog), DIALOG_WIDTH, DIALOG_HEIGHT);
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show_all (content);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    g_application_quit (G_APPLICATION (app));
  }
}

static void
gooroom_usbctl_app_activate (GApplication *app)
{
  GooroomUsbctlAppWindow *win;

  win = gooroom_usbctl_app_window_new (GOOROOM_USBCTL_APP (app));
  gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_CENTER);
  gtk_window_present (GTK_WINDOW (win));
}

static void
gooroom_usbctl_app_class_init (GooroomUsbctlAppClass *class)
{
  G_APPLICATION_CLASS (class)->startup = gooroom_usbctl_app_startup;
  G_APPLICATION_CLASS (class)->activate = gooroom_usbctl_app_activate;
}

GooroomUsbctlApp *
gooroom_usbctl_app_new (void)
{
  return g_object_new (GOOROOM_USBCTL_APP_TYPE,
                       "application-id", "kr.gooroom.gooroom-usbctl",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
