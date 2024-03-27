#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <cmath>
#include <fmt/core.h>

#include "values.h"

GST_DEBUG_CATEGORY(appsrc_pipeline_debug);
#define GST_CAT_DEFAULT appsrc_pipeline_debug

typedef struct _App App;

struct _App
{
    GstElement* pipeline;
    GstElement* appsrc;
    GstElement* appsink;
    GstBus* bus;

    gboolean eos = FALSE;

    GMainLoop* loop;
    guint sourceid, count;

    gpointer data;

    GTimer* timer;
    std::chrono::_V2::system_clock::time_point t1;
    gint ms_int;

    GstBufferList* buffer;

};

App s_app;

static gboolean
read_data(App* app)
{
    guint len;
    GstFlowReturn ret;

    GST_DEBUG("feed buffer");
    g_signal_emit_by_name(app->appsrc, "push-buffer-list", app->buffer, &ret);
    
    if (ret != GST_FLOW_OK)
    {
        /* some error, stop sending data */
        GST_DEBUG("some error %d", ret);
        return FALSE;
    }

    g_signal_emit_by_name(app->appsrc, "enough-data", &ret);
    
    if (ret != GST_FLOW_OK)
    {
        /* some error, stop sending data */
        GST_DEBUG("some error %d", ret);
        return FALSE;
    }

    return TRUE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed(GstElement* pipeline, guint size, App* app)
{
    if (app->sourceid == 0 && !app->eos)
    {
        GST_DEBUG("start feeding");
        app->sourceid = g_idle_add((GSourceFunc)read_data, app);
    }
}

/* This callback is called when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void
stop_feed(GstElement* pipeline, App* app)
{
    GstFlowReturn ret;
    if (app->sourceid != 0)
    {
        GST_DEBUG("stop feeding");
        g_source_remove(app->sourceid);
        app->sourceid = 0;

        /* signal eos */
        g_signal_emit_by_name(app->appsrc, "end-of-stream", &ret);
        app->eos = TRUE;
    
        /*if (ret != GST_FLOW_OK)
        {
            // some error, stop sending data
            GST_DEBUG("some error %d", ret);
        }
        */
    }
}

static void
stop(GstElement* appsink, App* app)
{
    auto t2 = std::chrono::high_resolution_clock::now();

    app->ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - app->t1).count();

    GST_DEBUG("eos");
    g_main_loop_quit(app->loop);
}

static void
new_sample(GstElement* appsink, App* app)
{
    GST_DEBUG("new-sample");
    GstSample* sample;
    g_signal_emit_by_name(appsink, "pull-sample", &sample, NULL);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    gst_buffer_extract(buffer, 0, app->data, HEIGHT * WIDTH * 4);

    /*FILE* pFile;
    pFile = fopen(fmt::format("{}", app->count++).c_str(), "wb");
    fwrite(app->data, 1, HEIGHT * WIDTH, pFile);
    fclose(pFile);*/
}

static gboolean
bus_message(GstBus* bus, GstMessage* message, App* app)
{
    GST_DEBUG("got message %s",
        gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_ERROR: {
        GError* err = NULL;
        gchar* dbg_info = NULL;

        gst_message_parse_error(message, &err, &dbg_info);
        g_printerr("ERROR from element %s: %s\n",
            GST_OBJECT_NAME(message->src), err->message);
        g_printerr("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
        g_error_free(err);
        g_free(dbg_info);
        g_main_loop_quit(app->loop);
        break;
    }
    case GST_MESSAGE_EOS:
        g_main_loop_quit(app->loop);
        break;
    default:
        break;
    }
    return TRUE;
}

static void
check_error(GError** error)
{
    if (*error != NULL)
    {
        g_error("Catched error: %s", (*error)->message);
        exit(-1);
    }
}

void setup()
{
    App* app = &s_app;
    GError* error = NULL;
    GstCaps* caps;
    GstVideoInfo info;

    /* create a mainloop to get messages and to handle the idle handler that will
     * feed data to appsrc. */
    app->loop = g_main_loop_new(NULL, TRUE);

    app->pipeline = gst_parse_launch("appsrc name=mysource ! nvvideoconvert flip-method=4 ! appsink name=mysink", &error);
    check_error(&error);
    g_assert(app->pipeline);

    app->bus = gst_pipeline_get_bus(GST_PIPELINE(app->pipeline));
    g_assert(app->bus);

    /* add watch for messages */
    gst_bus_add_watch(app->bus, (GstBusFunc)bus_message, app);

    /* get the appsrc */
    app->appsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "mysource");
    g_assert(app->appsrc);
    g_signal_connect(app->appsrc, "need-data", G_CALLBACK(start_feed), app);
    g_signal_connect(app->appsrc, "enough-data", G_CALLBACK(stop_feed), app);

    /* set the caps on the source */
    caps = gst_caps_new_simple ("video/x-raw",
      "format", G_TYPE_STRING, "RGBA",
      "width", G_TYPE_INT, WIDTH,
      "height", G_TYPE_INT, HEIGHT, NULL);
    GstCapsFeatures* feature = gst_caps_features_new ("memory:NVMM", NULL);
    gst_caps_set_features (caps, 0, feature);
    g_object_set(app->appsrc, "caps", caps, NULL);
    //gst_object_unref(caps);

    app->appsink = gst_bin_get_by_name(GST_BIN(app->pipeline), "mysink");
    g_assert(app->appsink);
    g_signal_connect(app->appsink, "eos", G_CALLBACK(stop), app);
    g_signal_connect(app->appsink, "new-sample", G_CALLBACK(new_sample), app);
    g_object_set(app->appsink, "wait-on-eos", TRUE, "emit-signals", TRUE, "async", FALSE, NULL);

    app->data = g_malloc(WIDTH * HEIGHT * 4);
    app->buffer = gst_buffer_list_new_sized(NUMBER);

    gst_buffer_list_make_writable(app->buffer);
    for (guint i = 0; i < NUMBER; i++)
    {
        GstBuffer* buffer = gst_buffer_new_allocate(NULL, HEIGHT * WIDTH * 4, NULL);

        if (i == 0)
        {
            gst_buffer_memset(buffer, 0, 0xFF, HEIGHT * WIDTH * 2);
        }
        gst_buffer_list_add(app->buffer, buffer);
        //gst_buffer_unref(buffer);
    }
}

void cleanup()
{
    App* app = &s_app;
    
    g_free(app->data);
    /*for (guint i = 0; i < NUMBER; i++)
    {
        GstBuffer* buffer = gst_buffer_list_get(app->buffer, i);
        gst_buffer_unref(buffer);
    }*/
    gst_buffer_list_unref(app->buffer);
    gst_object_unref(app->bus);
    g_main_loop_unref(app->loop);
    gst_object_unref(GST_OBJECT(app->appsrc));
    gst_object_unref(GST_OBJECT(app->appsink));
    gst_object_unref(GST_OBJECT(app->pipeline));
}

gint test()
{
    App* app = &s_app;
    
    app->t1 = std::chrono::high_resolution_clock::now();

    app->eos = FALSE;

    /* go to playing and wait in a mainloop. */
    gst_element_set_state(app->pipeline, GST_STATE_PLAYING);

    /* this mainloop is stopped when we receive an error or EOS */
    g_main_loop_run(app->loop);

    GST_DEBUG("stopping");

    gst_element_set_state(app->pipeline, GST_STATE_NULL);
    gst_element_set_state(app->pipeline, GST_STATE_READY);

    return app->ms_int;
}

int
main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    GST_DEBUG_CATEGORY_INIT(appsrc_pipeline_debug, "appsrc-pipeline", 0,
        "appsrc pipeline example");
    
    setup();

    gint count = 10;
    glong sum = 0;

    gint* values = new gint[count];
    for (gint i = 0; i < count; i++)
    {
        values[i] = test();
        sum += values[i];
    }

    gdouble mean = (gdouble)sum / count;

    gdouble sum2 = 0;
    for (gint i = 0; i < count; i++)
    {
        sum2 += std::pow(values[i]- mean, 2);
    }

    gdouble stdDev = std::sqrt(sum2 / (count - 1));
    
    std::cout << "Mean: " << mean << " ms" <<std::endl;
    std::cout << "Standard Deviation: " << stdDev << " ms" << std::endl;

    cleanup();

    return 0;
}