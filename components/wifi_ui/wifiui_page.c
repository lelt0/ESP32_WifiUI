#include "wifiui_page.h"

#include <stdio.h>
#include <string.h>

static wifiui_page_t ** pages = NULL;
static uint16_t pages_count = 0;
static void register_page(wifiui_page_t * page);

wifiui_page_t * wifiui_create_page(const char * title)
{
    wifiui_page_t* page = (wifiui_page_t*)malloc(sizeof(wifiui_page_t));

    page->id = pages_count;
    page->title = strdup(title);
    {
        char uri[16];
        snprintf(uri, sizeof(uri), "/page%u", pages_count);
        page->uri = strdup(uri);
    }    
    page->elements = NULL;
    page->element_count = 0;
    page->use_websocket = false;
    page->use_plotly = false;

    register_page(page);

    return page;
}

size_t wifiui_add_element(wifiui_page_t* page, const wifiui_element_t* element)
{
    if(page == NULL) return 0;
    if(element == NULL) return page->element_count;

    if(page->elements == NULL) {
        page->elements = (const wifiui_element_t**)malloc(sizeof(wifiui_element_t*));
    } else {
        page->elements = (const wifiui_element_t**)realloc(page->elements, sizeof(wifiui_element_t*) * (page->element_count + 1));
    }
    page->elements[page->element_count++] = element;

    if(element->system.use_websocket || element->system.on_recv_data != NULL) page->use_websocket = true;

    if(element->system.use_plotly) page->use_plotly = true;

    return page->element_count;
}

size_t wifiui_add_elements(wifiui_page_t* page, const wifiui_element_t* elements[], size_t element_count)
{
    for(size_t ele_i = 0; ele_i < element_count; ele_i++)
    {
        wifiui_add_element(page, elements[ele_i]);
    }
    return page->element_count;
}

wifiui_page_t ** wifiui_get_pages(uint16_t* pages_count_dst)
{
    *pages_count_dst = pages_count;
    return pages;
}

wifiui_element_t * wifiui_find_element(const wifiui_page_t * page, const wifiui_element_id id)
{
    if(page == NULL)
    {
        for(int page_i = 0; page_i < pages_count; page_i++)
        {
            wifiui_page_t* scan_page = pages[page_i];
            for(int ele_i = 0; ele_i < scan_page->element_count; ele_i++)
            {
                wifiui_element_t* element = (wifiui_element_t*)scan_page->elements[ele_i];
                if(element->id == id) return element;
            }
        }
    }
    else
    {
        for(int ele_i = 0; ele_i < page->element_count; ele_i++)
        {
            wifiui_element_t* element = (wifiui_element_t*)page->elements[ele_i];
            if(element->id == id) return element;
        }
    }

    return NULL;
}

void register_page(wifiui_page_t * page)
{
    pages = realloc(pages, sizeof(wifiui_page_t*) * (pages_count + 1));
    pages[pages_count++] = page;
}

const char * html_head_template;
const char * html_websocket_template;
dstring_t* wifiui_generate_page_html(const wifiui_page_t* page)
{
    dstring_t* html = dstring_create(1024);
    
    dstring_appendf(html, html_head_template, page->title);
    if(page->use_websocket) dstring_appendf(html, "%s", html_websocket_template);
    if(page->use_plotly) dstring_appendf(html, "<script src='/plotly.min.js'></script>");
    for(int i = 0; i < page->element_count; i++) {
        wifiui_element_t* element = (wifiui_element_t*)page->elements[i];
        if(element->system.create_partial_html != NULL) {
            dstring_t* partial_html = element->system.create_partial_html(element);
            dstring_appendf(html, "%s", partial_html->str);
            dstring_free(partial_html);
        }
    }
    dstring_appendf(html, "</body></html>");

    return html;
}

const char * html_head_template = R"(
<!DOCTYPE html>
<html>
<head>
<meta charset='utf-8'/>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>%s</title>
<style type='text/css'>
body { font-family: system-ui, sans-serif; margin: 0; padding: 1rem; line-height: 1.6; max-width: 900px; margin-left: auto; margin-right: auto; } 
h1, h2, h3 { line-height: 1.2; }
button { padding: 0.6em 1.2em; font-size: 1rem; border: none; border-radius: 0.5em; background: #0078ff; color: white; cursor: pointer; } button:hover { background: #005fcc; } 
img, video { max-width: 100%%; height: auto; display: block; margin: 1rem 0; } 
.wrap_text { white-space: pre-wrap; }
.multi_input { font-family: inherit; font-size: inherit; line-height: inherit; width: 100%%; box-sizing: border-box; resize: none; overflow: hidden; min-height: 1.6em; padding: 0.5em; white-space: pre-wrap; word-break: break-all; border: 1px solid #ccc; border-radius: 0.5em; margin-bottom: -0.4em; }
.single_input { font-family: inherit; font-size: inherit; line-height: inherit; width: 100%%; box-sizing: border-box; resize: none; overflow: hidden; min-height: 1.6em; padding: 0.5em; white-space: pre-wrap; word-break: break-all; border: 1px solid #ccc; border-radius: 0.5em; }
.combo_input { width: 100%%; padding: 8px; box-sizing: border-box; }
.combo_list { top: 100%%; left: 0; right: 0; border: 1px solid #ccc; border-top: none; overflow-y: auto; display: none; z-index: 1000; }
.combo_item { padding: 6px 8px; cursor: pointer; }
.combo_item:hover { background: #def; }
.message_log {width: 100%%; height: 200px; border: 1px solid #ccc; resize: none; overflow-y: auto; word-break: break-all; overflow-wrap: break-word; }
@media (max-width: 600px) { body { padding: 0.8rem; font-size: 0.95rem; } button { width: 100%%; } } 
@media (min-width: 601px) { body { font-size: 1.05rem; } }
</style>
<script>
function fit_textarea_height(id){ t = document.getElementById(id); t.style.height = 'auto'; t.style.height = t.scrollHeight + 'px'; }
</script>
</head>
<body>
)";

const char * html_websocket_template = R"(
<script>
let ws = new WebSocket('ws://' + location.host + location.pathname + '/ws');
let ws_actions = {};
ws.binaryType = 'arraybuffer';
ws.onmessage = function(evt) {
    var data = new DataView(event.data);
    let eid = data.getUint16(0, true);
    if (eid in ws_actions) { ws_actions[eid](event.data.slice(2)); }
};
function cstr2str(array) {
    const data = new Uint8Array(array);
    const nulIndex = data.indexOf(0);
    const slice = nulIndex >= 0 ? data.subarray(0, nulIndex) : data;
    const text = new TextDecoder("utf-8").decode(slice);
    return text;
}
function str2cstr(str) {
    const encoder = new TextEncoder();
    const bytes = encoder.encode(str);
    const cstr = new Uint8Array(bytes.length + 1);
    cstr.set(bytes, 0);
    cstr[bytes.length] = 0; // NULL終端
    return cstr;
}
function ws_send_with_eid(eid, array) {
    const header = new Uint8Array(2);
    header[0] = eid & 0xff;
    header[1] = (eid >> 8) & 0xff;
    const payload = new Uint8Array(header.length + array.length);
    payload.set(header, 0);
    payload.set(array, header.length);
    ws.send(payload);
}
window.addEventListener("beforeunload", () => {
  if (ws.readyState === WebSocket.OPEN) {
    ws.close();
  }
});
</script>
)";