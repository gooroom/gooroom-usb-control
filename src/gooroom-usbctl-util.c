#include "gooroom-usbctl-util.h"

int
gooroom_usbctl_get_usb_infos (usb_info **usbs)
{
  FILE *fp = NULL;
  char **usb_field = NULL;
  char **usb_content = NULL;
  int usb_num = 0;
  int norm = TRUE;
  char *usb_contents = NULL;
  int i, j;
  struct stat st;

  fp = fopen (USB_INFO_FILE, "r");
  if (!fp)
  {
    g_print ("Cannot get USB list.\n");
    return USB_INFO_SET_FAILURE;
  }

  fstat (fileno (fp), &st);
  usb_contents = (char *) calloc (1, st.st_size + 1);
  fread (usb_contents, st.st_size, 1, fp);
  usb_contents[st.st_size] = '\0';
  fclose (fp);

  usb_content = g_strsplit (usb_contents, "\n", -1);
  for (i=0; i<g_strv_length (usb_content); i++)
  {
    norm = TRUE;
    usbs[usb_num] = (usb_info *) calloc (1, sizeof (usb_info));
    usb_field = g_strsplit (usb_content[i], ",", -1);
    for (j=0; j<USB_INFO_NUM; j++)
    {
      if (!usb_field[j])
      {
        norm = FALSE;
        break;
      }
      usbs[usb_num]->value[j] = g_strdup (usb_field[j]);
    }
    if (norm)
      usb_num++;
    else
      free (usbs[usb_num]);
    g_strfreev (usb_field);
  }
  g_strfreev (usb_content);

  if (usb_contents)
    free (usb_contents);

  return usb_num;
}

int
gooroom_usbctl_get_usb_policy_info (usb_policy_info *policy)
{
  FILE *fp = NULL;
  char buf[BUFSIZ] = { 0, };
  int i;
  char *policy_contents = NULL;
  struct stat st;

  fp = fopen (USB_POLICY_INFO_FILE, "r");
  if (!fp)
  {
    g_print ("Cannot get USB policy.\n");
    return USB_POLICY_INFO_SET_FAILURE;
  }

  fstat (fileno (fp), &st);
  policy_contents = (char *) calloc (1, st.st_size + 1);
  fread (policy_contents, st.st_size, 1, fp);
  policy_contents[st.st_size] = '\0';
  fclose (fp);

  for (i=0; policy_contents[i]!='\n' && policy_contents[i]!=' '; i++)
    buf[i] = policy_contents[i];
  buf[i] = '\0';
  policy->value[USB_POLICY_INFO_USB_LIMIT] = g_strdup (buf);
  if (policy_contents)
    free (policy_contents);

  return USB_POLICY_INFO_SET_SUCCESS;
}
