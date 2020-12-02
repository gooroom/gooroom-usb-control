#include "gooroom-usbctl-util.h"

int
gooroom_usbctl_get_usb_infos (usb_info **usbs)
{
  FILE *fp = NULL;
  int usb_num = 0;

  if (g_file_test (USB_INFO_FILE, G_FILE_TEST_IS_REGULAR)
      && (fp = fopen (USB_INFO_FILE, "r")))
  {
    struct stat st;
    fstat (fileno (fp), &st);
    char *usb_contents = (char *) calloc (1, st.st_size + 1);
    fread (usb_contents, st.st_size, 1, fp);
    usb_contents[st.st_size] = '\0';
    fclose (fp);
    fp = NULL;

    char **usb_content = g_strsplit (usb_contents, "\n", -1);
    int norm = TRUE;
    int i;
    for (i=0; i<g_strv_length (usb_content); i++)
    {
      norm = TRUE;
      usbs[usb_num] = (usb_info *) calloc (1, sizeof (usb_info));
      char **usb_field = g_strsplit (usb_content[i], ",", -1);
      int j;
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
  }
  else
  {
    if (fp)
      fclose (fp);
    fp = NULL;
    g_print ("Cannot get USB list.\n");
    return USB_INFO_SET_FAILURE;
  }

  return usb_num;
}

int
gooroom_usbctl_get_usb_policy_info (usb_policy_info *policy)
{
  FILE *fp = NULL;

  if (g_file_test (USB_POLICY_INFO_FILE, G_FILE_TEST_IS_REGULAR)
      && (fp = fopen (USB_POLICY_INFO_FILE, "r")))
  {
    char buf[BUFSIZ] = { 0, };
    char *policy_contents = NULL;
    struct stat st;
    fstat (fileno (fp), &st);
    policy_contents = (char *) calloc (1, st.st_size + 1);
    fread (policy_contents, st.st_size, 1, fp);
    policy_contents[st.st_size] = '\0';
    fclose (fp);
    fp = NULL;

    int i;
    for (i=0; '\n'!=policy_contents[i] && ' '!=policy_contents[i]; i++)
        buf[i] = policy_contents[i];
    buf[i] = '\0';
    policy->value[USB_POLICY_INFO_USB_LIMIT] = g_strdup (buf);
    if (policy_contents)
        free (policy_contents);
  }
  else
  {
    if (fp)
      fclose (fp);
    fp = NULL;
    g_print ("Cannot get USB policy.\n");
    return USB_POLICY_INFO_SET_FAILURE;
  }

  return USB_POLICY_INFO_SET_SUCCESS;
}
