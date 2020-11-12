#include "gooroom-usbctl-window.h"
#include <json.h>

struct _GooroomUsbctlAppWindow {
  GtkApplicationWindow parent;
};

typedef struct _GooroomUsbctlAppWindowPrivate GooroomUsbctlAppWindowPrivate;

struct _GooroomUsbctlAppWindowPrivate
{
  int             timer_time;
  guint           timer_tag;
  guint           update_tag;

  GThread        *inotify_thr;

  GtkWidget      *user_image;
  GtkWidget      *user_name_label;
  
  GtkWidget      *usbctl_stack_switcher;
  GtkWidget      *reg_new_button;
  GtkWidget      *reg_new_info_label;
  GtkWidget      *reg_timeout_info_label;
  GtkWidget      *reg_timeout_value_label;

  GtkWidget      *reg_info_box;
  GtkWidget      *reg_name_value_entry;
  GtkEntryBuffer *reg_name_value_buffer;
  GtkWidget      *reg_product_value_label;
  GtkWidget      *reg_vendor_value_label;
  GtkWidget      *reg_serial_value_label;
  GtkWidget      *reg_model_value_label;
  GtkWidget      *reg_confirm_button;
  GtkWidget      *reg_cancel_button;

  GtkWidget      *list_page_box;
  GtkWidget      *approved_label;
  GtkWidget      *approved_value_label;
  GtkWidget      *approved_view;
  GtkWidget      *approved_list_box;
  GtkWidget      *rejected_label;
  GtkWidget      *rejected_view;
  GtkWidget      *rejected_list_box;
  GtkWidget      *waiting_label;
  GtkWidget      *waiting_view;
  GtkWidget      *waiting_list_box;
};

int              usb_num;
int              usb_exist_num;
int              usb_update_flag;
char            *login_id;
guint            sig_sub_id;
usb_info        *usbs[GOOROOM_USBCTL_USB_LIMIT];
usb_info        *new_usb;
except_info     *except;
usb_policy_info *usb_policy;
GDBusConnection *conn;

G_DEFINE_TYPE_WITH_PRIVATE(GooroomUsbctlAppWindow, gooroom_usbctl_app_window, GTK_TYPE_APPLICATION_WINDOW);

static char *
get_request_value (usb_info *usb,
                   char     *action)
{
  char *req_val = (char *) calloc (1, BUFSIZ);

  snprintf (req_val, BUFSIZ,
            "{ \"action\": \"%s\",\
               \"datetime\": \"%s\",\
               \"login_id\": \"%s\",\
               \"usb_name\": \"%s\",\
               \"usb_product\": \"%s\",\
               \"usb_size\": \"%s\",\
               \"usb_vendor\": \"%s\",\
               \"usb_serial\": \"%s\",\
               \"usb_model\": \"%s\",\
               \"req_seq\": \"%s\" }",
            action,
            usb->value[USB_INFO_TIME],
            login_id,
            usb->value[USB_INFO_NAME],
            usb->value[USB_INFO_PRODUCT],
            usb->value[USB_INFO_SIZE],
            usb->value[USB_INFO_VENDOR],
            usb->value[USB_INFO_SERIAL],
            usb->value[USB_INFO_MODEL],
            usb->value[USB_INFO_SEQ]);

  return req_val;
}

static char *
get_request_message (usb_info *usb,
                     char     *action)
{
  char *req_msg = (char *) calloc (1, BUFSIZ);
  
  snprintf (req_msg, BUFSIZ,
            "{ \"module\": {\
                 \"module_name\": \"%s\",\
                 \"task\": {\
                   \"task_name\": \"%s\",\
                   \"in\": %s \
                  }\
                }\
              }",
            "config",
            "client_event_usb_whitelist",
            get_request_value (usb, action));

  return req_msg;
}

static GtkWidget *
get_message_dialog (char *msg)
{
  GtkWidget *dialog = NULL;
  GtkWidget *content = NULL;
  dialog = gtk_dialog_new_with_buttons (_("Gooroom USB Control"),
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        _("OK"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (content), gtk_label_new (msg));
  gtk_window_set_default_size (GTK_WINDOW (dialog),
                               DIALOG_WIDTH,
                               DIALOG_HEIGHT);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_widget_show_all (content);

  return dialog;
}

static GtkWidget *
get_button_dialog (char *msg)
{
  GtkWidget *dialog = NULL;
  GtkWidget *content = NULL;

  dialog = gtk_dialog_new_with_buttons (_("Gooroom USB Control"),
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        _("YES"),
                                        GTK_RESPONSE_ACCEPT,
                                        _("NO"),
                                        GTK_RESPONSE_REJECT,
                                        NULL);
  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  gtk_container_add (GTK_CONTAINER (content), gtk_label_new (msg));
  gtk_window_set_default_size (GTK_WINDOW (dialog),
                               DIALOG_WIDTH,
                               DIALOG_HEIGHT);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_widget_show_all (content);

  return dialog;
}

char *
get_state_string (char *state)
{
  if (!strcmp (state, "registering"))
    return _("registering");
  else if (!strcmp (state, "register-approval"))
    return _("register-approval");
  else if (!strcmp (state, "unregistering"))
    return _("unregistering");
  else if (!strcmp (state, "registering-cancel"))
    return _("registering-cancel");
  else if (!strcmp (state, "register-deny"))
    return _("register-deny");
  else if (!strcmp (state, "unregister-deny"))
    return _("unregister-deny");
  else if (!strcmp (state, "register-approval-cancel"))
    return _("register-approval-cancel");
  else if (!strcmp (state, "unregister-approval"))
    return _("unregister-approval");
  else if (!strcmp (state, "register-deny-item-remove"))
    return _("register-deny-item-remove");
  else
    return _("unknown-state");
}

static void
reg_reset (GooroomUsbctlAppWindow *win)
{
  GooroomUsbctlAppWindowPrivate *priv;
  priv = gooroom_usbctl_app_window_get_instance_private (win);

  if (priv->timer_tag)
  {
    g_source_remove (priv->timer_tag);
    priv->timer_tag = 0;
  }
  gtk_widget_hide (priv->reg_new_info_label);
  gtk_widget_hide (priv->reg_timeout_info_label);
  gtk_widget_hide (priv->reg_timeout_value_label);
}

static void
hide_new_usb_info (gpointer user_data)
{
  GooroomUsbctlAppWindowPrivate *priv;

  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  gtk_entry_set_text (GTK_ENTRY (priv->reg_name_value_entry), "");
  gtk_widget_hide (priv->reg_name_value_entry);
  gtk_widget_hide (priv->reg_product_value_label);
  gtk_widget_hide (priv->reg_vendor_value_label);
  gtk_widget_hide (priv->reg_serial_value_label);
  gtk_widget_hide (priv->reg_model_value_label);
}

static void
reg_cancel_button_clicked (GtkButton *widget,
                           gpointer   user_data)
{
  GooroomUsbctlAppWindowPrivate *priv;

  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  if (sig_sub_id)
    g_dbus_connection_signal_unsubscribe (conn, sig_sub_id);
  reg_reset (GOOROOM_USBCTL_APP_WINDOW (user_data));
  gtk_widget_set_sensitive (priv->reg_confirm_button, FALSE);
  gtk_widget_set_sensitive (priv->reg_cancel_button, FALSE);
  if (new_usb)
  {
    free (new_usb);
    new_usb = NULL;
  }
  if (except)
  {
    free (except);
    except = NULL;
  }
  hide_new_usb_info (user_data);
}

static gboolean
timeout_handler (GooroomUsbctlAppWindow *win)
{
  GooroomUsbctlAppWindowPrivate *priv;
  char time_str[10];
  GtkWidget *dialog = NULL;
  char buf[BUFSIZ] = { 0, };


  priv = gooroom_usbctl_app_window_get_instance_private (win);
  if (priv->timer_time >= 10)
    snprintf (time_str, 10, "00:%d", priv->timer_time--);
  else
    snprintf (time_str, 10, "00:0%d", priv->timer_time--);
  gtk_label_set_text (GTK_LABEL (priv->reg_timeout_value_label), time_str); 
  gtk_widget_show (priv->reg_timeout_value_label);
  if (new_usb)
  {
    gtk_label_set_text (GTK_LABEL (priv->reg_product_value_label),
                        new_usb->value[USB_INFO_PRODUCT]);
    gtk_label_set_text (GTK_LABEL (priv->reg_vendor_value_label),
                        new_usb->value[USB_INFO_VENDOR]);
    gtk_label_set_text (GTK_LABEL (priv->reg_serial_value_label),
                        new_usb->value[USB_INFO_SERIAL]);
    gtk_label_set_text (GTK_LABEL (priv->reg_model_value_label),
                        new_usb->value[USB_INFO_MODEL]);
    gtk_widget_show_all (priv->reg_info_box);
    gtk_widget_set_focus_on_click (priv->reg_confirm_button, TRUE);
    reg_reset (win);
    gtk_label_set_text (GTK_LABEL (priv->reg_timeout_value_label),
                        _("** USB Name Required. **")); 
    gtk_widget_show (priv->reg_timeout_value_label);
  }
  else if (except) 
  {
    snprintf (buf, BUFSIZ,
              _("\nError Code: %s\n%s\n\n"),
              except->error_code,
              except->except_msg);
    dialog = get_message_dialog (buf);
    reg_cancel_button_clicked (GTK_BUTTON (priv->reg_cancel_button), win);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  }
  else if (priv->timer_time >= 0)
    return TRUE;
  else
    reg_cancel_button_clicked (GTK_BUTTON (priv->reg_cancel_button), win);

  return FALSE;
}

static void
take_new_usb (GDBusConnection *conn,
              const gchar     *sender,
              const gchar     *obj,
              const gchar     *inf,
              const gchar     *sig,
              GVariant        *param,
              gpointer         user_data)
{
  gchar *message = NULL;
  gchar **msg_field = NULL;
  gboolean valid_signal = FALSE;

  g_variant_get (param, "(s)", &message);
  msg_field = g_strsplit (message, ",", 0);
  if (!strcmp (msg_field[USB_SIGNAL_TYPE], "normal"))
  {
    new_usb = (usb_info *) calloc (1, sizeof (usb_info));
    login_id = g_strdup (msg_field[USB_SIGNAL_LOGIN_ID]);
    new_usb->value[USB_INFO_NAME] = NULL;
    new_usb->value[USB_INFO_PRODUCT] = g_strdup (msg_field[USB_SIGNAL_USB_PRODUCT]);
    new_usb->value[USB_INFO_SIZE] = g_strdup (msg_field[USB_SIGNAL_USB_SIZE]);
    new_usb->value[USB_INFO_VENDOR] = g_strdup (msg_field[USB_SIGNAL_USB_VENDOR]);
    new_usb->value[USB_INFO_SERIAL] = g_strdup (msg_field[USB_SIGNAL_USB_SERIAL]);
    new_usb->value[USB_INFO_MODEL] = g_strdup (msg_field[USB_SIGNAL_USB_MODEL]);
    new_usb->value[USB_INFO_SEQ] = g_strdup ("");
    valid_signal = TRUE;
  }
  else if (!strcmp (msg_field[USB_SIGNAL_TYPE], "except"))
  {
    except = (except_info *) calloc (1, sizeof (except_info));
    except->error_code = g_strdup (msg_field[EXCEPT_ERROR_CODE]);
    switch (atoi (except->error_code))
    {
      case 101:
        except->except_msg = g_strdup (EXCEPT_101);
        break;
      case 102:
        except->except_msg = g_strdup (EXCEPT_102);
        break;
      case 103:
        except->except_msg = g_strdup (EXCEPT_103);
        break;
      case 104:
        except->except_msg = g_strdup (EXCEPT_104);
        break;
      default:
        except->except_msg = g_strdup (msg_field[EXCEPT_EXCEPT_MSG]);
        break;
    }
    valid_signal = TRUE;
  }

  if (valid_signal)
    g_dbus_connection_signal_unsubscribe (conn, sig_sub_id);
}

static void
waiting_usb_signal (void)
{
  if (new_usb)
  {
    free (new_usb);
    new_usb = NULL;
  }
  if (except)
  {
    free (except);
    except = NULL;
  }

  sig_sub_id = g_dbus_connection_signal_subscribe (conn,
                                                   NULL,
                                                   NULL,
                                                   "media_usb_info",
                                                   NULL,
                                                   NULL,
                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                   take_new_usb,
                                                   NULL,
                                                   NULL);
}

static void
entry_buffer_deleted (GtkEntryBuffer *buffer,
                      guint           position,
                      guint           n_chars,
                      gpointer        user_data)
{
  GooroomUsbctlAppWindowPrivate *priv;

  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  if ((strlen (gtk_entry_buffer_get_text (buffer))) <= 0)
  {
    gtk_widget_set_sensitive (priv->reg_confirm_button, FALSE);
    if (new_usb)
      gtk_widget_show (priv->reg_timeout_value_label);
  }
}

static void
entry_buffer_inserted (GtkEntryBuffer *buffer,
                       guint           position,
                       char           *chars,
                       guint           n_chars,
                       gpointer        user_data)
{
  GooroomUsbctlAppWindowPrivate *priv;

  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  if ((strlen (gtk_entry_buffer_get_text (buffer))) > 0)
  {
    gtk_widget_set_sensitive (priv->reg_confirm_button, TRUE);
    gtk_widget_hide (priv->reg_timeout_value_label);
  }
}

static void
reg_new_button_clicked (GtkButton *widget,
                        gpointer   user_data)
{
  GooroomUsbctlAppWindowPrivate *priv;

  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  hide_new_usb_info (user_data);
  reg_reset (GOOROOM_USBCTL_APP_WINDOW (user_data));
  if (sig_sub_id)
    g_dbus_connection_signal_unsubscribe (conn, sig_sub_id);
  gtk_widget_show (priv->reg_new_info_label);
  gtk_widget_show (priv->reg_timeout_info_label);
  gtk_label_set_text (GTK_LABEL (priv->reg_timeout_value_label), "01:00"); 
  gtk_widget_show (priv->reg_timeout_value_label);
  gtk_widget_set_sensitive (priv->reg_confirm_button, FALSE);
  gtk_widget_set_sensitive (priv->reg_cancel_button, TRUE);
  priv->timer_time = 59;
  priv->timer_tag = g_timeout_add (1000,
                                   G_SOURCE_FUNC (timeout_handler),
                                   GOOROOM_USBCTL_APP_WINDOW (user_data));
  waiting_usb_signal ();
}

static gboolean
check_registering (char *serial)
{
  int i;

  for (i=0; i<usb_num; i++)
  {
    if (!strcmp (serial, usbs[i]->value[USB_INFO_SERIAL]))
    {
      if (!strcmp ("registering", usbs[i]->value[USB_INFO_STATE]))
        return TRUE;
      else
        return FALSE;
    }
  }
  return FALSE;
}

static void
reg_confirm_button_clicked (GtkButton *widget,
                            gpointer   user_data)
{
  gchar *time = NULL;
  GDateTime *ltime = NULL;
  int result;
  GVariant *ret_msg = NULL;
  GVariant *ret_val = NULL;
  char *arg = NULL;
  GtkWidget *dialog = NULL;
  char buf[BUFSIZ] = { 0, };
  const gchar *msg;
  const char *error_code;
  GError *error = NULL;
  json_object *root_obj = NULL;
  json_object *module_obj = NULL;
  json_object *task_obj = NULL;
  json_object *out_obj = NULL;
  json_object *status_obj = NULL;
  json_object *error_obj = NULL;
  json_object *message_obj = NULL;
  GooroomUsbctlAppWindowPrivate *priv;
  
  priv = gooroom_usbctl_app_window_get_instance_private (GOOROOM_USBCTL_APP_WINDOW (user_data));

  ltime = g_date_time_new_now_local ();
  time = g_date_time_format (ltime, "%F %H:%M:%S");

  new_usb->value[USB_INFO_NAME] = gtk_entry_get_text (GTK_ENTRY (priv->reg_name_value_entry));
  new_usb->value[USB_INFO_TIME] = g_strdup (time);

  g_date_time_unref (ltime);
  ltime = NULL;

  snprintf (buf, BUFSIZ,
            _("\nUSB NAME: %s\n\nRegister this USB?\n"),
            new_usb->value[USB_INFO_NAME]);

  dialog = get_button_dialog (buf);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    if (usb_exist_num < atoi (usb_policy->value[USB_POLICY_INFO_USB_LIMIT]))
    {
      if (!check_registering (new_usb->value[USB_INFO_SERIAL]))
      {
        arg = get_request_message (new_usb, "registering");
        ret_msg = g_dbus_connection_call_sync (conn,
                                               AGENT_DBUS_NAME,
                                               AGENT_DBUS_OBJ,
                                               AGENT_DBUS_IFACE,
                                               "do_task",
                                               g_variant_new ("(s)", arg),
                                               G_VARIANT_TYPE ("(v)"),
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
        if (error)
        {
          snprintf (buf, BUFSIZ,
                    _("\n[%s Failure]\n\n%s\n"),
                    _("registering"),
                    _("No response from Agent.\n"));
          g_error_free (error);
        }
        else if (ret_msg)
        {
          g_variant_get (ret_msg, "(v)", &ret_val);
          msg = g_variant_dup_string (ret_val, NULL);
          root_obj = json_tokener_parse (msg);
          json_object_object_get_ex (root_obj, "module", &module_obj);
          json_object_object_get_ex (module_obj, "task", &task_obj);
          json_object_object_get_ex (task_obj, "out", &out_obj);
          json_object_object_get_ex (out_obj, "status", &status_obj);
          json_object_object_get_ex (out_obj, "message", &message_obj);
          if (!strcmp (json_object_get_string (status_obj), "200"))
            snprintf (buf, BUFSIZ,
                      _("\n[%s Success]\n"),
                      _("registering"));
          else
          {
            json_object_object_get_ex (out_obj, "errorcode", &error_obj);
            if (error_obj)
            {
              error_code = json_object_get_string (error_obj);
              if (!strcmp (error_code, "601")) 
                snprintf (buf, BUFSIZ,
                          _("\n[%s Failure]\n\n%s\n"),
                          _("registering"),
                          ERROR_601);
              else if (!strcmp (error_code, "602")) 
                snprintf (buf, BUFSIZ,
                          _("\n[%s Failure]\n\n%s\n"),
                          _("registering"),
                          ERROR_602);
              else 
                snprintf (buf, BUFSIZ,
                          _("\n[%s Failure]\n\n%s\n"),
                          _("registering"),
                          ERROR_DEFAULT);
            }
            else
              snprintf (buf, BUFSIZ,
                        _("\n[%s Failure]\n\n%s\n"),
                        _("registering"),
                        ERROR_DEFAULT);
          }
          g_variant_unref (ret_msg);
          g_variant_unref (ret_val);
        }
        dialog = get_message_dialog (buf);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        return;
      }
      else
        snprintf (buf, BUFSIZ,
                 _("\n[USB Register Request Failed]\n\nUSB Serial\n(%s) is already registering.\n"),
                 new_usb->value[USB_INFO_SERIAL]);
    }
    else
      snprintf (buf, BUFSIZ,
               _("\n[USB Register Request Failed]\n\nUSB Registration Approval Limit Exceeded.\n"));

    dialog = get_message_dialog (buf);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
  }
}

static void
delete_button_clicked (GtkButton *widget,
                       gpointer   user_data)
{
  int usb_idx = GPOINTER_TO_INT (user_data);
  char *req = NULL;
  char *arg = NULL;
  int result;
  GVariant *ret_msg = NULL;
  GVariant *ret_val = NULL;
  GError *error = NULL;
  char *error_msg = NULL;
  const gchar *msg;
  const char *error_code;
  GtkWidget *dialog = NULL;
  char buf[BUFSIZ] = { 0, };
  json_object *root_obj = NULL;
  json_object *module_obj = NULL;
  json_object *task_obj = NULL;
  json_object *out_obj = NULL;
  json_object *status_obj = NULL;
  json_object *error_obj = NULL;
  json_object *message_obj = NULL;

  if ((!strcmp (usbs[usb_idx]->value[USB_INFO_STATE], "register-approval")) ||
      (!strcmp (usbs[usb_idx]->value[USB_INFO_STATE], "unregister-deny")))
  {
    snprintf (buf, BUFSIZ,
              _("\nUSB NAME: %s\nUSB STATE : %s\n\nUnregister Registered USB?\n"),
              usbs[usb_idx]->value[USB_INFO_NAME],
              get_state_string (usbs[usb_idx]->value[USB_INFO_STATE]));
    req = "unregistering";
  }
  else if (!strcmp (usbs[usb_idx]->value[USB_INFO_STATE], "registering"))
  {
    snprintf (buf, BUFSIZ,
              _("\nUSB NAME: %s\nUSB STATE : %s\n\nCancel Registering?\n"),
              usbs[usb_idx]->value[USB_INFO_NAME],
              get_state_string (usbs[usb_idx]->value[USB_INFO_STATE]));
    req = "registering-cancel";
  }
  else if ((!strcmp (usbs[usb_idx]->value[USB_INFO_STATE], "register-deny")) ||
           (!strcmp (usbs[usb_idx]->value[USB_INFO_STATE], "register-approval-cancel")))
  {
    snprintf (buf, BUFSIZ,
              _("\nUSB NAME: %s\nUSB STATE : %s\n\nDelete Register-Denied USB?\n"),
              usbs[usb_idx]->value[USB_INFO_NAME],
              get_state_string (usbs[usb_idx]->value[USB_INFO_STATE]));
    req = "register-deny-item-remove";
  }

  dialog = get_button_dialog (buf);
  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  if (result == GTK_RESPONSE_ACCEPT)
  {
    arg = get_request_message (usbs[usb_idx], req);
    ret_msg = g_dbus_connection_call_sync (conn,
                                           AGENT_DBUS_NAME,
                                           AGENT_DBUS_OBJ,
                                           AGENT_DBUS_IFACE,
                                           "do_task",
                                           g_variant_new ("(s)", arg),
                                           G_VARIANT_TYPE ("(v)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);
    if (error)
    {
      snprintf (buf, BUFSIZ,
                _("\n[%s Failure]\n\n%s\n"),
                get_state_string (req),
                _("No response from Agent.\n"));
      g_error_free (error);
    }
    else if (ret_msg)
    {
      g_variant_get (ret_msg, "(v)", &ret_val);
      msg = g_variant_dup_string (ret_val, NULL);
      root_obj = json_tokener_parse (msg);
      json_object_object_get_ex (root_obj, "module", &module_obj);
      json_object_object_get_ex (module_obj, "task", &task_obj);
      json_object_object_get_ex (task_obj, "out", &out_obj);
      json_object_object_get_ex (out_obj, "status", &status_obj);
      json_object_object_get_ex (out_obj, "message", &message_obj);
      if (!strcmp (json_object_get_string (status_obj), "200"))
        snprintf (buf, BUFSIZ,
                  _("\n[%s Success]\n"),
                  get_state_string (req));
      else
      {
        json_object_object_get_ex (out_obj, "errorcode", &error_obj);
        if (error_obj)
        {
          error_code = json_object_get_string (error_obj);
          if (!strcmp (error_code, "601")) 
            error_msg = ERROR_601;
          else if (!strcmp (error_code, "602")) 
            error_msg = ERROR_601;
          else 
            error_msg = ERROR_DEFAULT;
        }
        else
          error_msg = ERROR_DEFAULT;
        
        snprintf (buf, BUFSIZ,
                  _("\n[%s Failure]\n\n%s\n"),
                  get_state_string (req),
                  error_msg);
      }

      dialog = get_message_dialog (buf);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_variant_unref (ret_msg);
      g_variant_unref (ret_val);
    }
  }
}

static void
details_button_clicked (GtkButton *widget,
                        gpointer   user_data)
{
  GtkWidget *dialog = NULL;
  char buf[BUFSIZ] = { 0, };
  int usb_idx = GPOINTER_TO_INT (user_data);

  snprintf (buf, BUFSIZ,
            _("\nNAME: %s\nSTATE: %s\nPRODUCT: %s\nVENDOR: %s\nSERIAL: %s\nMODEL: %s\n"),
            usbs[usb_idx]->value[USB_INFO_NAME],
            get_state_string (usbs[usb_idx]->value[USB_INFO_STATE]),
            usbs[usb_idx]->value[USB_INFO_PRODUCT],
            usbs[usb_idx]->value[USB_INFO_VENDOR],
            usbs[usb_idx]->value[USB_INFO_SERIAL],
            usbs[usb_idx]->value[USB_INFO_MODEL]);

  dialog = get_message_dialog (buf);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
make_usb_list (GooroomUsbctlAppWindow *win)
{
  GooroomUsbctlAppWindowPrivate *priv;
  GtkWidget *usb = NULL;
  GtkWidget *info = NULL;
  GtkWidget *button = NULL;
  GtkWidget *button_image = NULL;
  int approved_num = 0;
  int rejected_num = 0;
  int waiting_num = 0;
  int i;
  char buf[BUFSIZ];
  char *markup;
  char *markup_color;
  GdkRGBA box_color;

  priv = gooroom_usbctl_app_window_get_instance_private (win);

  gdk_rgba_parse (&box_color, "#FFFFFF");
  priv->approved_list_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_override_background_color (priv->approved_list_box,
                                        GTK_STATE_FLAG_NORMAL,
                                        &box_color);
  gtk_container_add (GTK_CONTAINER (priv->approved_view), priv->approved_list_box);
  priv->rejected_list_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_override_background_color (priv->rejected_list_box,
                                        GTK_STATE_FLAG_NORMAL,
                                        &box_color);
  gtk_container_add (GTK_CONTAINER (priv->rejected_view), priv->rejected_list_box);
  priv->waiting_list_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_override_background_color (priv->waiting_list_box,
                                        GTK_STATE_FLAG_NORMAL,
                                        &box_color);
  gtk_container_add (GTK_CONTAINER (priv->waiting_view), priv->waiting_list_box);

  if (usb_policy)
    free (usb_policy);
  usb_policy = (usb_policy_info *) calloc (1, sizeof (usb_policy_info));
  if (gooroom_usbctl_get_usb_policy_info (usb_policy) == USB_POLICY_INFO_SET_FAILURE)
    usb_policy->value[USB_POLICY_INFO_USB_LIMIT] = "?";

  usb_exist_num = 0;
  usb_num = gooroom_usbctl_get_usb_infos (usbs);
  if (usb_num > 0)
  {
    for (i=0; i<usb_num; i++)
    {
      usb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_widget_override_background_color (usb,
                                            GTK_STATE_FLAG_NORMAL,
                                            &box_color);
      info = gtk_label_new (usbs[i]->value[USB_INFO_TIME]);
      gtk_box_pack_start (GTK_BOX (usb), info, FALSE, FALSE, 1);
      info = gtk_label_new (usbs[i]->value[USB_INFO_NAME]);
      gtk_box_pack_start (GTK_BOX (usb), info, FALSE, FALSE, 1);
      info = gtk_label_new (usbs[i]->value[USB_INFO_SERIAL]);
      gtk_box_pack_start (GTK_BOX (usb), info, FALSE, FALSE, 1);
      if (!strcmp (usbs[i]->value[USB_INFO_STATE], "unregistering"))
      {
        info = gtk_label_new (_("unregistering..."));
        gtk_box_pack_start (GTK_BOX (usb), info, FALSE, FALSE, 1);
      }

      button = gtk_button_new ();
      button_image = gtk_image_new ();
      gtk_image_set_from_icon_name (GTK_IMAGE (button_image), "remove", GTK_ICON_SIZE_BUTTON);
      gtk_button_set_image (GTK_BUTTON (button), button_image);
      gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (delete_button_clicked),
                        GINT_TO_POINTER (i));
      gtk_box_pack_end (GTK_BOX (usb), button, FALSE, FALSE, 1);
      if (!strcmp (usbs[i]->value[USB_INFO_STATE], "unregistering"))
        gtk_widget_set_sensitive (button, FALSE);

      button = gtk_button_new ();
      button_image = gtk_image_new ();
      gtk_image_set_from_icon_name (GTK_IMAGE (button_image),
                                    "dialog-information",
                                    GTK_ICON_SIZE_BUTTON);
      gtk_button_set_image (GTK_BUTTON (button), button_image);
      gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
      g_signal_connect (button,
                        "clicked",
                        G_CALLBACK (details_button_clicked),
                        GINT_TO_POINTER (i));
      gtk_box_pack_end (GTK_BOX (usb), button, FALSE, FALSE, 1);

      if ((!strcmp (usbs[i]->value[USB_INFO_STATE], "register-approval")) ||
          (!strcmp (usbs[i]->value[USB_INFO_STATE], "unregistering")) ||
          (!strcmp (usbs[i]->value[USB_INFO_STATE], "unregister-deny")))
      {
        gtk_box_pack_start (GTK_BOX (priv->approved_list_box), usb, FALSE, FALSE, 0);
        approved_num++;
        usb_exist_num++;
      }
      else if ((!strcmp (usbs[i]->value[USB_INFO_STATE], "register-deny")) ||
               (!strcmp (usbs[i]->value[USB_INFO_STATE], "register-approval-cancel")))
      {
        gtk_box_pack_start (GTK_BOX (priv->rejected_list_box), usb, FALSE, FALSE, 0);
        rejected_num++;
      }
      else if (!strcmp (usbs[i]->value[USB_INFO_STATE], "registering"))
      {
        gtk_box_pack_start (GTK_BOX (priv->waiting_list_box), usb, FALSE, FALSE, 0);
        waiting_num++;
        usb_exist_num++;
      }
    }
  }
  gtk_label_set_text (GTK_LABEL (priv->approved_label), _("Approved"));

  if (strcmp ("?", usb_policy->value[USB_POLICY_INFO_USB_LIMIT])
      && (approved_num > atoi (usb_policy->value[USB_POLICY_INFO_USB_LIMIT])))
    markup_color = "red";
  else
    markup_color = "black";
  
  markup = g_markup_printf_escaped ("(<span color=\"%s\">%d</span>/%s)",
                                    markup_color,
                                    approved_num,
                                    usb_policy->value[USB_POLICY_INFO_USB_LIMIT]);
  gtk_label_set_markup (GTK_LABEL (priv->approved_value_label), markup);
  snprintf (buf, BUFSIZ,
            _("Rejected(%d)"),
            rejected_num);
  gtk_label_set_text (GTK_LABEL (priv->rejected_label), buf);
  snprintf (buf, BUFSIZ,
            _("Waiting(%d)"),
            waiting_num);
  gtk_label_set_text (GTK_LABEL (priv->waiting_label), buf);
  gtk_widget_show_all (priv->list_page_box);
}

static void
destroy_usb_list (GooroomUsbctlAppWindow *win)
{
  GooroomUsbctlAppWindowPrivate *priv;
  int i;

  priv = gooroom_usbctl_app_window_get_instance_private (win);

  gtk_widget_destroy (priv->approved_list_box);
  gtk_widget_destroy (priv->rejected_list_box);
  gtk_widget_destroy (priv->waiting_list_box);

  for (i=usb_num-1; i>=0; i--)
  {
    free (usbs[i]);
    usbs[i] = NULL;
  }
}

static gpointer
file_modify_check (gpointer user_data)
{
  int fd;
  int wd[2];
  char buf[BUFSIZ] __attribute__ ((aligned(__alignof__(struct inotify_event))));
  const struct inotify_event *event;
  ssize_t len;
  char *ptr;

  fd = inotify_init1 (IN_CLOEXEC);
  wd[0] = inotify_add_watch (fd, USB_INFO_FILE, IN_MODIFY);
  wd[1] = inotify_add_watch (fd, USB_POLICY_INFO_FILE, IN_MODIFY);

  for (;;)
  {
    len = read (fd, buf, sizeof (buf));
    if ((len == -1 && errno != EAGAIN) || (len <= 0))
      continue;
    for (ptr=buf; ptr<buf+len; ptr+=sizeof(struct inotify_event)+event->len)
    {
      event = (const struct inotify_event *) ptr;
      if (event->mask & IN_MODIFY)
        usb_update_flag = TRUE;
    }
  }
  inotify_rm_watch (fd, wd[0]);
  inotify_rm_watch (fd, wd[1]);
  close (fd);

  return NULL;
}

static gboolean
update_usb_list (GooroomUsbctlAppWindow *win)
{
  if (usb_update_flag)
  {
    destroy_usb_list (win);
    make_usb_list (win);
    usb_update_flag = FALSE;
  }

  return TRUE;
}

static void
gooroom_usbctl_app_window_init (GooroomUsbctlAppWindow *win)
{
  GooroomUsbctlAppWindowPrivate *priv;
  GdkRGBA box_color;
  GdkPixbuf *image_buf = NULL;
  gchar *user_image_file = NULL;
  GKeyFile *user_key_file = NULL;
  gchar *user_account_file = NULL;
  struct passwd *pw = getpwuid (getuid ());

  priv = gooroom_usbctl_app_window_get_instance_private (win);
  gtk_widget_init_template (GTK_WIDGET (win));

  make_usb_list (win);

  conn = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  if (pw)
  {
    gtk_label_set_text (GTK_LABEL (priv->user_name_label), pw->pw_name);
    user_key_file = g_key_file_new ();
    user_account_file = g_strconcat ("/var/lib/AccountsService/users/",
                                     pw->pw_name, NULL);
    if (g_key_file_load_from_file (user_key_file, user_account_file,
                                   G_KEY_FILE_NONE, NULL))
    {
      user_image_file = g_key_file_get_string (user_key_file,
                                               "User", "Icon", NULL);
      if (user_image_file)
      {
        image_buf = gdk_pixbuf_new_from_file_at_size (user_image_file,
                                                      100, 100, NULL);
        gtk_image_set_from_pixbuf (GTK_IMAGE (priv->user_image), image_buf);
      }
    }
    g_key_file_free (user_key_file);
    g_free (user_account_file);
  }
  else
    gtk_label_set_text (GTK_LABEL (priv->user_name_label), "UNKNOWN");

  hide_new_usb_info (win);
  gtk_box_set_homogeneous (GTK_BOX (priv->usbctl_stack_switcher), TRUE);
  gtk_entry_set_alignment (GTK_ENTRY (priv->reg_name_value_entry), 0.5);
  priv->reg_name_value_buffer = gtk_entry_buffer_new ("", -1);
  gtk_entry_buffer_set_max_length (GTK_ENTRY_BUFFER (priv->reg_name_value_buffer), 14);
  gtk_entry_set_buffer (GTK_ENTRY (priv->reg_name_value_entry),
                        GTK_ENTRY_BUFFER (priv->reg_name_value_buffer));
  gdk_rgba_parse (&box_color, "#FFFFFF");
  gtk_widget_override_background_color (priv->reg_info_box,
                                        GTK_STATE_FLAG_NORMAL,
                                        &box_color);
  gtk_widget_hide (priv->reg_new_info_label);
  gtk_widget_hide (priv->reg_timeout_info_label);
  gtk_widget_hide (priv->reg_timeout_value_label);
  gtk_widget_set_sensitive (priv->reg_confirm_button, FALSE);
  gtk_widget_set_sensitive (priv->reg_cancel_button, FALSE);
  g_signal_connect (priv->reg_name_value_buffer,
                    "inserted-text",
                    G_CALLBACK (entry_buffer_inserted),
                    win);
  g_signal_connect (priv->reg_name_value_buffer,
                    "deleted-text",
                    G_CALLBACK (entry_buffer_deleted),
                    win);
  g_signal_connect (priv->reg_new_button,
                    "clicked",
                    G_CALLBACK (reg_new_button_clicked),
                    win);
  g_signal_connect (priv->reg_confirm_button,
                    "clicked",
                    G_CALLBACK (reg_confirm_button_clicked),
                    win);
  g_signal_connect (priv->reg_cancel_button,
                    "clicked",
                    G_CALLBACK (reg_cancel_button_clicked),
                    win);
  priv->inotify_thr = g_thread_new (NULL,
                                    &file_modify_check,
                                    win);
  usb_update_flag = FALSE;
  priv->update_tag = g_timeout_add (1000,
                                    G_SOURCE_FUNC (update_usb_list),
                                    win);
}

static void
gooroom_usbctl_app_window_dispose (GObject *object)
{
  G_OBJECT_CLASS (gooroom_usbctl_app_window_parent_class)->dispose (object);
}

static void
gooroom_usbctl_app_window_class_init (GooroomUsbctlAppWindowClass *class)
{
  G_OBJECT_CLASS (class)->dispose = gooroom_usbctl_app_window_dispose;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               GOOROOM_USBCTL_UI);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                user_image);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                user_name_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                usbctl_stack_switcher);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_new_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_new_info_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_timeout_info_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_timeout_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_info_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_name_value_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_product_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_vendor_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_serial_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_model_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_confirm_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                reg_cancel_button);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                list_page_box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                approved_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                approved_value_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                approved_view);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                rejected_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                rejected_view);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                waiting_label);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class),
                                                GooroomUsbctlAppWindow,
                                                waiting_view);
}

GooroomUsbctlAppWindow *
gooroom_usbctl_app_window_new (GooroomUsbctlApp *app)
{
  return g_object_new (GOOROOM_USBCTL_APP_WINDOW_TYPE,
                       "application",
                       app,
                       NULL);
}
