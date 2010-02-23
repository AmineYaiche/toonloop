#include <gtk/gtk.h>
#include <GL/glx.h>
#include <gst/gst.h>
#include "pipeline.h"
#include "draw.h"
#include <gdk/gdk.h>
#include "gstgtk.h"
#include <iostream>

/**
 * GST bus signal watch callback 
 */
void Pipeline::end_stream_cb(GstBus* bus, GstMessage * msg, gpointer data)
{
    switch (GST_MESSAGE_TYPE(msg)) 
    {
        case GST_MESSAGE_EOS:
            g_print("End-of-stream\n");
            g_print("For more information, try to run it with GST_DEBUG=gldisplay:2\n");
            break;
        case GST_MESSAGE_ERROR:
        {
            gchar *debug = NULL;
            GError *err = NULL;
            gst_message_parse_error(msg, &err, &debug);
            g_print("Error: %s\n", err->message);
            g_error_free (err);
            if (debug) 
            {
                g_print ("Debug deails: %s\n", debug);
                g_free (debug);
            }
            break;
        }
        default:
          break;
    }
    gtk_main_quit();
}
void Pipeline::stop()
{
    gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_NULL);
    gst_object_unref(pipeline_);
    // gtk main quit?
}
Pipeline::Pipeline()
{
    pipeline_ = NULL;
    
    pipeline_ = GST_PIPELINE(gst_pipeline_new("pipeline"));
    //state_ = 0;
    GstElement* glupload;
    
    videosrc_  = gst_element_factory_make("videotestsrc", "videotestsrc0");
    glupload  = gst_element_factory_make ("glupload", "glupload0");
    videosink_ = gst_element_factory_make("glimagesink", "glimagesink0");

    if (!videosrc_ || !glupload || !videosink_)
    {
        g_print("one element could not be found \n");
        exit(1);
    }
    //// change video source caps
    //GstCaps *caps = gst_caps_new_simple("video/x-raw-yuv",
    //                                    "width", G_TYPE_INT, 640,
    //                                    "height", G_TYPE_INT, 480,
    //                                    "framerate", GST_TYPE_FRACTION, 25, 1,
    //                                    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'),
    //                                    NULL) ;
    
    /* change video source caps */
    GstCaps *caps = gst_caps_new_simple("video/x-raw-rgb",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL) ;


    GstCaps *outcaps = gst_caps_new_simple("video/x-raw-gl",
                                        "width", G_TYPE_INT, 800,
                                        "height", G_TYPE_INT, 600,
                                        NULL) ;
    
    // configure elements
    g_object_set(G_OBJECT(videosrc_), "num-buffers", 400, NULL);
    g_object_set(G_OBJECT(videosink_), "client-reshape-callback", reshapeCallback, NULL);
    g_object_set(G_OBJECT(videosink_), "client-draw-callback", drawCallback, NULL);
    //g_object_set(G_OBJECT(videosink_), "client-data", NULL, NULL);
    // add elements
    gst_bin_add_many(GST_BIN(pipeline_), glupload, videosrc_, videosink_, NULL);
    gboolean link_ok = gst_element_link_filtered(videosrc_, glupload, caps);
    gst_caps_unref(caps);
    if(!link_ok)
    {
        g_warning("Failed to link videosrc to glupload!\n") ;
        exit(1);
    }
    link_ok = gst_element_link_filtered(glupload, videosink_, outcaps) ;
    gst_caps_unref(outcaps) ;
    if(!link_ok)
    {
        g_warning("Failed to link glupload to glimagesink!\n") ;
        exit(1);
    }
    /* setup bus */
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(bus, "message::error", G_CALLBACK(end_stream_cb), this);
    g_signal_connect(bus, "message::warning", G_CALLBACK(end_stream_cb), this);
    g_signal_connect(bus, "message::eos", G_CALLBACK(end_stream_cb), this);
    gst_object_unref(bus);

    // make it play !!

    /* run */
    GstStateChangeReturn ret;
    ret = gst_element_set_state(GST_ELEMENT(pipeline_), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_print ("Failed to start up pipeline!\n");
        /* check if there is an error message with details on the bus */
        GstMessage* msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
        if (msg)
        {
          GError *err = NULL;
          gst_message_parse_error (msg, &err, NULL);
          g_print ("ERROR: %s\n", err->message);
          g_error_free (err);
          gst_message_unref (msg);
        }
        exit(1);
    }
}

gboolean on_expose_event(GtkWidget* widget, GdkEventExpose* event, GstElement* videosink)
{
    gst_x_overlay_expose(GST_X_OVERLAY(videosink));
    return FALSE;
}

//client reshape callback
void reshapeCallback(GLuint width, GLuint height, gpointer data)
{
    glViewport(0, 0, width, height);
    
    float w = float(width) / float(height);
    float h = 1.0;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-w, w, -h, h, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

gboolean drawCallback(GLuint texture, GLuint width, GLuint height, gpointer data)
{
    static GTimeVal current_time;
    static glong last_sec = current_time.tv_sec;
    static gint nbFrames = 0;

    g_get_current_time(&current_time);
    nbFrames++ ;

    if ((current_time.tv_sec - last_sec) >= 1)
    {
        std::cout << "GRAPHIC FPS = " << nbFrames << std::endl;
        nbFrames = 0;
        last_sec = current_time.tv_sec;
    }

    // now, draw the texture
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f,0.0f,0.0f);

    glBegin(GL_QUADS);
	      // Front Face
	      glTexCoord2f((gfloat)width / 2, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
	      glTexCoord2f(0.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
	      glTexCoord2f(0.0f, (gfloat)height / 2 ); glVertex3f( 1.0f,  1.0f,  1.0f);
	      glTexCoord2f((gfloat)width / 2, (gfloat)height / 2); glVertex3f(-1.0f,  1.0f,  1.0f);
    glEnd();

    // DRAW LINES
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glColor4f(1.0, 1.0, 0.0, 0.6);
    int num = 64;
    float x;
    for (int i = 0; i < num; i++)
    {
        x = (i / float(num)) * 4 - 2;
        draw::draw_line(float(x), -2.0, float(x), 2.0);
        draw::draw_line(-2.0, float(x), 2.0, float(x));
    }
    //return TRUE causes a postRedisplay
    return TRUE;
}

GstBusSyncReply sync_handler(GstBus* bus, GstMessage* message, GtkWidget* widget)
{
    // ignore anything but 'prepare-xwindow-id' element messages
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
    {
        return GST_BUS_PASS;
    }
    if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
    {
        return GST_BUS_PASS;
    }
    
    g_print("setting xwindow id\n");
    gst_x_overlay_set_gtk_window(GST_X_OVERLAY(GST_MESSAGE_SRC(message)), widget);
    gst_message_unref(message);
    return GST_BUS_DROP;
}

void Pipeline::set_drawing_area(GtkWidget* drawing_area)
{
    //needed when being in GST_STATE_READY, GST_STATE_PAUSED
    //or resizing/obscuring the window
    g_signal_connect(drawing_area, "expose-event", G_CALLBACK(on_expose_event), videosink_);
    
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)sync_handler, drawing_area);
    gst_bus_add_signal_watch(bus);
}

Pipeline::~Pipeline() {}

