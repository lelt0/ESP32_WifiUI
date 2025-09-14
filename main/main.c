#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "math.h"

#include "wifiui_server.h"
#include "wifiui_element_heading.h"
#include "wifiui_element_stext.h"
#include "wifiui_element_button.h"
#include "wifiui_element_dtext.h"
#include "wifiui_element_link.h"
#include "wifiui_element_input.h"
#include "wifiui_element_ap_connect_form.h"
#include "wifiui_element_message_log.h"
#include "wifiui_element_timeplot.h"
#include "wifiui_element_scatter3d_plot.h"
#include "wifiui_element_scatterplot.h"

static const char *TAG = "sample";

const wifiui_element_dtext_t* dtext_time = NULL;
const wifiui_element_timeplot_t* timeplot = NULL;
const wifiui_element_scatterplot_t* scatterplot = NULL;
const wifiui_element_scatter3dplot_t* scatter3dplot = NULL;
void status_send_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        double time = esp_timer_get_time() / 1000000.0;

        if(dtext_time != NULL)
        {
            char update_text[64];
            snprintf(update_text, 32, "Boot time: %6.3lfs", time);
            dtext_time->change_text(dtext_time, update_text);
        }

        if(timeplot != NULL)
        {
            float val1 = sin(time) + 0.2 * ((float)rand()/RAND_MAX);
            float val2 = cos(time*0.5) + 0.2 * ((float)rand()/RAND_MAX);
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
            uint16_t point_count = rand() % 100;
            if(point_count > 0)
            {
                float *x = (float*)malloc(point_count * sizeof(float));
                float *y = (float*)malloc(point_count * sizeof(float));
                float *z = (float*)malloc(point_count * sizeof(float));
                uint32_t *color = (uint32_t*)malloc(point_count * sizeof(uint32_t));
                for(int i = 0; i < point_count; i++)
                {
                    x[i] = ((float)rand()/RAND_MAX);
                    y[i] = ((float)rand()/RAND_MAX);
                    z[i] = ((float)rand()/RAND_MAX);
                    color[i] = (uint32_t)(rand()%UINT32_MAX);
                }
                scatter3dplot->update_plot(scatter3dplot, point_count, x, y, z, color);
                free(x);
                free(y);
                free(z);
                free(color);
            }
        }

        wifiui_print_server_status();
    }
}

#define LED_GPIO 19
static bool led_status = true;
const wifiui_element_dtext_t* dtext_led = NULL;
void toggle_led(const wifiui_element_button_t * dummy, void* arg)
{
    led_status = !led_status;
    gpio_set_level(LED_GPIO, led_status);

    if(dtext_led != NULL) {
        if(led_status){
            dtext_led->change_text(dtext_led, "LED status: <font color='#00C000'><b>ON</b></font>");
        }else{
            dtext_led->change_text(dtext_led, "LED status: <font color='#C00000'><b>OFF</b></font>");
        }
    }
}

const wifiui_element_msglog_t* msglog = NULL;
void input_callback(char* str, void* param)
{
    ESP_LOGI(TAG, "INPUT: %s", str);
    {
        size_t buf_len = 16 + strlen(str);
        char buf[buf_len];
        snprintf(buf, buf_len, "INPUT: %s\n", str);
        if(msglog != NULL) msglog->print_message(msglog, buf);
    }

    if(strcmp(str, "mem") == 0)
    {
        // print free memory
        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack high water mark: %u words", (unsigned int)high_water_mark);
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Current free heap: %u bytes", (unsigned int)free_heap);
        ESP_LOGI(TAG, "Minimum free heap ever: %u bytes", (unsigned int)min_free_heap);
    }
    else if(strcmp(str, "server") == 0)
    {
        // print server status
        wifiui_print_server_status();
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

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("WifiUI Sample", 1));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_static_text("<b>This is WifiUI sample page.</b>\nHello, World!"));
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_time = wifiui_element_dynamic_text("Boot time: --")));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Button Control", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_button("Toggle LED", toggle_led, NULL));
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_led = wifiui_element_dynamic_text("LED status: --")));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Plot pages", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto time-plot sample page", timeplot_page));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto scatter-plot sample page", scatter_page));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_link("goto scatter3D-plot sample page", scatter3d_page));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("AP Connection", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_ap_connect_form(internet_connected));
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_staip = wifiui_element_dynamic_text("current IP as STA: --")));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Mirror Console", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) (msglog = wifiui_element_message_log(true)));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_input("Send", input_callback, NULL, NULL, true));

    
    wifiui_add_element(timeplot_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(timeplot_page, (const wifiui_element_t*) (timeplot = wifiui_element_timeplot("Plot Sample", 3, (char*[]){"signalA", "signalB", "signalC"}, "Value", -2, 2, 30)));


    wifiui_add_element(scatter_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(scatter_page, (const wifiui_element_t*) (scatterplot = wifiui_element_scatterplot("Scatter Sample", "x", 0, 0, "y", 0, 0)));


    wifiui_add_element(scatter3d_page, (const wifiui_element_t*) wifiui_element_link("goto top page", top_page));
    wifiui_add_element(scatter3d_page, (const wifiui_element_t*) (scatter3dplot = wifiui_element_scatter3d_plot("Plot Sample", 0, 1, 0, 1, 0, 1)));

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, led_status);

    wifiui_start("", "", top_page);

    xTaskCreate(status_send_task, "status_send_task", 4096, NULL, 5, NULL);
}