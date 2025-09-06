#include <string.h>
#include <stdio.h>
#include "wifiui_element_message_log.h"
#include "dstring.h"
#include "esp_log.h"

static dstring_t* create_partial_html(const wifiui_element_t* self);
static void mirror_log_init(const wifiui_element_msglog_t* mirror_log_element);
static void print_message(const wifiui_element_t* self, const char* message);

const wifiui_element_msglog_t * wifiui_element_message_log(bool mirror_log_mode)
{
    wifiui_element_msglog_t* self = (wifiui_element_msglog_t*)malloc(sizeof(wifiui_element_msglog_t));
    set_default_common(&self->common, WIFIUI_MESSAGE_LOG, create_partial_html);

    if(mirror_log_mode) {
        self->print_message = NULL;
        mirror_log_init(self);
    }
    else{
        self->print_message = print_message;
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
            "ws_actions[%d]=function(data){"
                "const isScrolledToBottom = terminal.scrollHeight - terminal.clientHeight <= terminal.scrollTop + 1;"
                "terminal.value += cstr2str(data);"
                "if (isScrolledToBottom) { terminal.scrollTop = terminal.scrollHeight; }"
            "};"
        "}"
        "</script>",
        self_mlog->common.id_str, self_mlog->common.id_str, self_mlog->common.id
    );
    return html;
}


static vprintf_like_t s_orig_vprintf = NULL;  // オリジナルのログ関数ポインタ
static bool in_mirror_log = false;              // ログ関数再帰防止フラグ
static int mirror_log_vprintf(const char *fmt, va_list args);
static const wifiui_element_msglog_t* s_mirror_log_element = NULL;
void mirror_log_init(const wifiui_element_msglog_t* mirror_log_element)
{
    s_mirror_log_element = mirror_log_element;
    s_orig_vprintf = esp_log_set_vprintf(mirror_log_vprintf); // シリアル出力を盗んでWebSocketでも送信するようにする
}
static int mirror_log_vprintf(const char *fmt, va_list args)
{
    if (in_mirror_log) {
        return s_orig_vprintf(fmt, args);
    }

    in_mirror_log = true;

    static char buf[256];
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(buf, sizeof(buf), fmt, args_copy);
    va_end(args_copy);
    if (len > 0 && s_mirror_log_element != NULL) {
        wifiui_element_send_data(&s_mirror_log_element->common, buf, strlen(buf) + 1);
    }

    int ret = s_orig_vprintf(fmt, args);

    in_mirror_log = false;
    return ret;
}

void print_message(const wifiui_element_t* self, const char* message)
{
    wifiui_element_send_data(self, message, strlen(message) + 1);
}