#include <string.h>
#include <stdio.h>
#include "wifiui_element_scatterplot.h"
#include "dstring.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void add_plot(const wifiui_element_scatterplot_t* self, const char* series_name, const uint32_t point_count, const float* x, const float* y, bool draw_line);

const wifiui_element_scatterplot_t * wifiui_element_scatterplot(
    const char* x_label, float x_min, float x_max, 
    const char* y_label, float y_min, float y_max)
{
    wifiui_element_scatterplot_t* self = (wifiui_element_scatterplot_t*)malloc(sizeof(wifiui_element_scatterplot_t));
    set_default_common(&self->common, WIFIUI_SCATTERPLOT, create_partial_html);
    self->common.system.use_websocket = true;
    self->common.system.use_ploty = true;

    self->x_label = strdup(x_label);
    self->x_auto_scale = (x_min >= x_max);
    self->x_min = x_min;
    self->x_max = x_max;
    self->y_label = strdup(y_label);
    self->y_auto_scale = (y_min >= y_max);
    self->y_min = y_min;
    self->y_max = y_max;
    self->add_plot = add_plot;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_scatterplot_t* self_plot = (wifiui_element_scatterplot_t*)self;
    dstring_t* html = dstring_create(1024);
    dstring_appendf(html, 
        "<div class='container'><canvas id='%s_plot'></canvas></div>"
        "<script>"
        "{"
            "const plot_id = '%s_plot';"
            "const COLORS = ['red', 'blue', 'green', 'orange', 'magenta', 'cyan', 'yellow'];"
            "const canvas = document.getElementById(plot_id);"
            "const plot = new Plot2D(canvas, 1.0, [%f, %f], [%f, %f], '%s', '%s');"
            "plot.getXAxis().drawName(true);"
            "plot.getYAxis().drawName(true);"

            "function addPlot(series_name, x, y, draw_line) {"
                "const series_names = plot.getSeriesNameList();"
                "let idx = series_names.indexOf(series_name);"
                "if (idx === -1) {"
                    // 新規系列
                    "idx = series_names.length;"
                    "plot.addSeries(series_name, COLORS[idx%%COLORS.length], x, y, true, draw_line);"
                "} else {"
                    // 既存系列を更新
                    "const s = plot.getSeries(series_name);"
                    "s.setData(x, y);"
                    "s.drawLines(draw_line);"
                "}"
                "plot.render();"
            "}"
            "function parseMessage(buffer) {"
                "const u8 = new Uint8Array(buffer);"
                "const totalBytes = buffer.byteLength;"
                "let offset = 0;"
                // ヘルパ: offset から次の NULL (0) のインデックスを探す
                "function findNull(from) {"
                    "const idx = u8.indexOf(0, from);"
                    "if (idx === -1) throw new Error('NULL terminator not found for C-string');"
                    "return idx;"
                "}"
                // --- name ---
                "const nameNull = findNull(offset);"
                "const name = cstr2str(buffer.slice(offset, nameNull + 1));"
                "offset = nameNull + 1;"
                // --- draw_line (1 byte) ---
                "if (offset >= totalBytes) throw new Error('Unexpected end of buffer before draw_line');"
                "const draw_line = (u8[offset] !== 0);"
                "offset += 1;"
                "offset = (Math.floor(offset / 4) + 1) * 4;"
                // --- float 配列 (残り) ---
                "const remain = totalBytes - offset;"
                "if (remain === 0) {"
                    "return { name, draw_line, x: new Float32Array(0), y: new Float32Array(0) };"
                "}"
                "if (remain %% 4 !== 0) throw new Error('Remaining bytes are not multiple of 4 (float32 alignment)');"
                "const totalFloats = remain / 4;"
                "if (totalFloats %% 2 !== 0) throw new Error('Expect even number of floats to split into x and y');"

                "const half = totalFloats / 2;"
                // 最速パス（もしオフセットが4バイト境界なら TypedArray で一気読み）
                "if (offset %% 4 === 0) {"
                    "const floats = new Float32Array(buffer, offset, totalFloats);"
                    "const x = floats.slice(0, half);"
                    "const y = floats.slice(half);"
                    "return { name, draw_line, x, y };"
                "}"
                // 汎用パス（DataView を使って任意オフセットから読み出す）
                "const dv = new DataView(buffer, offset);"
                "const x = new Float32Array(half);"
                "const y = new Float32Array(half);"
                "for (let i = 0; i < half; i++) {"
                    "x[i] = dv.getFloat32(i * 4, true);"
                    "y[i] = dv.getFloat32((half + i) * 4, true);"
                "}"
                "return { name, draw_line, x, y };"
            "}"

            "ws_actions[%d]=function(array)"
            "{"
                "const { name, draw_line, x, y } = parseMessage(array);"
                "addPlot(name, x, y, draw_line);"
            "};"
        "}"
        "</script>"
        , self_plot->common.id_str, self_plot->common.id_str
        , self_plot->x_min, self_plot->x_max, self_plot->y_min, self_plot->y_max
        , self_plot->x_label, self_plot->y_label
        , self_plot->common.id
    );
    return html;
}

void add_plot(const wifiui_element_scatterplot_t* self, const char* series_name, const uint32_t point_count, const float* x, const float* y, bool draw_line)
{
    size_t series_name_size = strlen(series_name) + 1;
    size_t x_offset = (((series_name_size + 1) / 4) + 1) * 4; // series_name + draw_lineで4byteアライメント
    size_t half_length = point_count * sizeof(float); // x,yそれぞれ分の片方の意
    size_t buf_size = x_offset + half_length * 2;
    char* buf = (char*)malloc(buf_size);

    strncpy(buf, series_name, series_name_size);
    *((uint8_t*)(buf + series_name_size)) = (draw_line?1:0);
    memcpy(buf + x_offset, x, half_length);
    memcpy(buf + x_offset + half_length, y, half_length);

    wifiui_element_send_data(&self->common, buf, buf_size);

    free(buf);
}
