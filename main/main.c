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
#include "rot_encoder.h"
#include "lcd_char2x8.h"
#include "switch.h"

static const char *TAG = "sample";

const wifiui_element_dtext_t* dtext_time = NULL;
const wifiui_element_timeplot_t* timeplot_ROT_ENCODER = NULL;
const wifiui_element_slider_t* slider_LED_switch = NULL;
const wifiui_element_slider_t* slider_LED_red = NULL;
const wifiui_element_slider_t* slider_LED_green = NULL;
const wifiui_element_slider_t* slider_LED_blue = NULL;

typedef enum menu_status
{
    MENU_HOME,
    MENU_SSID,
    MENU_STAIP,
    MENU_ONOFF,
    MENU_R,
    MENU_G,
    MENU_B,
    MENU__END
} menu_status_t;
static menu_status_t screen_state = MENU_HOME;
static menu_status_t home_selection_state = MENU_SSID;
static menu_status_t home_head_pos = MENU_SSID;
static uint32_t s_ip_addr = 0;

const char* get_menu_name(const menu_status_t m)
{
    switch (m)
    {
        case MENU_SSID: return "SSID";
        case MENU_STAIP: return "STA IP";
        case MENU_ONOFF: return "ON/OFF";
        case MENU_R: return "Red";
        case MENU_G: return "Green";
        case MENU_B: return "Blue";
        default: break;
    }
    return "";
}
void update_lcd()
{
    switch (screen_state)
    {
    case MENU_HOME:
        {
            char line[9];
            menu_status_t line_menu = home_head_pos;
            snprintf(line, sizeof(line), "%c %s", (home_selection_state==line_menu?'>':' '), get_menu_name(line_menu));
            lcd_char2x8_write(0, line);

            line_menu = home_head_pos + 1;
            snprintf(line, sizeof(line), "%c %s", (home_selection_state==line_menu?'>':' '), get_menu_name(line_menu));
            lcd_char2x8_write(1, line);
        }
        break;
    case MENU_SSID:
        {
            char ap_ssid[16];
            wifiui_get_ap_ssid(ap_ssid, sizeof(ap_ssid));
            lcd_char2x8_write(-1, ap_ssid);
        }
        break;
    case MENU_STAIP:
        {
            if(s_ip_addr == 0) {
                lcd_char2x8_write(-1, "No IP");
            } else {
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR((esp_ip4_addr_t*)&s_ip_addr));
                lcd_char2x8_write(-1, ip_str);
            }
        }
        break;
    case MENU_ONOFF:
        {
            char line[9];
            snprintf(line, sizeof(line), "LED %s", (slider_LED_switch->current_value < 0.5f ? "OFF" : "ON"));
            lcd_char2x8_write(-1, line);
        }
        break;
    case MENU_R:
        {
            char line[9];
            snprintf(line, sizeof(line), "R:%3d", (int)slider_LED_red->current_value);
            lcd_char2x8_write(-1, line);
        }
        break;
    case MENU_G:
        {
            char line[9];
            snprintf(line, sizeof(line), "G:%3d", (int)slider_LED_green->current_value);
            lcd_char2x8_write(-1, line);
        }
        break;
    case MENU_B:
        {
            char line[9];
            snprintf(line, sizeof(line), "B:%3d", (int)slider_LED_blue->current_value);
            lcd_char2x8_write(-1, line);
        }
        break;
    default:
        break;
    }
}

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
    }
}

/* LED slider input callback */
static void led_color_changed_callback(float value)
{
    if(slider_LED_switch && slider_LED_red && slider_LED_green && slider_LED_blue)
    {
        uint32_t r = (uint32_t) (slider_LED_switch->current_value * slider_LED_red->current_value);
        uint32_t g = (uint32_t) (slider_LED_switch->current_value * slider_LED_green->current_value);
        uint32_t b = (uint32_t) (slider_LED_switch->current_value * slider_LED_blue->current_value);
        set_LED(r, g, b);

        if(timeplot_ROT_ENCODER)
        {
            timeplot_ROT_ENCODER->update_plots(timeplot_ROT_ENCODER, esp_timer_get_time()/1000, (float[]){r, b, g});
        }
    }
    if(screen_state == MENU_ONOFF || screen_state == MENU_R || screen_state == MENU_G || screen_state == MENU_B)
    {
        update_lcd();
    }
}

/* Text input callback */
const wifiui_element_serialLog_t* msglog = NULL;
void input_callback(char* str, void* param)
{
    ESP_LOGI(TAG, "INPUT: %s", str);

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
    s_ip_addr = ip_addr;
    
    char ip_str[64];
    snprintf(ip_str, sizeof(ip_str), "current IP as STA: " IPSTR, IP2STR((esp_ip4_addr_t*)&ip_addr));
    if(dtext_staip != NULL) dtext_staip->change_text(dtext_staip, ip_str);
}

static void rot_encoder_on_right(void)
{
    if(screen_state == MENU_HOME)
    {
        home_selection_state++;
        if(home_selection_state >= MENU__END) home_selection_state = MENU__END - 1;
        if(home_selection_state >= home_head_pos + 2) home_head_pos++;
        update_lcd();
    }
    else if(screen_state == MENU_ONOFF)
    {
        if(slider_LED_switch)
        {
            float toggled_value = (slider_LED_switch->current_value < 0.5f) ? 1.0f : 0.0f;
            slider_LED_switch->set_value(slider_LED_switch, toggled_value);
            led_color_changed_callback(0);
        }
    }
    else if(screen_state == MENU_R || screen_state == MENU_G || screen_state == MENU_B)
    {
        if(slider_LED_red && slider_LED_green && slider_LED_blue)
        {
            if(screen_state == MENU_R) slider_LED_red->set_value(slider_LED_red, slider_LED_red->current_value + 1);
            if(screen_state == MENU_G) slider_LED_green->set_value(slider_LED_green, slider_LED_green->current_value + 1);
            if(screen_state == MENU_B) slider_LED_blue->set_value(slider_LED_blue, slider_LED_blue->current_value + 1);
            led_color_changed_callback(0);
        }
    }
}
static void rot_encoder_on_left(void)
{
    if(screen_state == MENU_HOME)
    {
        if(home_selection_state > MENU_HOME + 1) home_selection_state--;
        if(home_selection_state <= home_head_pos - 1) home_head_pos--;
        update_lcd();
    }
    else if(screen_state == MENU_ONOFF)
    {
        if(slider_LED_switch)
        {
            float toggled_value = (slider_LED_switch->current_value < 0.5f) ? 1.0f : 0.0f;
            slider_LED_switch->set_value(slider_LED_switch, toggled_value);
            led_color_changed_callback(0);
        }
    }
    else if(screen_state == MENU_R || screen_state == MENU_G || screen_state == MENU_B)
    {
        if(slider_LED_red && slider_LED_green && slider_LED_blue)
        {
            if(screen_state == MENU_R) slider_LED_red->set_value(slider_LED_red, slider_LED_red->current_value - 1);
            if(screen_state == MENU_G) slider_LED_green->set_value(slider_LED_green, slider_LED_green->current_value - 1);
            if(screen_state == MENU_B) slider_LED_blue->set_value(slider_LED_blue, slider_LED_blue->current_value - 1);
            led_color_changed_callback(0);
        }
    }
}
static void rot_encoder_on_click(void)
{
    if(screen_state == MENU_HOME)
    {
        screen_state = home_selection_state;
    }
    else
    {
        screen_state = MENU_HOME;
    }
    update_lcd();
}

static void user_button_click(void)
{
    if(slider_LED_switch)
    {
        float toggled_value = (slider_LED_switch->current_value < 0.5f) ? 1.0f : 0.0f;
        slider_LED_switch->set_value(slider_LED_switch, toggled_value);
        led_color_changed_callback(0);
    }
}

void app_main(void)
{
    esp_log_level_set("dns_redirect_server", ESP_LOG_WARN);

    wifiui_page_t* top_page = wifiui_create_page("ESP32-S3 Expand Board");

    /* Boot time */
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_time = wifiui_element_dynamic_text("Boot time: --")));

    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("LED", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_switch = wifiui_element_switch("ON/OFF", false, NULL, led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_red = wifiui_element_slider("R", 0, 127, 1, 32, "#f00", led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_green = wifiui_element_slider("G", 0, 127, 1, 16, "#0f0", led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (slider_LED_blue = wifiui_element_slider("B", 0, 127, 1, 0, "#00f", led_color_changed_callback)));
    wifiui_add_element(top_page, (const wifiui_element_t*) (timeplot_ROT_ENCODER = wifiui_element_timeplot("LED", 0, 255, 30, (char*[]){"R", "B", "G"}, 3)));

    /* Serial log & Text input sample */
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("Mirror Console", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) (msglog = wifiui_element_serial_log(true)));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_input("Send", input_callback, NULL, NULL, true));

    /* Access Point (AP) connection sample  */
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_heading("AP Connection", 2));
    wifiui_add_element(top_page, (const wifiui_element_t*) wifiui_element_ap_connect_form(internet_connected));
    wifiui_add_element(top_page, (const wifiui_element_t*) (dtext_staip = wifiui_element_dynamic_text("current IP as STA: --")));

    init_LED();
    led_color_changed_callback(0.0); // LEDに初期値を反映

    lcd_char2x8_init();
    lcd_char2x8_write(0, "ESP32");

    rot_encoder_init(rot_encoder_on_right, rot_encoder_on_left, rot_encoder_on_click);

    switch_init(user_button_click);

    /* Start WifiUI */
    wifiui_start(NULL, NULL, top_page);

    // create interval task
    xTaskCreate(status_send_task, "status_send_task", 4096, NULL, 5, NULL);

    update_lcd();
}