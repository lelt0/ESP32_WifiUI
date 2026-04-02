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
    const char* y_label, float y_min, float y_max, float time_window_sec,
    char** series_names, uint8_t series_count)
{
    wifiui_element_timeplot_t* self = (wifiui_element_timeplot_t*)malloc(sizeof(wifiui_element_timeplot_t));
    set_default_common(&self->common, WIFIUI_TIMEPLOT, create_partial_html);
    self->common.system.use_websocket = true;
    self->common.system.use_ploty = true;

    self->series_count = series_count;
    self->series_names = malloc(sizeof(char*) * series_count);
    self->series_colors = malloc(sizeof(const char*) * series_count);
    for(uint8_t i = 0; i < series_count; i++) {
        self->series_names[i] = strdup(series_names[i]);
        self->series_colors[i] = COLORS[i % (sizeof(COLORS) / sizeof(COLORS[0]))];
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
    dstring_t* series_names = dstring_create_json_list((const char* const*)self_plot->series_names, self_plot->series_count, 8);
    dstring_t* series_colors = dstring_create_json_list(self_plot->series_colors, self_plot->series_count, 8);
    dstring_t* html = dstring_create(1024);
    dstring_appendf(html, 
        "<div class='container'><canvas id='%s_plot'></canvas></div>"
        "<script>"
        "{"
            "const plot_id = '%s_plot';"
            "const TIME_WINDOW = %f;"
            "const FIXED_YRANGE = [%f, %f];"

            "let time0_sec = NaN;" // ESP時刻が0秒のときのページ時刻
            "const canvas = document.getElementById(plot_id);"
            "const plot = new Plot2D(canvas, 0.8, [-TIME_WINDOW, 0], FIXED_YRANGE, undefined, '%s');"
            "plot.getYAxis().drawName(true);"
            "const xaxis2 = plot.addXAxis('abs_t', [-TIME_WINDOW, 0], undefined, 0);"
            
            "const series_names = %s;"
            "const series_colors = %s;"
            "for(let i = 0; i < %d; i++){"
                "plot.addSeries(series_names[i], series_colors[i], null, null, true, true, xaxis2);"
            "}"

            "function updateValues(time_sec, values){"
                "if(isNaN(time0_sec)){ time0_sec = performance.now() / 1000.0 - time_sec; }"
                "values.forEach((value, i)=>{"
                    "if(!isNaN(value)){ plot.getSeries(series_names[i]).addData(time_sec, value); }"
                "});"
            "}"
            "function updatePlot(){"
                "const now_sec = performance.now() / 1000.0;"
                "if(isNaN(time0_sec)){return;}"
                "xaxis2.setMinMax(null, now_sec - time0_sec);"
                "for(const sname of series_names){ plot.getSeries(sname).delOldX(); }" // 過去のデータを削除
                "plot.render();"
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
        , self_plot->common.id_str, self_plot->time_window_sec, self_plot->y_min, self_plot->y_max, self_plot->y_label
        , series_names->str, series_colors->str, self_plot->series_count
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