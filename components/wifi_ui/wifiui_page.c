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
    page->use_websocket = true;
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

typedef struct {
    wifiui_element_t common;
    const char* html;
} wifiui_element_html_t;
static dstring_t* return_html(const wifiui_element_t* self)
{
    wifiui_element_html_t* self_html = (wifiui_element_html_t*)self;
    dstring_t* html = dstring_create(64);
    dstring_appendf(html, "<div class='wrap_text'>%s</div>", self_html->html);
    return html;
}
size_t wifiui_add_html(wifiui_page_t* page, const char * html)
{
    wifiui_element_html_t* html_element = (wifiui_element_html_t*)malloc(sizeof(wifiui_element_html_t));
    set_default_common(&html_element->common, WIFIUI_STATIC_TEXT, return_html);
    html_element->html = strdup(html);

    return wifiui_add_element(page, (const wifiui_element_t*) html_element);
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
    
    dstring_appendf(html, html_head_template, page->title, page->title);
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
width: 100vmin; margin: 0 auto; padding: 1em 1em;
}

button { display: block;
margin: 0.5em auto; padding: 0.6em 1.2em; border: none; border-radius: 0.5em;
font-size: 1em; background: #2d6cdf; color: #fff; cursor: pointer;
}
button { width: 100%%;} @media (min-width: 600px) { button { width: auto; min-width: 100px; } }
button:hover { background: #005fcc; } 

img, video { display: block; margin: 0.5em auto; max-width: 100%%; max-height: 100vh; }

.plot_container canvas { display: block; margin: 0.5em auto; background: #fff; border: 1px solid #eee;}
.inline { display: inline-block; margin: 0.5em; width: auto;}
.width_fixed {white-space:nowrap; overflow:hidden; text-overflow:ellipsis;}
.wrap_text { white-space: pre-wrap; }
.multi_input { font-family: inherit; font-size: inherit; line-height: inherit; width: 100%%; box-sizing: border-box; resize: none; overflow: hidden; min-height: 1.6em; padding: 0.5em; white-space: pre-wrap; word-break: break-all; border: 1px solid #ccc; border-radius: 0.5em; margin-bottom: -0.4em; }
.single_input { font-family: inherit; font-size: inherit; line-height: inherit; width: 100%%; box-sizing: border-box; resize: none; overflow: hidden; min-height: 1.6em; padding: 0.5em; white-space: pre-wrap; word-break: break-all; border: 1px solid #ccc; border-radius: 0.5em; }
.combo_input { width: 100%%; padding: 8px; box-sizing: border-box; }
.combo_list { top: 100%%; left: 0; right: 0; border: 1px solid #ccc; border-top: none; overflow-y: auto; display: none; z-index: 1000; }
.combo_item { padding: 6px 8px; cursor: pointer; }
.combo_item:hover { background: #def; }
.serial_log {width: 100%%; height: 200px; border: 1px solid #ccc; resize: none; overflow-y: auto; word-break: break-all; overflow-wrap: break-word; background:#000; color:#fff; }
</style>
<script>
function fit_textarea_height(id){ t = document.getElementById(id); t.style.height = 'auto'; t.style.height = t.scrollHeight + 'px'; }
</script>
</head>
<body>
<header style='position:sticky; top:0; background:#fff; font-size:1.2rem; font-weight:bold; display:flex; justify-content:space-between; align-items:center;'>
 <span>%s</span>
 <span id='websocket_status' style='width:0.8em; height:0.8em; background:#0f0; border-radius:50%%;'></span>
</header>
)";

const char * html_websocket_template = R"(
<script>
let ws = new WebSocket('ws://' + location.host + location.pathname + '/ws');
let ws_actions = {};
let last_ws_pong_time_ms = Date.now();
ws.binaryType = 'arraybuffer';
ws.onmessage = (evt)=>{
 if(evt.data.byteLength === 0) { last_ws_pong_time_ms = Date.now();
 }else{
  var d = new DataView(evt.data);
  let eid = d.getUint16(0, true);
  if (eid in ws_actions) { ws_actions[eid](evt.data.slice(2)); }
 }
};
const cstr2str = (array)=>{
 const d = new Uint8Array(array);
 const i = d.indexOf(0);
 return new TextDecoder('utf-8').decode(i >= 0 ? d.subarray(0, i) : d);
};
const str2cstr = (str)=>{
 const e = new TextEncoder();
 const d = e.encode(str);
 const cstr = new Uint8Array(d.length + 1);
 cstr.set(d, 0);
 cstr[d.length] = 0; //NULL
 return cstr;
};
const floats2bytes = (floats)=>{
 return new Uint8Array((new Float32Array(floats)).buffer);
};
const bytes2floats = (bytes)=>{
 return new Float32Array(bytes);
};
const ws_send_with_eid = (eid, d)=>{
 const h = new Uint8Array(2);
 h[0] = eid & 0xff;
 h[1] = (eid >> 8) & 0xff;
 const pd = new Uint8Array(h.length + d.length);
 pd.set(h, 0);
 pd.set(d, h.length);
 ws.send(pd);
};
const ws_ping = ()=>{ ws_send_with_eid(0, str2cstr(location.pathname)); }
window.addEventListener('beforeunload',()=>{ if(ws.readyState===WebSocket.OPEN) ws.close(); });
setInterval(ws_ping, 1000);
setInterval(()=>{
  const stat_ele = document.getElementById('websocket_status');
  const hbeat_sec = (Date.now() - last_ws_pong_time_ms) / 1000;
  //stat_ele.innerText = 'server status: ' + hbeat_sec.toFixed(3);
  stat_ele.style.background = (hbeat_sec > 5? '#f00': (hbeat_sec > 3? '#ff0': '#0f0'));
 },100);
</script>
)";