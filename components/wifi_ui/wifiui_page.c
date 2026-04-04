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
    page->use_ploty = false;

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

    if(element->system.use_ploty) page->use_ploty = true;

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
    if(page->use_ploty) dstring_appendf(html, "<script src='/ploty.js'></script>");
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
<meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1.0">
<title>%s</title>
<style>
* { box-sizing: border-box; }

body { background: #FFF; color: #222; font-family: system-ui, -apple-system, sans-serif; line-height: 1.6; 
  width: min(100vh, 100vw); margin: 0 auto; padding: 1em 1em;
}

h1, h2, h3, h4, h5, h6, p { margin: 0 0 0.8em 0; text-align: left; }

button { display: block;
  margin: 0.5em auto; padding: 12px 16px; border: none; border-radius: 8px;
  font-size: 1rem; background: #2d6cdf; color: #fff; cursor: pointer;
}
button { width: 100%%;} @media (min-width: 600px) { button { width: auto; min-width: 100px; } }
button:hover { background: #005fcc; } 

img, video { display: block; 
  margin: 10px auto; max-width: 100%%; max-height: 100vh; }

.plot_container canvas { display: block; 
  margin: 1em auto; 
  background: #fff; border: 1px solid #eee;
}

.inline { display: inline-block;
  margin: 5px; width: auto;
}
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