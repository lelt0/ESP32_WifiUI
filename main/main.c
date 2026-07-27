#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "math.h"

#include "wifiui_server.h"
#include "wifiui_element_heading.h"
#include "wifiui_element_stext.h"
#include "wifiui_element_button.h"
#include "wifiui_element_dtext.h"
#include "wifiui_element_link.h"
#include "wifiui_element_input.h"
#include "wifiui_element_ap_connect_form.h"
#include "wifiui_element_serial_log.h"
#include "wifiui_element_timeplot.h"
#include "wifiui_element_scatter3d_plot.h"
#include "wifiui_element_scatterplot.h"
#include "wifiui_element_slider.h"

#include "led_ws2812.h"

static const char *TAG = "sample";

const wifiui_element_dtext_t* dtext_time = NULL;
const wifiui_element_timeplot_t* timeplot = NULL;
const wifiui_element_scatterplot_t* scatterplot = NULL;
const wifiui_element_scatter3dplot_t* scatter3dplot = NULL;
const wifiui_element_slider_t* slider_LED_switch = NULL;
const wifiui_element_slider_t* slider_LED_red = NULL;
const wifiui_element_slider_t* slider_LED_green = NULL;
const wifiui_element_slider_t* slider_LED_blue = NULL;

/* Interval task */
void status_send_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        double time = esp_timer_get_time() / 1000000.0;

        if(((int)time %10)==0) wifiui_print_server_status();

        if(dtext_time != NULL)
        {
            char update_text[64];
            snprintf(update_text, 32, "Boot time: %6.3lfs", time);
            dtext_time->change_text(dtext_time, update_text);
        }

        if(timeplot != NULL)
        {
            float val1 = sinf(time) + 0.2 * ((float)rand()/RAND_MAX);
            float val2 = cosf(time*0.5) + 0.2 * ((float)rand()/RAND_MAX);
            uint64_t time_ms = (uint64_t)time*1000;
            timeplot->update_plots(timeplot, time_ms, (float[]){val1, val2, NAN});
            if(((int)time)%2==0) timeplot->update_plot(timeplot, "signalC", time_ms, (float)rand()/RAND_MAX);
        }

        if(scatterplot != NULL)
        {
            size_t point_count = ((size_t)time) % 100;
            if(point_count > 0)
            {
                float* x = (float*)malloc(point_count * sizeof(float));
                float* y = (float*)malloc(point_count * sizeof(float));
                for(int i = 0; i < point_count; i++)
                {
                    x[i] = 0.1 * i * sin(i);
                    y[i] = 0.1 * i * cos(i);
                }
                scatterplot->add_plot(scatterplot, "sample-1", point_count, x, y, true);
                free(x);
                free(y);
            }
        }

        if(scatter3dplot != NULL)
        {
            const int D = 32;
            size_t point_count = D * D;
            float *x = (float*)malloc(point_count * sizeof(float));
            float *y = (float*)malloc(point_count * sizeof(float));
            float *z = (float*)malloc(point_count * sizeof(float));
            uint32_t *color = (uint32_t*)malloc(point_count * sizeof(uint32_t));
            for(int32_t yi = 0; yi < D; yi++)
                for(int32_t xi = 0; xi < D; xi++)
                {
                    int32_t i = yi * D + xi;
                    x[i] = (float) xi / D;
                    y[i] = (float) yi / D;
                    z[i] = 0.25 * sinf(6 * x[i] + time/8.f) * sinf(6 * y[i] + time/8.f);
                    uint8_t r = (xi << 3);
                    uint8_t b = (yi << 3);
                    uint8_t g = (((D - xi - 1) + (D - yi - 1)) << 2);
                    color[i] = RGB(r, g, b);
                }
            scatter3dplot->update_plot(scatter3dplot, point_count, x, y, z, color);
            free(x);
            free(y);
            free(z);
            free(color);
        }
    }
}

/* LED slider input callback */
static void led_color_changed_callback(float value)
{
    if(slider_LED_switch && slider_LED_red && slider_LED_green && slider_LED_blue)
    {
        int sw = (int)slider_LED_switch->current_value;
        uint32_t r = sw * ((uint32_t) slider_LED_red->current_value);
        uint32_t g = sw * ((uint32_t) slider_LED_green->current_value);
        uint32_t b = sw * ((uint32_t) slider_LED_blue->current_value);
        set_LED(r, g, b);
    }
}

/* Text input callback */
const wifiui_element_serialLog_t* msglog = NULL;
void input_callback(char* str, void* param)
{
    ESP_LOGI(TAG, "INPUT: %s", str);
    {
        size_t buf_len = 16 + strlen(str);
        char buf[buf_len];
        snprintf(buf, buf_len, "INPUT: %s\n", str);
        if(msglog != NULL) msglog->print(msglog, buf);
    }

    if(strcmp(str, "chip") == 0)
    {
        esp_chip_info_t chip_info;
        uint32_t flash_size;
        esp_chip_info(&chip_info);
        printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
            (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
            (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

        volatile unsigned major_rev = chip_info.revision / 100;
        volatile unsigned minor_rev = chip_info.revision % 100;
        printf("silicon revision v%d.%d, ", major_rev, minor_rev);

        if(esp_flash_get_size(NULL, &flash_size) == ESP_OK)
        {
            printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
        }
    }
    else if(strcmp(str, "ram") == 0)
    {
        puts("\n=== RAM info ===");
        UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        size_t stack_high_water_mark_bytes = (size_t)stack_high_water_mark * sizeof(StackType_t);
        printf("Stack of task '%s'\n", pcTaskGetName(NULL));
        printf("  high water mark  : %zu bytes\n", stack_high_water_mark_bytes);
        size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t used_heap = total_heap - free_heap;
        size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        size_t max_used_heap = total_heap - min_free_heap;
        float usage_rate = (total_heap > 0) ? ((float)used_heap * 100.0f / (float)total_heap) : 0.0f;
        float max_usage_rate = (total_heap > 0) ? ((float)max_used_heap * 100.0f / (float)total_heap) : 0.0f;
        puts("Heap");
        printf("  Total            : %zu bytes\n", total_heap);
        printf("  Current usage    : %zu bytes (%.2f %%)\n", used_heap, usage_rate);
        printf("  Max usage ever   : %zu bytes (%.2f %%)\n", max_used_heap, max_usage_rate);
        puts("===================\n");
    }
    else if(strcmp(str, "server") == 0)
    {
        // print server status
        wifiui_print_server_status();
    }
    else
    {
        puts("unknown command!");
        puts("- chip: show chip information");
        puts("- ram: show RAM usage");
        puts("- server: show server status");
    }
}

const wifiui_element_dtext_t* dtext_staip = NULL;
void internet_connected(uint32_t ip_addr)
{
    char ip_str[64];
    snprintf(ip_str, sizeof(ip_str), "current IP as STA: " IPSTR, IP2STR((esp_ip4_addr_t*)&ip_addr));
    if(dtext_staip != NULL) dtext_staip->change_text(dtext_staip, ip_str);
}

void app_main(void)
{
    wifiui_page_t* top_page = wifiui_create_page("WifiUI Sample");
    wifiui_page_t* timeplot_page = wifiui_create_page("time-plot sample");
    wifiui_page_t* scatter_page = wifiui_create_page("scatter-plot sample");
    wifiui_page_t* scatter3d_page = wifiui_create_page("scatter3d-plot sample");

    /* Static & Dynamic text sample */
    wifiui_add_html(top_page, "<p>This is WifiUI sample page.\nHello, World!</p>");
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_time = wifiui_element_dynamic_text("Boot time: --")));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Control", 2));
    /* Switch&Slider sample */
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_switch = wifiui_element_switch("LED", false, NULL, led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_red = wifiui_element_slider("R", 0, 127, 1, 32, "#f00", led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_green = wifiui_element_slider("G", 0, 127, 1, 16, "#0f0", led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_blue = wifiui_element_slider("B", 0, 127, 1, 0, "#00f", led_color_changed_callback)));

    /* Serial log & Text input sample */
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Mirror Console", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) (msglog = wifiui_element_serial_log(true)));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_input("Send", input_callback, NULL, NULL, true));

    /* Access Point (AP) connection sample  */
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("AP Connection", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_ap_connect_form(internet_connected));
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_staip = wifiui_element_dynamic_text("current IP as STA: --")));

    /* 2D & 3D Plot links  */
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Plot pages", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto time-plot sample page", timeplot_page));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto scatter-plot sample page", scatter_page));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto scatter3D-plot sample page", scatter3d_page));

    /* Time Plot sample */
    wifiui_add_element(timeplot_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(timeplot_page, (const wifiui_element_t*) (timeplot = wifiui_element_timeplot("Value", -2, 2, 30, (char*[]){"signalA", "signalB", "signalC"}, 3)));

    /* Scatter (X-Y) Plot sample */
    wifiui_add_element(scatter_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(scatter_page, (const wifiui_element_t*) (scatterplot = wifiui_element_scatterplot("x", 0, 0, "y", 0, 0)));

    /* 3D Scatter Plot sample */
    wifiui_add_element(scatter3d_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(scatter3d_page, (const wifiui_element_t*) (scatter3dplot = wifiui_element_scatter3d_plot(0, 1, 0, 1, -0.5, 0.5)));

    init_LED();
    led_color_changed_callback(0.0); // LEDに初期値を反映

    /* Start WifiUI */
    wifiui_start("", "", top_page);

    // create interval task
    xTaskCreate(status_send_task, "status_send_task", 4096, NULL, 5, NULL);
}