#include <string.h>
#include <stdio.h>
#include "wifiui_element_plot.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void update_plot(const wifiui_element_plot_t* self, const char* series_name, float value);
static void update_plots(const wifiui_element_plot_t* self, const float* values);
static const char* COLORS[] = {"red", "blue", "green", "orange", "magenta", "cyan", "yellow"};

const wifiui_element_plot_t * wifiui_element_plot(
    const char* plot_title, uint8_t series_count, char** series_names, 
    const char* y_label, float y_min, float y_max, 
    float time_window_sec)
{
    wifiui_element_plot_t* self = (wifiui_element_plot_t*)malloc(sizeof(wifiui_element_plot_t));
    set_default_common(&self->common, WIFIUI_PLOT, create_partial_html);
    self->common.system.use_plotly = true;

    self->plot_title = strdup(plot_title);
    self->series_count = series_count;
    self->series_names = (char**)malloc(sizeof(char*) * series_count);
    self->series_colors = (char**)malloc(sizeof(char*) * series_count);
    for(uint8_t i = 0; i < series_count; i++) {
        self->series_names[i] = strdup(series_names[i]);
        self->series_colors[i] = COLORS[i % sizeof(COLORS)];
        printf("[yatadebug][%d] %s %s\n", i, self->series_names[i], self->series_colors[i]);
    }
    self->y_label = strdup(y_label);
    self->y_auto_scale = (y_min >= y_max);
    self->y_min = y_min;
    self->y_max = y_max;
    self->time_window_sec = time_window_sec;
    self->update_plot = update_plot;
    self->update_plots = update_plots;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_plot_t* self_plot = (wifiui_element_plot_t*)self;
    dstring_t* html = dstring_create(1024);
    dstring_appendf(html, 
        "<div id='%s_plot' style='width:100%%;height:500px;'></div>"
        "<script>"
        "{"
            "const plot_id = '%s_plot';"
            "const TIME_WINDOW = %f;"
            "const FIXED_YRANGE = [%f, %f];"

            "const layout = {"
                "title: {text:'%s'},"
                "xaxis: {"
                    "title: {text:'Time [s]'},"
                    "showgrid: true,"
                    "range: [-TIME_WINDOW, 0]"
                "},"
                "xaxis2: {"
                    "overlaying: 'x',"
                    "side: 'top',"
                    "showgrid: false,"
                    "range: [-TIME_WINDOW, 0]"
                "},"
                "yaxis: {"
                    "title: {text:'%s'},"
                    "autorange: true"
                "},"
                "legend: { orientation: 'h' }"
            "};"
            "let dummy = {"
                "x: [-TIME_WINDOW, 0],"
                "y: [null, null],"
                "xaxis: 'x',"
                "yaxis: 'y',"
                "showlegend: false,"
                "hoverinfo: 'skip'"
            "};"
            "Plotly.newPlot(plot_id, [dummy], layout);"
            
            "let traces = [];"
            "const series_names = %s;"
            "const series_colors = %s;"
            "for(let series_i = 0; series_i < %d; series_i++){"
                "traces.push({x:[], y:[]});"
                "Plotly.addTraces(plot_id, {"
                    "x: [],"
                    "y: [],"
                    "mode: 'lines',"
                    "xaxis: 'x2',"
                    "name: series_names[series_i],"
                    "line: {color: series_colors[series_i]}"
                "});"
            "}"

            "function updateVelues(now_sec, values)"
            "{"
                "values.forEach((value, value_i)=>{"
                    "traces[value_i].x.push(now_sec);"
                    "traces[value_i].y.push(value);"
                "});"
            "}"
            "function updatePlot(now_sec)"
            "{"
                "traces.forEach((trace, trace_i)=>{"
                    "while (trace.x.length > 1 && trace.x[1] - now_sec < -TIME_WINDOW) {"
                        "trace.x.shift();"
                        "trace.y.shift();"
                    "}"
                "});"
                "traces.forEach((trace, trace_i)=>{"
                    "Plotly.restyle(plot_id, {"
                        "x: [trace.x],"
                        "y: [trace.y]"
                    "}, 1/*dummy*/ + trace_i);"
                "});"
                "Plotly.relayout(plot_id, {"
                    "'xaxis2.range': [now_sec-TIME_WINDOW, now_sec]"
                "});"

                "const autoscale = true;" // TODO
                "if (autoscale) {"
                    "Plotly.relayout(plot_id, {'yaxis.autorange': true});"
                "} else {"
                    "Plotly.relayout(plot_id, {'yaxis.range': FIXED_YRANGE});"
                "}"
            "}"
            "setInterval(() => { updatePlot(performance.now() / 1000.0);}, 100);"

            "/* === TODO: デモ用: 疑似データ受信 === */"
            "setInterval(() => {"
                "const nowSec = performance.now() / 1000.0;"
                "const val1 = Math.sin(nowSec) + Math.random()*0.2;"
                "const val2 = Math.cos(nowSec*0.5) + Math.random()*0.2;"
                "updateVelues(nowSec, [val1, val2]);"
                "updatePlot(nowSec);"
            "}, 200);"
        "}"
        "</script>"
        , self_plot->common.id_str
        , self_plot->common.id_str, self_plot->time_window_sec, self_plot->y_min, self_plot->y_max
        , self_plot->plot_title, self_plot->y_label
        , "['signalA', 'signalB']", "['red', 'blue']", 2 // TODO
    );
    printf("[yatadebug] self_plot->plot_title: %s\n", self_plot->plot_title);
    printf("[yatadebug] self_plot->y_label: %s\n", self_plot->y_label);
    return html;
}

void update_plot(const wifiui_element_plot_t* self, const char* series_name, float value)
{
    uint8_t series_i = 0;
    for(series_i = 0; series_i < self->series_count; series_i++)
    {
        if(strcmp(series_name, self->series_names[series_i]) == 0) break;
    }
    if(series_i == self->series_count) return;

    size_t buf_size = sizeof(char) + sizeof(uint8_t) + sizeof(float);
    char* buf = (char*)malloc(buf_size);
    char* buf_org = buf;
    *((char*)(buf++)) = '#';
    *((uint8_t*)(buf++)) = (uint8_t)series_i;
    *((float*)(buf)) = (float)value;
    wifiui_element_send_data(&self->common, buf, buf_size);
    free(buf_org);
}

void update_plots(const wifiui_element_plot_t* self, const float* values)
{
    wifiui_element_send_data(&self->common, (const char*)values, sizeof(float) * self->series_count);
}