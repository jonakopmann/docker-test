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
    UfoResources* res;
    cl_mem buffer;

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

CustomData data;

void init()
{
    /* Initialize cumstom data structure */
    memset(&data, 0, sizeof(data));

    GError* error = NULL;

    data.res = ufo_resources_new(&error);
    if (error != NULL)
    {
        g_error("resources: %s", (error)->message);
        exit(-1);
    }

    cl_context ctx = (cl_context)ufo_resources_get_context(data.res);

    cl_int error2;
    data.buffer = clCreateBuffer(ctx, CL_MEM_HOST_NO_ACCESS, WIDTH * HEIGHT * 4 * NUMBER, NULL, &error2);
    if (error2 != 0)
    {
        g_error("buffer: %d", error2);
        exit(-1);
    }
}

void free()
{
    clReleaseMemObject(data.buffer);
    g_object_unref(data.res);
}

gint test()
{
    GError* error = NULL;

    /* Initialize Ufo */
    data.graph = UFO_TASK_GRAPH(ufo_task_graph_new());
    data.manager = ufo_plugin_manager_new();

    /* Create the tasks */
    data.memory_in = ufo_plugin_manager_get_task(data.manager, "memory-in", &error);
    if (error != NULL)
    {
        g_error("memory-in: %s", (error)->message);
        exit(-1);
    }
    data.flip = ufo_plugin_manager_get_task(data.manager, "flip", &error);
    if (error != NULL)
    {
        g_error("flip: %s", (error)->message);
        exit(-1);
    }
    data.memory_out = ufo_plugin_manager_get_task(data.manager, "memory-out", &error);
    if (error != NULL)
    {
        g_error("memory-out: %s", (error)->message);
        exit(-1);
    }
    
    data.scheduler = ufo_scheduler_new();

    ufo_base_scheduler_set_resources(data.scheduler, data.res);

    /* Configure memory-in */
    g_object_set(G_OBJECT(data.memory_in),
        "pointer", data.buffer,
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

    if (error != NULL)
    {
        g_error("run: %s", (error)->message);
        exit(-1);
    }
    g_free(outBuffer);

    /* Destroy all objects */
    g_object_unref(data.memory_in);
    g_object_unref(data.flip);
    g_object_unref(data.memory_out);
    g_object_unref(data.graph);
    g_object_unref(data.scheduler);
    g_object_unref(data.manager);

    return std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
}

int
main(int argc, char* argv[])
{
    gint count = 3600;
    glong sum = 0;

    init();
    gint* values = new gint[count];
    gint max = 0;
    for (gint i = 0; i < count; i++)
    {
        values[i] = test();
        sum += values[i];
        max = std::max(max, values[i]);
    }
    free();

    gdouble mean = (gdouble)sum / count;

    gdouble sum2 = 0;
    for (gint i = 0; i < count; i++)
    {
        sum2 += std::pow(values[i]- mean, 2);
    }

    gdouble stdDev = std::sqrt(sum2 / (count - 1));
    
    std::cout << "Mean: " << mean << " ms" <<std::endl;
    std::cout << "Standard Deviation: " << stdDev << " ms" << std::endl;
    std::cout << "Max: " << max << " ms" <<std::endl;

    return 0;
}