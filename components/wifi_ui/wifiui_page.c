#include "wifiui_page.h"

#include <stdio.h>
#include <string.h>

static wifiui_page_t ** pages = NULL;
static uint16_t pages_count = 0;
static void register_page(wifiui_page_t * page);

const wifiui_page_t * wifiui_create_page(const char * title, void * elements[], size_t element_count)
{
    bool websocket_required = false;
    void* elements_ptr = (void*)malloc(sizeof(void*) * element_count);
    for(int i = 0; i < element_count; i++) {
        ((void**)elements_ptr)[i] = elements[i];

        wifiui_element_t* element = (wifiui_element_t*)elements[i];
        if(element->system.use_websocket || element->system.on_recv_data != NULL) websocket_required = true;
    }
    
    wifiui_page_t* page = (wifiui_page_t*)malloc(sizeof(wifiui_page_t));
    page->title = strdup(title);
    if(pages_count == 0) {
        page->uri = "/";
    } else {
        char uri[16];
        snprintf(uri, sizeof(uri), "/page%05u/", pages_count);
        page->uri = strdup(uri);
    }    
    page->element_handlers = elements_ptr;
    page->element_count = element_count;
    page->has_websocket = websocket_required;

    register_page(page);

    return page;
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
                wifiui_element_t* element = (wifiui_element_t*)scan_page->element_handlers[ele_i];
                if(element->id == id) return element;
            }
        }
    }
    else
    {
        for(int ele_i = 0; ele_i < page->element_count; ele_i++)
        {
            wifiui_element_t* element = (wifiui_element_t*)page->element_handlers[ele_i];
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
char* wifiui_generate_page_html(const wifiui_page_t* page)
{
    const size_t html_max = 4096;// TODO 
    char* html = (char*)malloc(html_max);

    
    int off = snprintf(html, html_max, html_head_template, page->title);
    if(page->has_websocket)
    {
        off += snprintf(html + off, html_max - off, "%s", html_websocket_template);
    }
    for(int i = 0; i < page->element_count; i++) {
        wifiui_element_t* element = (wifiui_element_t*)page->element_handlers[i];
        if(element->system.create_partial_html != NULL) {
            char* partial_html = element->system.create_partial_html(element);
            off += snprintf(html + off, html_max - off, "%s", partial_html);
            free(partial_html);
        }
    }
    off += snprintf(html + off, html_max - off, "</body></html>");

    char* ret = strdup(html);
    free(html);
    return ret;
}

const char * html_head_template = R"(
<!DOCTYPE html>
<html>
<head>
<title>%s</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style type='text/css'>
p, div, span, li, td, th { white-space: pre-wrap; }
body { font-family: system-ui, sans-serif; margin: 0; padding: 1rem; line-height: 1.6; max-width: 900px; margin-left: auto; margin-right: auto; } 
h1, h2, h3 { line-height: 1.2; } 
button { padding: 0.6em 1.2em; font-size: 1rem; border: none; border-radius: 0.5em; background: #0078ff; color: white; cursor: pointer; } button:hover { background: #005fcc; } 
img, video { max-width: 100%%; height: auto; display: block; margin: 1rem 0; } 
@media (max-width: 600px) { body { padding: 0.8rem; font-size: 0.95rem; } button { width: 100%%; } } 
@media (min-width: 601px) { body { font-size: 1.05rem; } }
</style>
</head>
<body>
)";

const char * html_websocket_template = R"(
<script>
let ws = new WebSocket('ws://' + location.host + '/ws');
let ws_actions = {};
ws.binaryType = 'arraybuffer';
ws.onmessage = function(evt) {
    var data = new DataView(event.data);
    let eid = data.getUint16(0, true);
    if (eid in ws_actions) { ws_actions[eid](new Uint8Array(event.data, 2)) }
};
function cstr2str(array) {
    const nulIndex = array.indexOf(0);
    const slice = nulIndex >= 0 ? array.subarray(0, nulIndex) : array;
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
</script>
)";