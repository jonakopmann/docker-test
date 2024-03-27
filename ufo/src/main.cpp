#include <ufo/ufo.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <CL/cl.h>

#include "values.h"

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    UfoTaskGraph* graph;
    UfoPluginManager* manager;
    UfoBaseScheduler* scheduler;

    UfoTaskNode* memory_in;
    UfoTaskNode* flip;
    UfoTaskNode* memory_out;
} CustomData;

static void
check_error(GError** error)
{
    if (*error != NULL)
    {
        g_error("Catched error: %s", (*error)->message);
        exit(-1);
    }
}

gint test()
{
    CustomData data;
    GError* error = NULL;

    /* Initialize cumstom data structure */
    memset(&data, 0, sizeof(data));

    /* Initialize Ufo */
    data.graph = UFO_TASK_GRAPH(ufo_task_graph_new());
    data.manager = ufo_plugin_manager_new();

    /* Create the tasks */
    data.memory_in = ufo_plugin_manager_get_task(data.manager, "memory-in", &error);
    check_error(&error);
    data.flip = ufo_plugin_manager_get_task(data.manager, "flip", &error);
    check_error(&error);
    data.memory_out = ufo_plugin_manager_get_task(data.manager, "memory-out", &error);
    check_error(&error);

    /*gpointer buffer = g_malloc0(WIDTH * HEIGHT * NUMBER * 4);
    for (guint32 i = 0; i < NUMBER; i++)
    {
        memset((gint8*)buffer + i * WIDTH * HEIGHT * 4, 0xFF, WIDTH * HEIGHT * 2);
    }*/
    
    data.scheduler = ufo_scheduler_new();
    //g_object_set(data.scheduler, "enable-tracing", TRUE, NULL);

    auto res = ufo_resources_new(&error);
    check_error(&error);

    cl_context ctx = (cl_context)ufo_resources_get_context(res);

    cl_int error2;
    cl_mem buffer = clCreateBuffer(ctx, CL_MEM_HOST_NO_ACCESS, WIDTH * HEIGHT * 4 * NUMBER, NULL, &error2);

    ufo_base_scheduler_set_resources(data.scheduler, res);

    /* Configure memory-in */
    g_object_set(G_OBJECT(data.memory_in),
        "pointer", buffer,
        "width", WIDTH,
        "height", HEIGHT,
        "number", NUMBER,
        "bitdepth", sizeof(guint32) * 8,
        "memory-location", 1,
        NULL);

    /* Configure flip */
    g_object_set(G_OBJECT(data.flip),
        "direction", 1,
        NULL);

    gpointer outBuffer = g_malloc(WIDTH * HEIGHT * NUMBER * 4);
    /* Configure memory-out */
    g_object_set(G_OBJECT(data.memory_out),
        "pointer", outBuffer,
        "max-size", WIDTH * HEIGHT * NUMBER * 4,
        NULL);

    /* Connect tasks in graph */
    ufo_task_graph_connect_nodes(data.graph, data.memory_in, data.flip);
    ufo_task_graph_connect_nodes(data.graph, data.flip, data.memory_out);

    /* Run graph */

    auto t1 = std::chrono::high_resolution_clock::now();

    ufo_base_scheduler_run(data.scheduler, data.graph, &error);

    auto t2 = std::chrono::high_resolution_clock::now();

    check_error(&error);

    clReleaseMemObject(buffer);
    g_free(outBuffer);

    /* Destroy all objects */
    g_object_unref(data.memory_in);
    g_object_unref(data.flip);
    g_object_unref(data.memory_out);
    g_object_unref(data.graph);
    g_object_unref(res);
    g_object_unref(data.scheduler);
    g_object_unref(data.manager);

    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}

int
main(int argc, char* argv[])
{
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

    return 0;
}