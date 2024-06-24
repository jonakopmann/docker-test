#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <chrono>
#include <cmath>

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
    //ret = gst_app_src_push_buffer_list(GST_APP_SRC(app->appsrc), app->buffer);
    
    if (ret != GST_FLOW_OK)
    {
        /* some error, stop sending data */
        GST_DEBUG("failed to push buffers %d", ret);
        return FALSE;
    }

    /* signal eos */
    ret = gst_app_src_end_of_stream(GST_APP_SRC(app->appsrc));
    app->eos = TRUE;

    if (ret != GST_FLOW_OK)
    {
        // some error, stop sending data
        GST_DEBUG("failed to set eos %d", ret);
    }

    return FALSE;
}

/* This signal callback is called when appsrc needs data, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
static void
start_feed(GstElement* pipeline, guint size, App* app)
{
    if (!app->eos)
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
    GST_DEBUG("new sample");

    GstSample* sample;
    //sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    g_signal_emit_by_name(appsink, "pull-sample", &sample, NULL);

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    //gst_buffer_extract(buffer, 0, app->data, HEIGHT * WIDTH * 4);

    gst_sample_unref(sample);

    return;

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

    app->pipeline = gst_parse_launch("appsrc name=mysource ! videoflip method=horizontal-flip ! appsink name=mysink", &error);
    check_error(&error);
    g_assert(app->pipeline);

    /* get the appsrc */
    app->appsrc = gst_bin_get_by_name(GST_BIN(app->pipeline), "mysource");
    g_assert(app->appsrc);

    /* set the caps on the source */
    gst_video_info_set_format(&info, GST_VIDEO_FORMAT_RGBA, WIDTH, HEIGHT);
    caps = gst_video_info_to_caps(&info);
    g_object_set(app->appsrc,
                "caps", caps,
                "format", GST_FORMAT_TIME,
                "max-buffers", NUMBER,
                "max-bytes", NUMBER * WIDTH * HEIGHT * 4, NULL);

    app->appsink = gst_bin_get_by_name(GST_BIN(app->pipeline), "mysink");
    g_assert(app->appsink);
    g_object_set(app->appsink,
                "max-buffers", NUMBER,
                "async", false, NULL);

    app->data = g_malloc(WIDTH * HEIGHT * 4);
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
    //gst_object_unref(app->bus);
    //g_main_loop_unref(app->loop);
    gst_object_unref(GST_OBJECT(app->appsrc));
    gst_object_unref(GST_OBJECT(app->appsink));
    gst_object_unref(GST_OBJECT(app->pipeline));
}

gint test()
{
    App* app = &s_app;

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
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();

    /* go to playing and wait in a mainloop. */
    gst_element_set_state(app->pipeline, GST_STATE_PLAYING);

    GstFlowReturn ret;
    ret = gst_app_src_push_buffer_list(GST_APP_SRC(app->appsrc), app->buffer);

    if (ret != GST_FLOW_OK)
    {
        GST_DEBUG("failed to push buffers %d", ret);
    }

    for (gint i = 0; i < NUMBER; i++)
    {
        GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(app->appsink));
        g_assert(sample);
        gst_sample_unref(sample);
    }

    auto t2 = std::chrono::high_resolution_clock::now();

    GST_DEBUG("stopping");

    gst_element_set_state(app->pipeline, GST_STATE_NULL);
    gst_element_set_state(app->pipeline, GST_STATE_READY);

    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}

int
main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    GST_DEBUG_CATEGORY_INIT(appsrc_pipeline_debug, "appsrc-pipeline", 0,
        "appsrc pipeline example");
    
    setup();

    gint count = 3600;
    glong sum = 0;

    gint* values = new gint[count];
    gint max = 0;
    for (gint i = 0; i < count; i++)
    {
        values[i] = test();
        sum += values[i];
        max = std::max(max, values[i]);
    }

    gdouble mean = (gdouble)sum / count;

    gdouble sum2 = 0;
    for (gint i = 0; i < count; i++)
    {
        sum2 += std::pow(values[i]- mean, 2);
    }

    gdouble stdDev = std::sqrt(sum2 / (count - 1));

    // TODO: max value
    std::cout << "Mean: " << mean << " ms" <<std::endl;
    std::cout << "Standard Deviation: " << stdDev << " ms" << std::endl;
    std::cout << "Max: " << max << " ms" <<std::endl;

    cleanup();

    return 0;
}