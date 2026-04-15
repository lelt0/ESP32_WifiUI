#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include "wifiui_element_message_log.h"
#include "dstring.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_rom_serial_output.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void mirror_log_init(const wifiui_element_msglog_t* mirror_log_element);
static void print_message(const wifiui_element_msglog_t* self, const char* message);
static void init_custom_stdout(void);

const wifiui_element_msglog_t * wifiui_element_message_log(bool mirror_log_mode)
{
    wifiui_element_msglog_t* self = (wifiui_element_msglog_t*)malloc(sizeof(wifiui_element_msglog_t));
    set_default_common(&self->common, WIFIUI_MESSAGE_LOG, create_partial_html);

    self->print_message = print_message;
    if(mirror_log_mode) {
        mirror_log_init(self);
    }

    return self;
}

dstring_t* create_partial_html(const wifiui_element_t* self)
{
    wifiui_element_msglog_t* self_mlog = (wifiui_element_msglog_t*)self;
    dstring_t* html = dstring_create(512);
    dstring_appendf(html, 
        "<textarea id='%s' class='message_log' readonly></textarea>"
        "<script>"
        "{"
            "const terminal = document.getElementById('%s');"
            "ws_actions[%d]=function(array){"
                "const isScrolledToBottom = terminal.scrollHeight - terminal.clientHeight <= terminal.scrollTop + 1;"
                "terminal.value += cstr2str(array);"
                "if (isScrolledToBottom) { terminal.scrollTop = terminal.scrollHeight; }"
            "};"
        "}"
        "</script>",
        self_mlog->common.id_str, self_mlog->common.id_str, self_mlog->common.id
    );
    return html;
}
static StreamBufferHandle_t ws_stream;
static const wifiui_element_msglog_t* s_mirror_log_element = NULL;
static void ws_tee_task(void *arg)
{
    char* buf = malloc(128);
    while (true) {
        size_t len = xStreamBufferReceive(ws_stream, buf, sizeof(buf), portMAX_DELAY);
        if (len > 0 && s_mirror_log_element) {
            wifiui_element_send_data(&s_mirror_log_element->common, buf, len);
        }
    }
    free(buf);
}
void mirror_log_init(const wifiui_element_msglog_t* mirror_log_element)
{
    if(s_mirror_log_element != NULL)
    {
        ESP_LOGW("WIFIUI_MSG_LOG_ELEMENT", "mirror_log already exists. the wifiui system can only have one mirror_log.");
        return;
    }

    s_mirror_log_element = mirror_log_element;
    ws_stream = xStreamBufferCreate(512, 1);
    xTaskCreatePinnedToCore(ws_tee_task, "ws_tee", 4096, NULL, 3, NULL, 0);
    init_custom_stdout();
}

void print_message(const wifiui_element_msglog_t* self, const char* message)
{
    wifiui_element_send_data(&self->common, message, strlen(message) + 1);
}

static ssize_t my_write(int fd, const void *data, size_t size)
{
    const char * const start = (const char *)data;
    const char * const end = (const char *)(data + size);

    if (ws_stream) xStreamBufferSend(ws_stream, start, size, 0);

    // 元UARTへROM経由で出力
    const char* o = start;
    while(o < end) esp_rom_output_tx_one_char(*(o++));
    return size;
}

static int my_open(const char *path, int flags, int mode)
{
    return 0;
}

static int my_close(int fd)
{
    return 0;
}
static const esp_vfs_fs_ops_t my_vfs_ops = {
    .write = my_write,
    .open  = my_open,
    .close = my_close,
};
void init_custom_stdout(void)
{
    // シリアル出力を盗んでWebSocketでも送信するようにする
    esp_vfs_register_fs("/dev/mylog", &my_vfs_ops, ESP_VFS_FLAG_STATIC, NULL);
    freopen("/dev/mylog", "w", stdout);
    freopen("/dev/mylog", "w", stderr);
}