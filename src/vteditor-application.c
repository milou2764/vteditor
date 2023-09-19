/* vteditor-application.c
 *
 * Copyright 2023 milou
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "vteditor-application.h"
#include "vteditor-window.h"
#include "VTFLib.h"

struct _VteditorApplication
{
	GtkApplication parent_instance;
};

typedef struct tagVTFHEADER
{
    char            signature[4];       // File signature ("VTF\0"). (or as little-endian integer, 0x00465456)
    unsigned int    version[2];         // version[0].version[1] (currently 7.2).
    unsigned int    headerSize;         // Size of the header struct  (16 byte aligned; currently 80 bytes) + size of the resources dictionary (7.3+).
    unsigned short  width;              // Width of the largest mipmap in pixels. Must be a power of 2.
    unsigned short  height;             // Height of the largest mipmap in pixels. Must be a power of 2.
    unsigned int    flags;              // VTF flags.
    unsigned short  frames;             // Number of frames, if animated (1 for no animation).
    unsigned short  firstFrame;         // First frame in animation (0 based). Can be -1 in environment maps older than 7.5, meaning there are 7 faces, not 6.
    unsigned char   padding0[4];        // reflectivity padding (16 byte alignment).
    float           reflectivity[3];    // reflectivity vector.
    unsigned char   padding1[4];        // reflectivity padding (8 byte packing).
    float           bumpmapScale;       // Bumpmap scale.
    unsigned int    highResImageFormat; // High resolution image format.
    unsigned char   mipmapCount;        // Number of mipmaps.
    unsigned int    lowResImageFormat;  // Low resolution image format (always DXT1).
    unsigned char   lowResImageWidth;   // Low resolution image width.
    unsigned char   lowResImageHeight;  // Low resolution image height.

    // 7.2+
    unsigned short  depth;              // Depth of the largest mipmap in pixels. Must be a power of 2. Is 1 for a 2D texture.

    // 7.3+
    unsigned char   padding2[3];        // depth padding (4 byte alignment).
    unsigned int    numResources;       // Number of resources this vtf has. The max appears to be 32.

    unsigned char   padding3[8];        // Necessary on certain compilers
} VTFHEADER;

G_DEFINE_TYPE (VteditorApplication, vteditor_application, GTK_TYPE_APPLICATION)

VteditorApplication *
vteditor_application_new (const char        *application_id,
                          GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (VTEDITOR_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     NULL);
}

static void
vteditor_application_activate (GApplication *app)
{
	GtkWindow *window;

	g_assert (VTEDITOR_IS_APPLICATION (app));

	window = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (window == NULL)
		window = g_object_new (VTEDITOR_TYPE_WINDOW,
		                       "application", app,
		                       NULL);

	gtk_window_present (window);
}

static void
vteditor_application_class_init (VteditorApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	app_class->activate = vteditor_application_activate;
}

static void
open_dialog_cb (GObject *source_object, GAsyncResult *res, gpointer data) {
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source_object);
  GFile *gfile;
  char *contents;
  gsize length;
  GError *err = NULL;
  GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (data));
  GtkImage* image;
  char* extension;
  GdkPaintable* paintable;
  VTFHEADER vtfHeader;
  void* dst;
  char* filename;
  vlUInt uiVTFImage;									// VTF image handle.
  GdkPixbuf* pixbuf;

  image = GTK_IMAGE (((VteditorWindow*)window)->image);
  if ((gfile = gtk_file_dialog_open_finish(dialog, res, &err)) != NULL
      && g_file_load_contents (gfile, NULL, &contents, &length, NULL, &err)){
        filename = g_file_get_path(gfile);
        extension = strrchr(g_file_get_basename(gfile), '.');
        if (strcmp(extension, ".vtf")==0){
          vtfHeader = *(VTFHEADER*)contents;
          printf("ceci est un vtf\n");
          printf("signature: %s\n", vtfHeader.signature);
          printf("version: %x.%x\n", vtfHeader.version[0], vtfHeader.version[1]);
          printf("headerSize: %i\n", vtfHeader.headerSize);
          printf("width: %d\n", vtfHeader.width);

          vlInitialize();
          vlCreateImage(&uiVTFImage);
          vlBindImage(uiVTFImage);
          vlImageLoad(filename, vlFalse);

          dst = malloc(vlImageComputeImageSize(vlImageGetWidth(), vlImageGetHeight(), 1, 1, IMAGE_FORMAT_RGB888));
          if(!vlImageConvert(vlImageGetData(0, 0, 0, 0), dst, vlImageGetWidth(), vlImageGetHeight(), vlImageGetFormat(), IMAGE_FORMAT_RGB888))
          {
            free(dst);

            printf(" Error converting input file:\n%s\n\n", vlGetLastError());
            return;
          }
          pixbuf = gdk_pixbuf_new_from_data(dst, GDK_COLORSPACE_RGB, false, 8, vtfHeader.width, vtfHeader.height, 3*vtfHeader.width, NULL, NULL);
          gtk_image_set_from_pixbuf(image, pixbuf);

        }else{
          paintable = (GdkPaintable*)gdk_texture_new_from_file (gfile, &err);
          gtk_image_set_from_paintable (image, paintable);
        }
  }
}

static void
vteditor_application_openfile_action (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  VteditorApplication *self = user_data;
  GtkWindow *window = gtk_application_get_active_window (GTK_APPLICATION (self));
  GtkFileDialog *fileDiag = gtk_file_dialog_new();

  gtk_file_dialog_open(fileDiag, window, NULL, open_dialog_cb, user_data);
  g_object_unref (fileDiag);
}

static void
vteditor_application_about_action (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
	static const char *authors[] = {"milou", NULL};
	VteditorApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (VTEDITOR_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	gtk_show_about_dialog (window,
	                       "program-name", "vteditor",
	                       "logo-icon-name", "org.bsoldiers.vte",
	                       "authors", authors,
	                       "version", "0.1.0",
	                       "copyright", "© 2023 milou",
	                       NULL);
}

static void
vteditor_application_quit_action (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
	VteditorApplication *self = user_data;

	g_assert (VTEDITOR_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
	{ "openfile", vteditor_application_openfile_action },
	{ "quit", vteditor_application_quit_action },
	{ "about", vteditor_application_about_action },
};

static void
vteditor_application_init (VteditorApplication *self)
{
	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<primary>q", NULL });
}
