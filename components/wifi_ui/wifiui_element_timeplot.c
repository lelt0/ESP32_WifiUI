#include <string.h>
#include <stdio.h>
#include "wifiui_element_timeplot.h"
#include "dstring.h"
#include "math.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void update_plot(const wifiui_element_timeplot_t* self, const char* series_name, const uint64_t time_ms, float value);
static void update_plots(const wifiui_element_timeplot_t* self, const uint64_t time_ms, const float* values);
static const char* COLORS[] = {"red", "blue", "green", "orange", "magenta", "cyan", "yellow"};

const wifiui_element_timeplot_t * wifiui_element_timeplot(
    const char* plot_title, uint8_t series_count, char** series_names, 
    const char* y_label, float y_min, float y_max, 
    float time_window_sec)
{
    wifiui_element_timeplot_t* self = (wifiui_element_timeplot_t*)malloc(sizeof(wifiui_element_timeplot_t));
    set_default_common(&self->common, WIFIUI_TIMEPLOT, create_partial_html);
    self->common.system.use_websocket = true;
    self->common.system.use_plotly = true;

    self->plot_title = strdup(plot_title);
    self->series_count = series_count;
    self->series_names = (char**)malloc(sizeof(char*) * series_count);
    self->series_colors = (char**)malloc(sizeof(char*) * series_count);
    for(uint8_t i = 0; i < series_count; i++) {
        self->series_names[i] = strdup(series_names[i]);
        self->series_colors[i] = COLORS[i % sizeof(COLORS)];
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
    wifiui_element_timeplot_t* self_plot = (wifiui_element_timeplot_t*)self;
    dstring_t* series_names = dstring_create_json_list(self_plot->series_names, self_plot->series_count, 8);
    dstring_t* series_colors = dstring_create_json_list(self_plot->series_colors, self_plot->series_count, 8);
    dstring_t* html = dstring_create(1024);
    dstring_appendf(html, 
        "<div id='%s_plot' style='width:100%%;height:500px;'></div>"
        "<script>"
        "{"
            "const plot_id = '%s_plot';"
            "const TIME_WINDOW = %f;"
            "const FIXED_YRANGE = [%f, %f];"

            "let time0_sec = NaN;" // ESP時刻が0秒のときのページ時刻
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

            "function updateValues(time_sec, values)"
            "{"
                "if(isNaN(time0_sec)){ time0_sec = performance.now() / 1000.0 - time_sec; }"
                "values.forEach((value, value_i)=>{"
                    "if(!isNaN(value)){"
                        "traces[value_i].x.push(time_sec);"
                        "traces[value_i].y.push(value);"
                    "}"
                "});"
            "}"
            "function updatePlot()"
            "{"
                "const now_sec = performance.now() / 1000.0;"
                "if(isNaN(time0_sec)){return;}"

                "traces.forEach((trace, trace_i)=>{"
                    "while (trace.x.length > 1 && trace.x[1] < (now_sec - time0_sec) - TIME_WINDOW) {"
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
                    "'xaxis2.range': [(now_sec-time0_sec)-TIME_WINDOW, now_sec-time0_sec]"
                "});"

                "const autoscale = %s;"
                "if (autoscale) {"
                    "Plotly.relayout(plot_id, {'yaxis.autorange': true});"
                "} else {"
                    "Plotly.relayout(plot_id, {'yaxis.range': FIXED_YRANGE});"
                "}"
            "}"
            "setInterval(() => { updatePlot(performance.now() / 1000.0);}, 100);"

            "function getUint64(dataview, byteOffset, littleEndian)"
            "{"
                "const left =  dataview.getUint32(byteOffset, true);"
                "const right = dataview.getUint32(byteOffset+4, true);"
                "const combined = littleEndian? left + 2**32*right : 2**32*left + right;"
                "if (!Number.isSafeInteger(combined))"
                    "console.warn(combined, 'exceeds MAX_SAFE_INTEGER. Precision may be lost');"
                "return combined;"
            "}"
            "ws_actions[%d]=function(array){"
                "const data = new DataView(array);"
                "let time = getUint64(data, 0, true) / 1000.0;"
                "const values = [];"
                "for (let i = 0; i < %d; i++) {"
                    "values.push(data.getFloat32(8+i*4, true));"
                "}"
                "updateValues(time, values);"
                "updatePlot();"
            "};"

            // "/* === デモ用: 疑似データ受信 === */"
            // "setInterval(() => {"
            //     "const nowSec = performance.now() / 1000.0;"
            //     "const val1 = Math.sin(nowSec) + Math.random()*0.2;"
            //     "const val2 = Math.cos(nowSec*0.5) + Math.random()*0.2;"
            //     "updateValues(nowSec, [val1, val2]);"
            //     "updatePlot();"
            // "}, 200);"
        "}"
        "</script>"
        , self_plot->common.id_str
        , self_plot->common.id_str, self_plot->time_window_sec, self_plot->y_min, self_plot->y_max
        , self_plot->plot_title, self_plot->y_label
        , series_names->str, series_colors->str, self_plot->series_count
        , (self_plot->y_auto_scale?"true":"false")
        , self_plot->common.id, self_plot->series_count
    );
    dstring_free(series_names);
    dstring_free(series_colors);
    return html;
}

void update_plot(const wifiui_element_timeplot_t* self, const char* series_name, const uint64_t time_ms, float value)
{
    uint8_t series_i = 0;
    for(series_i = 0; series_i < self->series_count; series_i++)
    {
        if(strcmp(series_name, self->series_names[series_i]) == 0) break;
    }
    if(series_i == self->series_count) return;

    size_t values_buf_size = sizeof(float) * self->series_count;
    float* values_buf = (float*)malloc(values_buf_size);
    for(int i = 0; i < self->series_count; i++) values_buf[i] = ((i == series_i)? value : NAN);
    update_plots(self, time_ms, values_buf);
    free(values_buf);
}

void update_plots(const wifiui_element_timeplot_t* self, const uint64_t time_ms, const float* values)
{
    size_t buf_size = sizeof(uint64_t) + self->series_count * sizeof(float);
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    *((uint64_t*)buf) = time_ms;
    memcpy(buf + sizeof(uint64_t), values, self->series_count * sizeof(float));
    wifiui_element_send_data(&self->common, (const char*)buf, buf_size);
    free(buf);
}