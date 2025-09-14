#include <string.h>
#include <stdio.h>
#include "wifiui_element_scatter3d_plot.h"
#include "dstring.h"
#include "math.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void update_plot(const wifiui_element_scatter3dplot_t* self, uint16_t point_count, const float* x, const float* y, const float* z, const uint32_t* rgb);

const wifiui_element_scatter3dplot_t * wifiui_element_scatter3d_plot(
    const char* plot_title, 
    float x_min, float x_max,
    float y_min, float y_max,
    float z_min, float z_max)
{
    wifiui_element_scatter3dplot_t* self = (wifiui_element_scatter3dplot_t*)malloc(sizeof(wifiui_element_scatter3dplot_t));
    set_default_common(&self->common, WIFIUI_TIMEPLOT, create_partial_html);
    self->common.system.use_websocket = true;
    self->common.system.use_plotly = true;

    self->plot_title = strdup(plot_title);
    self->x_min = x_min;
    self->x_max = x_max;
    self->y_min = y_min;
    self->y_max = y_max;
    self->z_min = z_min;
    self->z_max = z_max;
    self->update_plot = update_plot;

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_scatter3dplot_t* self_plot = (wifiui_element_scatter3dplot_t*)self;
    dstring_t* html = dstring_create(1024);
    dstring_appendf(html,  
        "<div id='%s_plot' style='width:100vmin; height:100vmin;'></div>"
        "<script>"
        "{"
            "const plot_id = '%s_plot';"
            "let trace = { x: [], y: [], z: [], mode: 'markers', type: 'scatter3d', marker: { size: 4, color: [] } };"
            "const layout = {"
                "title: {text:'%s'},"
                "scene: {"
                    "xaxis: { range: [%f, %f], autorange: false },"
                    "yaxis: { range: [%f, %f], autorange: false },"
                    "zaxis: { range: [%f, %f], autorange: false },"
                    "camera: { eye: { x:1, y:1, z:1 }}"
                "},"
                "margin: {l:0,r:0,b:0,t:0}"
            "};"
            "Plotly.newPlot(plot_id, [trace], layout);"

            // 視点変更中に描画を更新しないようにするフラグ
            "let isDragging = false;"
            // PC用
            "document.getElementById(plot_id).on('plotly_relayout', function(eventData) {"
                "isDragging = false;"
            "});"
            "document.getElementById(plot_id).on('plotly_relayouting', (eventdata) => {"
                "isDragging = true;"
            "});"
            // スマホ用
            "const glContainer = document.getElementById(plot_id).querySelector('.gl-container');"
            "glContainer.addEventListener('touchstart', () => {"
                "isDragging = true;"
                "console.log('drag start (touch)');"
            "});"
            "glContainer.addEventListener('touchmove', () => {"
                "if (isDragging) console.log('dragging... (touch)');"
            "});"
            "glContainer.addEventListener('touchend', () => {"
                "isDragging = false;"
                "console.log('drag end (touch)');"
            "});"

            "ws_actions[%d]=function(array) {"
                "if (!isDragging) {"
                    "const data = new DataView(array);"
                    "let xData = [];"
                    "let yData = [];"
                    "let zData = [];"
                    "let colorData = [];"
                    "const point_count = Math.floor(array.byteLength / 15);"
                    "for (let i = 0; i < point_count; i++) {"
                        "let offset = i * 15;"
                        "let x = data.getFloat32(offset, true);"
                        "let y = data.getFloat32(offset + 4, true);"
                        "let z = data.getFloat32(offset + 8, true);"
                        "let r = data.getUint8(offset + 12);"
                        "let g = data.getUint8(offset + 13);"
                        "let b = data.getUint8(offset + 14);"
                        "xData.push(x);"
                        "yData.push(y);"
                        "zData.push(z);"
                        "colorData.push(`rgb(${r}, ${g}, ${b})`);"
                    "}"
                    "Plotly.update(plot_id, { x: [xData], y: [yData], z: [zData], marker: { size: 4, color: colorData }});"
                "}"
            "};"
        "}"
        "</script>"
        , self_plot->common.id_str
        , self_plot->common.id_str, self_plot->plot_title
        , self_plot->x_min, self_plot->x_max
        , self_plot->y_min, self_plot->y_max
        , self_plot->z_min, self_plot->z_max
        , self_plot->common.id
    );
    return html;
}

void update_plot(const wifiui_element_scatter3dplot_t* self, uint16_t point_count, const float* x, const float* y, const float* z, const uint32_t* rgb)
{
    size_t point_data_size = sizeof(float)*3 + sizeof(uint8_t)*3;
    size_t buf_size = point_count * point_data_size;
    uint8_t* buf = (uint8_t*)malloc(buf_size);
    for(size_t point_i = 0; point_i < point_count; point_i++)
    {
        size_t offset = point_data_size * point_i;
        *((float*)(buf + offset + 0)) = x[point_i];
        *((float*)(buf + offset + 4)) = y[point_i];
        *((float*)(buf + offset + 8)) = z[point_i];
        *((uint8_t*)(buf + offset + 12)) = 0xFF & (rgb[point_i] >> 16);
        *((uint8_t*)(buf + offset + 13)) = 0xFF & (rgb[point_i] >> 8);
        *((uint8_t*)(buf + offset + 14)) = 0xFF & rgb[point_i];
    }
    wifiui_element_send_data(&self->common, (const char*)buf, buf_size);
    free(buf);
}