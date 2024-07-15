
// To Silence warnings from external libraries
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include "external/include/SDL2/SDL.h"
#include "external/include/SDL2/SDL_rect.h"
#include "external/include/SDL2/SDL_rwops.h"
#include "external/include/SDL2/SDL_render.h"
#include "external/include/SDL2/SDL_error.h"
#include "external/include/SDL2/SDL_events.h"
#include "external/include/SDL2/SDL_image.h"
#include "external/include/SDL2/SDL_surface.h"
#include "external/include/SDL2/SDL_timer.h"
#include "external/include/SDL2/SDL_scancode.h"
#include "external/include/SDL2/SDL_video.h"
#include "external/include/SDL2/SDL_keycode.h"
#include "external/include/SDL2/SDL_pixels.h"
#include "external/include/SDL2/SDL_ttf.h"
#include "external/include/SDL2/SDL_scancode.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_INCLUDE_STB_RECT_PACK_H
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "./external/include/stb_image.h"
#include "./external/include/stb_image_write.h"
#include "./external/include/stb_truetype.h"
#include "./external/include/stb_rect_pack.h"

#include "./external/tracy-0.10/public/tracy/Tracy.hpp"


#pragma GCC diagnostic pop

#include <stdio.h>
#include <assert.h>
#include <corecrt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "./include/util.h"


#define SCREEN_WIDTH        1280 
#define SCREEN_HEIGHT       720

#define FONT_ROWS           6   
#define FONT_COLS           18

#define ASCII_LOW           32
#define ASCII_HIGH          126


typedef struct font_t{
    SDL_Renderer*    renderer;
    SDL_Texture*     font_texture;
    size_t           font_char_width;
    size_t           font_char_height;
    const char*      path;                                                          // path of either font or image
    SDL_Rect         glyph_table[ASCII_HIGH - ASCII_LOW + 1];
}font_t;

typedef struct rendered_text_t{
    font_t*     font;
    abuf*       text;                 
    char        ch;               // Current character getting rendered.
    vec2_t      pos;              // Rendering Position.
    SDL_Color   color;            // Rendered text color.
    float       scale;
    SDL_Rect*   curs;           // later animate it.
}rendered_text_t;

typedef struct cursor_t{
    size_t x;
    size_t y;
}cursor_t;

typedef struct line_t{
    size_t size;
    size_t cap;
    char*  data;
    size_t end_row;
    size_t l_of;             // offset within line
}line_t;

typedef struct editor_t{
    size_t      screen_rows;             // should change according to scale
    size_t      screen_cols;
    size_t      rows;                    // number of actual rows
    line_t      *l;                      // array of lines
    size_t      l_cap;                   // length of allocated array of lines
    cursor_t    curs;                    // cursor position
    size_t      row_of;                  // row offset currently scrolled to
    size_t      curr_l;                  // current line
    size_t      pad;
}editor_t;

bool running = true;
bool resize  = true;
bool rescale = true;
bool render  = true;

void set_screen_dimensions(editor_t *e , rendered_text_t *text);
void insert_text_at(editor_t *e, const char*text,size_t text_size);
void render_n_text_file(editor_t *e, rendered_text_t *text);

void init_editor(rendered_text_t *text, editor_t *e)
{
    set_screen_dimensions(e, text);

    e->curs.x       =   0;
    e->curs.y       =   0;
    e->rows         =   0;
    e->row_of       =   0;
    e->l_cap        =  10;
    e->curr_l       =   0; 
    e->pad          =   3;         

    // start with at least 10 allocated lines
    e->l            = (line_t *)malloc(sizeof(line_t)*e->l_cap);

    assert(e->l);
}

size_t get_first_screen_row(editor_t *e){
    // current line first row
    size_t line_first_row = (e->row_of > 0) ? (e->l[e->row_of-1].end_row+1) : 0;
    return line_first_row + e->l[e->row_of].l_of;
}

size_t get_last_screen_row(editor_t *e){
    // current line first row
    size_t line_first_row = (e->row_of > 0) ? (e->l[e->row_of-1].end_row+1) : 0;
    return line_first_row + e->l[e->row_of].l_of + e->screen_rows;
}

// TODO: Find a simpler way.
void editor_scroll(editor_t *e)
{
    ZoneScoped;
    // first row rendered on screen
    size_t first_screen_row = get_first_screen_row(e);

    // scroll up if cursor y position is smaller than first screen row 
    if(e->curs.y < first_screen_row){
        // scroll up remaining of wrapped line rows
        if(e->l[e->row_of].l_of > 0){
            e->l[e->row_of].l_of--;
        }else{
            // scroll up a new line
            e->row_of--;

            size_t line_first_row   = (e->row_of > 0) ? (e->l[e->row_of-1].end_row+1) : 0;
            size_t rows_per_line    = e->l[e->row_of].end_row - line_first_row;

            // set line offset to last row of previous line
            e->l[e->row_of].l_of        = rows_per_line;
        }
    }

    // Upcoming row when we scroll down 
    size_t last_screen_row = get_last_screen_row(e);

    if(e->curs.y >= last_screen_row){
        size_t line_first_row        = (e->row_of > 0) ? (e->l[e->row_of-1].end_row+1) : 0;
        size_t rows_per_line =       (e->l[e->row_of].end_row - line_first_row);

        // scroll down remaining of wrapped line rows
        if(e->l[e->row_of].l_of < rows_per_line){
            e->l[e->row_of].l_of++;
        }else{
            // scroll down a new line.
            e->row_of++;
            e->l[e->row_of].l_of=0;
        }
    }
}

void editor_append_line(editor_t *e, char *s, size_t len)
{
    if(e->rows >= e->l_cap){
        e->l_cap*=2;
        e->l = (line_t *)realloc(e->l, sizeof(line_t)* e->l_cap);
    }
    size_t row = e->rows;
    e->l[row].size = len;
    e->l[row].cap = len+1;
    e->l[row].data = (char *)malloc(len + 1);
    memcpy(e->l[row].data, s, len);
    e->l[row].data[len] = '\0';
    e->l[row].l_of=0;
    e->l[row].end_row=0;
    e->rows++;
}

void editor_open(editor_t *e,const char *file_path)
{
    FILE *file =(FILE *) check_ptr(fopen(file_path, "rb"));
    char *line;
    size_t line_capacity = 0;
    ssize_t line_length;
    while((line_length = getline(&line,&line_capacity,file)) != -1){
        // remove new line characters.
        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')){
            line_length--;
        }
        editor_append_line(e, line, (size_t)line_length);
    }
    free(line);
    fclose(file);
}

void set_screen_dimensions(editor_t *e , rendered_text_t *text)
{
    ZoneScoped;

    int w,h;
    SDL_Window *window = SDL_RenderGetWindow(text->font->renderer);
    SDL_GetWindowSize(window,&w,&h);
    e->screen_rows     =  (size_t)floorf(((float)h / ((float)text->font->font_char_height  * text->scale)));
    e->screen_cols  =  (size_t)floorf(((float)w / ((float)text->font->font_char_width   * text->scale) - (float)e->pad));
}

// Binary search to current line
void update_current_line(editor_t *e) 
{
    size_t row = e->curs.y;

    size_t high = e->rows - 1;
    size_t low = 0;
    size_t mid;
    size_t item;
    size_t target = 0;
    

    if (e->rows == 0){
        e->curr_l = 0;
        return;
    }

    if(row > e->l[high].end_row){
        e->curr_l = e->rows-1;
        return;
    }
     
    if (e->l[high].end_row < row) {
        e->curr_l =  high;
        return;
    }

    while (low <= high) 
    {
        mid = (low + high) / 2;

        item = e->l[mid].end_row;

        if (item > row) {
            high = mid - 1;
        } else if (item < row) {
            target = mid;
            low = mid + 1;
        } else {
            e->curr_l = mid;
            return;
        }
    }
    e->curr_l = target;
}

ssize_t get_line_from_row(editor_t *e, size_t row) 
{
    ssize_t high = e->rows - 1;
    ssize_t low = 0;
    ssize_t mid;
    ssize_t item;
    ssize_t target = -1;
    

    if (e->rows == 0){
        return 0;
    }

    if(row > e->l[high].end_row){
        return e->rows-1;
    }
     
    if (e->l[high].end_row < row) {
        return high;
    }

    while (low <= high) 
    {
        mid = (low + high) / 2;

        item = e->l[mid].end_row;

        if (item > row) {
            high = mid - 1;
        } else if (item < row) {
            target = mid;
            low = mid + 1;
        } else {
            return mid;
        }
    }
    return target+1;
}

bool snap_cursor(editor_t *e)
{
    if(e->curs.y == e->l[e->curr_l].end_row){
        // snap cursor to the end of line.
        size_t len ;
        if(e->l[e->curr_l].size == e->screen_cols){
            len = e->screen_cols;
        }else{
            len = e->l[e->curr_l].size % e->screen_cols;
        }
        if(e->curs.x > len){
            e->curs.x = len;
            return true;
        }
    }
    return false;
}

void move_cursor_up(editor_t *e)
{
    if (e->curs.y > 0){
        e->curs.y-=1;
        update_current_line(e);
        snap_cursor(e);
    }
}

void move_cursor_down(editor_t *e)
{
    if(e->curs.y < e->l[e->rows-1].end_row){
        e->curs.y+=1;
        update_current_line(e);
        snap_cursor(e);
    }
}
void move_cursor_right(editor_t *e)
{
    // TODO: limt to screen instead
    if (e->curs.x < e->screen_cols){
        e->curs.x+=1;
        if(snap_cursor(e)){
            e->curs.x=0;
            e->curs.y++;
            update_current_line(e);
        }
    }else{
        e->curs.x=0;
        e->curs.y++;
        update_current_line(e);
    }
}

void move_cursor_left(editor_t *e)
{
    if (e->curs.x > 0){
        e->curs.x-=1;
    }else {
        if(e->curs.y > 0){
            e->curs.y--;
            update_current_line(e);
            e->curs.x = e->screen_cols;
            snap_cursor(e);
        }
    }
}

size_t get_index_in_line(editor_t *e)
{
    size_t screen_row_start = (e->curr_l > 0) ? (e->l[e->curr_l-1].end_row+1) : 0;
    size_t line_row = e->curs.y - screen_row_start;

    return line_row * e->screen_cols + e->curs.x;
}

void move_cursor_to_next_word(editor_t *e)
{
    size_t index_in_line = get_index_in_line(e);
    while(e->l[e->curr_l].data[index_in_line++] != ' ')
    {
        if(index_in_line > e->l[e->curr_l].size){
            break;
        }
        move_cursor_right(e);
    }

}

void move_cursor_file_start(editor_t *e)
{
    while(e->curs.y > 0){
        e->curs.y--;
        editor_scroll(e);
    }
    e->curs.x = 0;
    update_current_line(e);
}

void move_cursor_file_end(editor_t *e)
{
    while(e->curs.y < e->l[e->rows-1].end_row){
        e->curs.y++;
        editor_scroll(e);
    }
    update_current_line(e);
    snap_cursor(e);
}

void move_cursor_page_up(editor_t *e)
{
    size_t idx = (e->row_of > 0) ? e->row_of-1:0;
    size_t rows = (e->l[idx].end_row+1) + (e->l[e->row_of].l_of);
    while(e->curs.y > rows){
        move_cursor_up(e);
    }
}

void move_cursor_page_down(editor_t *e)
{
    size_t idx = (e->row_of > 0) ? e->row_of-1:0;
    size_t rows = (e->l[idx].end_row+1) + (e->l[e->row_of].l_of);
    while(e->curs.y < (rows + e->screen_rows -1))
    {
        move_cursor_down(e);
    }
}

void editor_zoom_in(editor_t *e, rendered_text_t *text)
{
    if(text->scale < 15.0f){
        text->scale+=0.5f;
        rescale = true;
        set_screen_dimensions(e,text);
        update_current_line(e);
        snap_cursor(e);
        if(e->curs.x > e->screen_cols){
            e->curs.x = e->screen_cols;
        }
    }
}

void editor_zoom_out(editor_t *e, rendered_text_t *text)
{
    if(text->scale >= 1.5f){
        text->scale-=0.5f;
        rescale = true;
        set_screen_dimensions(e,text);
        update_current_line(e);
        snap_cursor(e);
        if(e->curs.y > e->l[e->rows-1].end_row){
            e->curs.y = e->l[e->rows-1].end_row;
        }
    }
}

void editor_delete_line(editor_t *e)
{
    if(e->curr_l > 0){
        free(e->l[e->curr_l].data);
        memmove(&e->l[e->curr_l],&e->l[e->curr_l+1],e->rows - e->curr_l + 1);
        e->rows--;
    }
}

void append_text_to(editor_t *e, const char* text, size_t text_size, size_t line)
{
    ZoneScoped;
    size_t size     = e->l[line].size;
    
    while(size + text_size > e->l[line].cap){
        e->l[line].cap*=2;
        e->l[line].data = (char *)realloc(e->l[line].data, sizeof(char) * e->l[line].cap);
    }
    memcpy(&e->l[line].data[size], text, text_size+1);
    e->l[line].size+=text_size;
}

void text_buffer_backspace(editor_t *e)
{
    // if cursor.x = 1 
    // append rest of line to previous line
    // delete line , (memove dance)
    // decrease num of rows
    size_t idx = get_index_in_line(e);
    size_t size = e->l[e->curr_l].size;

    if (idx < 0 || idx > size){
        idx = e->l[e->curr_l].size;
    }
    if(e->curs.x == 0 && idx == 0 && e->curr_l > 1){
        append_text_to(e, e->l[e->curr_l].data + idx, size-idx,e->curr_l-1);
        editor_delete_line(e);
    }else if(e->curs.x > 0 && size > 0){
        memmove(&e->l[e->curr_l].data[idx-1],&e->l[e->curr_l].data[idx],size-idx+1);
        e->l[e->curr_l].size-=1;
    }
}

void editor_to_file(editor_t *e, const char* path)
{
    FILE *file = (FILE *) check_ptr(fopen(path, "w"));
    for (size_t i=0 ; i<e->rows; i++){
        fprintf(file, "%s\n",e->l[i].data);
    }
    fclose(file);
}

void poll_events(rendered_text_t *text, editor_t *e)
{
    ZoneScoped;

    SDL_Event event = {0};
    const Uint8 *keyboard_state_array = SDL_GetKeyboardState(NULL);


    while (SDL_PollEvent(&event)) {
        switch (event.type) 
        {
            case SDL_QUIT:{
                running = false;
            }break;

            case SDL_WINDOWEVENT:{
                if(event.window.event==SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ){
                    resize = true;
                }
            }break;

            case SDL_TEXTINPUT:{
                size_t text_size = strlen(event.text.text);
                insert_text_at(e,event.text.text,text_size);
                render_n_text_file(e,text);
                for(size_t i = 0; i < text_size; i++){
                    move_cursor_right(e);
                }
                
                }break;

            case SDL_KEYDOWN:{
                switch(event.key.keysym.sym){
                    case SDLK_BACKSPACE:
                        text_buffer_backspace(e);
                        move_cursor_left(e);
                        break;
                    case SDLK_RETURN:
                        // TODO: create a new empty line after current cursor (memove dance)
                        // fill it with the rest of line right to current cursor position
                        // move cursor down and set cursor.x = 0
                        break;

                    case SDLK_LEFT:
                        move_cursor_left(e);
                        break;

                    case SDLK_RIGHT:
                        move_cursor_right(e);
                        break;
                    case SDLK_UP:
                        move_cursor_up(e);
                        break;
                    case SDLK_DOWN:
                        move_cursor_down(e);
                        break;
                    case SDLK_PAGEUP:{
                        move_cursor_page_up(e);
                        break;
                    }
                    case SDLK_PAGEDOWN:{
                        move_cursor_page_down(e);
                        break;
                    }
                    case SDLK_HOME:{
                        e->curs.x = 0;
                    }break;

                    case SDLK_END:{
                        e->curs.x = e->screen_cols;
                        snap_cursor(e);
                    }break;

                    default:
                        break;
                }
                if ((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_Q]))
                {
                    running = false;
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && ((keyboard_state_array[SDL_SCANCODE_KP_PLUS]) || (keyboard_state_array[SDL_SCANCODE_EQUALS])))
                {
                    editor_zoom_in(e, text);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && ((keyboard_state_array[SDL_SCANCODE_KP_MINUS]) || (keyboard_state_array[SDL_SCANCODE_KP_MINUS])))
                {
                    editor_zoom_out(e, text);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_HOME]))
                {
                    move_cursor_file_start(e);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_END]))
                {
                    move_cursor_file_end(e);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_RIGHT]))
                {
                    move_cursor_to_next_word(e);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_PAGEDOWN]))
                {
                    for (size_t i =0 ; i< e->screen_rows;i++) {
                        if(e->curs.y < e->l[e->rows-1].end_row){
                            move_cursor_down(e);
                            editor_scroll(e);
                        }else{
                            break;
                        }
                    }
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_PAGEUP]))
                {
                    for (size_t i =0 ; i< e->screen_rows;i++) {
                        if(e->curs.y > 0){
                            move_cursor_up(e);
                            editor_scroll(e);
                        }else{
                            break;
                        }
                    }
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_S]))
                {
                    editor_to_file(e, "save_test.txt");
                }
            }break;
        }
    }
}

SDL_Surface *surface_from_image(const char *path)
{
    int req_format = STBI_rgb_alpha;
    int width, height, n;
    unsigned char *pixels = stbi_load(path, &width, &height, &n, req_format);
    if(pixels == NULL){
        DEBUG_PRT("Couldnt load file : %s because :  %s\n",path, stbi_failure_reason());
        exit(1);
    }
    Uint32 rmask, gmask, bmask, amask;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        int shift = (req_format == STBI_rgb) ? 8 : 0;
        rmask = 0xff000000 >> shift;
        gmask = 0x00ff0000 >> shift;
        bmask = 0x0000ff00 >> shift;
        amask = 0x000000ff >> shift;
    #else // little endian, like x86
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
    #endif

    int depth, pitch;

    depth = 32;
    pitch = 4*width;

    SDL_Surface* surf = (SDL_Surface *)check_ptr(SDL_CreateRGBSurfaceFrom((void*)pixels, width, height, depth, pitch,
                                                            rmask, gmask, bmask, amask));

    return surf;
}

void render_char(editor_t *e, rendered_text_t *text)
{
    ZoneScoped;

    assert(text->ch >= ASCII_LOW  || text->ch == '\n');
    assert(text->ch <= ASCII_HIGH || text->ch == '\n');

    const size_t index = text->ch - ASCII_LOW; // ascii to index

    const SDL_Rect dst_rect = {
        .x = (int) floorf((text->pos.x + e->pad) * text->font->font_char_width * text->scale),
        .y = (int) floorf(text->pos.y * text->font->font_char_height * text->scale),
        .w = (int) floorf(text->font->font_char_width * text->scale),
        .h = (int) floorf(text->font->font_char_height * text->scale),
    };
    {
        ZoneScoped;
        log_error(SDL_RenderCopy(text->font->renderer, text->font->font_texture, &(text->font->glyph_table[index]), &dst_rect));
    }
}

void set_text_mode(rendered_text_t *text)
{
    log_error(SDL_SetTextureColorMod(text->font->font_texture, text->color.r, text->color.g, text->color.b));
    log_error(SDL_SetTextureAlphaMod(text->font->font_texture,text->color.a));
}

void render_n_string(editor_t *e,rendered_text_t *text,char *string,size_t text_size)
{
    ZoneScoped;

    bool highlight = false;
    for(size_t i = 0; i < text_size; i++)
    {
        text->ch = string[i];
        if(text->ch == '/'){
            if((i+1) < text_size && string[i+1] == '/'){
                highlight = true;
                text->color.r=129;
                text->color.g=180;
                text->color.b=70;
                set_text_mode(text);
            }
        }
        if(text->ch == '"'){
            if(highlight == true){
                text->color.r=255;
                text->color.g=255;
                text->color.b=255;
                set_text_mode(text);
                highlight = false;
            }else{
                highlight = true;
                text->color.r=247;
                text->color.g=140;
                text->color.b=98;
                set_text_mode(text);
            }
        }
        render_char(e,text);
        text->pos.x++;
    }
    if (highlight){
        text->color.r=255;
        text->color.g=255;
        text->color.b=255;
        set_text_mode(text);
        highlight = false;
    }
}

void render_seperator(editor_t *e, rendered_text_t *text)
{
    const SDL_Rect dst_rect = (SDL_Rect) {
        .x = (int) floorf(((e->pad-0.75f) * text->font->font_char_width * text->scale)),
        .y = (int) floorf(0),
        .w = (int) floorf((text->font->font_char_width * text->scale)/8),
        .h = (int) floorf(text->font->font_char_height * text->scale * e->screen_rows),
    };
    log_error(SDL_SetRenderDrawColor(text->font->renderer, 56,60,64,255));
    log_error(SDL_RenderFillRect(text->font->renderer, &dst_rect));
}

void render_line_number(editor_t *e, rendered_text_t *text)
{
    ZoneScoped;

    size_t max_width =0;
    size_t x = 0;
    size_t y = 0;

    size_t idx = (e->row_of > 0) ? e->row_of-1:0;
    size_t screen_start_row;
    if(e->l[idx].end_row == 0){
        screen_start_row = (e->l[e->row_of].l_of);
    }else{
        screen_start_row = (e->l[idx].end_row+1) + (e->l[e->row_of].l_of);
    }
    
    for(size_t i = 0; i < e->screen_rows; i++)
    {
        x= 0;
        y= i;
        size_t current_row = screen_start_row+i;
        size_t line = get_line_from_row(e, current_row);
        if(e->l[line].end_row < current_row){
            break;
        }
        if(line == e->curr_l){
            text->color.r = 132;
            text->color.g = 132;
            text->color.b = 132;
        }else{
            text->color.r = 66;
            text->color.g = 66;
            text->color.b = 66;
        }
        set_text_mode(text);
        size_t change = e->l[line].end_row-current_row;
        i+= change;
        
        char number_str[21]; 
        // start from line 1
        sprintf(number_str, "%llu", line+1);

        for (size_t j = 0; number_str[j] != '\0'; j++) 
        {
            size_t index = number_str[j] - ASCII_LOW; // ascii to index

            const SDL_Rect dst_rect = {
                .x = (int) floorf(x++ * text->font->font_char_width * text->scale),
                .y = (int) floorf(y * text->font->font_char_height * text->scale),
                .w = (int) floorf(text->font->font_char_width * text->scale),
                .h = (int) floorf(text->font->font_char_height * text->scale),
            };
            log_error(SDL_RenderCopy(text->font->renderer, text->font->font_texture, &(text->font->glyph_table[index]), &dst_rect));
            max_width=MAX(max_width, j);
        }
    }
    text->color.r = 255;
    text->color.g = 255;
    text->color.b = 255;
    set_text_mode(text);
    e->pad = max_width+2;
    render_seperator(e, text);
}


// TODO : this does too much, it handles word wrapping and computing each line row end 
// Find a way to seperate each functions
void render_n_text_file(editor_t *e, rendered_text_t *text)
{   
    ZoneScoped;

    // If window is resized recompute screen columns and rows
    if(resize){
        set_screen_dimensions(e, text);
        resize=false;
    }

    size_t rows = 0;
    text->pos.x = 0;
    text->pos.y = 0;

    // Iterate over each line
    for(size_t i = 0; i < e->rows; i++)
    {
        // only render lines in screen 
        size_t file_row = i + e->row_of;

        // didnt offset outside of lines
        if(file_row < e->rows)
        {
            size_t len = e->l[file_row].size;

            // absolute startinng row of current line, its just the end of the previous line + 1
            size_t screen_row_start = (file_row > 0) ? (e->l[file_row-1].end_row+1) : (int)text->pos.y;

            // handle word wrapping and scroll
            // Wrap around if line length is larger than number of screen columns
            if(len > e->screen_cols)
            {
                // if length of text is larger than screen columns 
                while(len > e->screen_cols){
                    // start rendering from not only line offset but also offset within current wrapped line
                    if(e->l[file_row].l_of > rows){
                        rows++;
                        len -= e->screen_cols;
                    }else{
                        len -= e->screen_cols;
                        if (render)
                            render_n_string(e, text,e->l[file_row].data+e->screen_cols*rows, e->screen_cols);
                        // rendered a row, reset x position and increment y position
                        text->pos.x = 0;
                        text->pos.y++;
                        rows++;
                    }
                }
                // render remaining part of line
                if (render)
                    render_n_string(e, text,e->l[file_row].data+e->screen_cols*rows,len);

                // absolute row of the end of the current line
                e->l[file_row].end_row = screen_row_start + rows;

                text->pos.x = 0;
                text->pos.y++;
                rows = 0;
            }else{
                // line fits entirely in screen no wrapping required.
                if (render)
                    render_n_string(e, text,e->l[file_row].data, len);
                e->l[file_row].end_row = screen_row_start + rows;
                text->pos.x = 0;
                text->pos.y++;
            }
            // when the entire screen is full of text just break, no need to go further.
            // TODO: fix to when a scale occurs , then we must iterate through rest of rows to calculate end row size
            // But we dont want to render text
            if (((text->pos.y > e->screen_rows))){
                render = false;
                if(rescale == false){
                    render = true;
                    break;
                }
            }
        }
        if(file_row == e->rows-1){
            rescale = false;
            render = true;
        }
    }
}

void move_cursor_rect(editor_t *e, rendered_text_t *text)
{
    size_t screen_row_start = (e->row_of > 0) ? (e->l[e->row_of-1].end_row + 1) : 0;

    assert(e->curs.y >= (screen_row_start + e->l[e->row_of].l_of));

    // cursor position on screen is cursor absolute position minus the offset
    size_t cursor_screen_pos = e->curs.y - (screen_row_start + e->l[e->row_of].l_of);

    text->curs->x = (int) floorf(text->font->font_char_width * text->scale *  (e->curs.x + e->pad));  // x is same as we do text wrapping
    text->curs->y = (int) floorf(text->font->font_char_height * text->scale * cursor_screen_pos);
    text->curs->w = (int) floorf((text->font->font_char_width * text->scale) / 4);
    text->curs->h = (int) floorf(text->font->font_char_height * text->scale);
}

void render_cursor(editor_t *e, rendered_text_t *text)
{
    ZoneScoped;

    move_cursor_rect(e, text);
    log_error(SDL_SetRenderDrawColor(text->font->renderer, 255,203,100,255));
    log_error(SDL_RenderFillRect(text->font->renderer, text->curs));
}

void insert_text_at(editor_t *e, const char*text, size_t text_size)
{
    ZoneScoped;
    size_t idx = get_index_in_line(e);
    size_t size = e->l[e->curr_l].size;
    size_t capacity = e->l[e->curr_l].cap;

    if (idx < 0 || idx > size){
        idx = e->l[e->curr_l].size;
    } 
    while(size + text_size > capacity){
        e->l[e->curr_l].cap*=2;
        e->l[e->curr_l].data = (char *)realloc(e->l[e->curr_l].data, sizeof(char) * e->l[e->curr_l].cap);
    }
    memmove(&e->l[e->curr_l].data[idx+text_size],&e->l[e->curr_l].data[idx],size-idx+1);

    memcpy(&e->l[e->curr_l].data[idx],text,text_size);
    e->l[e->curr_l].size+=text_size;

}


void init_font_image(font_t *font)
{
    SDL_Surface *font_surface = surface_from_image(font->path);

    font->font_char_height =  (size_t)(font_surface->h/FONT_ROWS);
    font->font_char_width  =  (size_t)(font_surface->w/FONT_COLS);
    // remove black background
    SDL_SetColorKey(font_surface, SDL_TRUE, SDL_MapRGB(font_surface->format, 0x00, 0x00, 0x00));

    font->font_texture = (SDL_Texture *)check_ptr(SDL_CreateTextureFromSurface(font->renderer, font_surface));

    // not needed anymore
    stbi_image_free(font_surface->pixels);
    
    SDL_FreeSurface(font_surface);

    // precompute glyph table
    for(size_t i = ASCII_LOW; i <= ASCII_HIGH; i++)
    {
        const size_t index  = i - ASCII_LOW; // ascii to index
        const size_t col    = index % FONT_COLS;
        const size_t row    = index / FONT_COLS;

        font->glyph_table[index] = (SDL_Rect){
            .x = (int)(col * font->font_char_width),
            .y = (int)(row * font->font_char_height),
            .w = (int)(font->font_char_width),
            .h = (int)(font->font_char_height),
        };
    }
}

void init_font_ttf(font_t *font)
{
    TTF_Init();
    TTF_Font* ttf_font = (TTF_Font*) check_ptr(TTF_OpenFont(font->path, 18));

    size_t size = ASCII_HIGH-ASCII_LOW+FONT_ROWS;
    char *ascii_chars  = (char *)malloc(size+1);
    char ch = ASCII_LOW;

    for (size_t row = 0; row < FONT_ROWS; row++)
    {
        for (size_t col = 0; col < FONT_COLS+1; col++)
        {
            if(col == FONT_COLS || (row * (FONT_COLS+1) + col) == size-2){
                ascii_chars[row * (FONT_COLS+1) + col] = '\n';
                break;
            }
            ascii_chars[row * (FONT_COLS+1) + col] = ch++;
        }
    }
    ascii_chars[size-1] = '\0';

    // const char ascii_chars[] =  " !\"#$%&\'()*+,-./01\n"
    //                             "23456789:;<=>?@ABC\n"
    //                             "DEFGHIJKLMNOPQRSTU\n"
    //                             "VWXYZ[\\]^_`abcdefg\n"
    //                             "hijklmnopqrstuvwxy\n"
    //                             "z{|}~\n"

    SDL_Surface* font_surface = TTF_RenderText_Blended_Wrapped(ttf_font, ascii_chars,COLOR_WHITE,0);
    font->font_char_height    =  (size_t)(font_surface->h/FONT_ROWS);
    font->font_char_width     =  (size_t)(font_surface->w/FONT_COLS);
    font->font_texture        =  (SDL_Texture *)check_ptr(SDL_CreateTextureFromSurface(font->renderer, font_surface));

    // SDL_Rect texture_rect;
    // texture_rect.x = 0; //the x coordinate
    // texture_rect.y = 0; //the y coordinate
    // texture_rect.w = font_surface->w * 0.5 ; //the width of the texture
    // texture_rect.h = font_surface->h * 0.5; //the height of the texture

    // SDL_RenderCopy(font->renderer, font->font_texture, NULL, &texture_rect); 
    // SDL_RenderPresent(font->renderer); //updates the renderer

    free(ascii_chars);
    SDL_FreeSurface(font_surface);
    TTF_CloseFont(ttf_font); 
    TTF_Quit();
    // precompute glyph table
    for(size_t i = ASCII_LOW; i <= ASCII_HIGH; i++)
    {
        const size_t index  = i - ASCII_LOW; // ascii to index
        const size_t col    = index % FONT_COLS;
        const size_t row    = index / FONT_COLS;

        font->glyph_table[index] = (SDL_Rect){
            .x = (int)(col * font->font_char_width),
            .y = (int)(row * font->font_char_height),
            .w = (int)(font->font_char_width),
            .h = (int)(font->font_char_height),
        };
    }
}

// TODO: Needs more work 
void init_font_ttf_stb(font_t *font, float size)
{
    SDL_RWops *rw = (SDL_RWops *) check_ptr(SDL_RWFromFile(font->path, "rb"));

    Sint64 file_size = SDL_RWsize(rw);
	unsigned char* buffer = (unsigned char *)malloc(file_size);

    if(SDL_RWread(rw, buffer, file_size, 1) != 1){
        DEBUG_PRT("Couldnt read from data source :  %s\n",SDL_GetError());
    }
    
    SDL_RWclose(rw);

    stbtt_fontinfo    info;

    if(stbtt_InitFont(&info, buffer, 0) == 0) {
        DEBUG_PRT("Couldnt init font, because :  %s\n",stbi_failure_reason());
        exit(1);
    }

    unsigned char* bitmap = NULL;
    int w, h,xoff,yoff;
    bitmap = stbtt_GetCodepointBitmap(&info, 0, stbtt_ScaleForPixelHeight(&info, size),'i', &w, &h,&xoff,&yoff);

	SDL_Texture* atlas = SDL_CreateTexture(font ->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);

	SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);

    Uint32* pixels = (Uint32*)malloc(w * h * sizeof(Uint32));

	static SDL_PixelFormat* format = NULL;
	if(format == NULL){
        format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    } 
	for(int i = 0; i < w * h; i++){
		pixels[i] = SDL_MapRGBA(format, 0xff, 0xff, 0xff, bitmap[i]);
	}
	SDL_UpdateTexture(atlas, NULL, pixels, w * sizeof(Uint32));

    SDL_Rect texture_rect;
    texture_rect.x = 0; //the x coordinate
    texture_rect.y = 0; //the y coordinate
    texture_rect.w = w ; //the width of the texture
    texture_rect.h = h; //the height of the texture

    SDL_RenderCopy(font->renderer, atlas, NULL, &texture_rect); 
    SDL_RenderPresent(font->renderer); //updates the renderer


	free(pixels);
	free(bitmap);


    // // precompute glyph table
    // for(size_t i = ASCII_LOW; i <= ASCII_HIGH; i++)
    // {
    //     const size_t index  = i - ASCII_LOW; // ascii to index
    //     const size_t col    = index % FONT_COLS;
    //     const size_t row    = index / FONT_COLS;

    //     font->glyph_table[index] = (SDL_Rect){
    //         .x = (int)(chars[index].x0),
    //         .y = (int)(chars[index].y0),
    //         .w = (int)(chars[index].x1 - chars[index].x0),
    //         .h = (int)(chars[index].y1 - chars[index].y0),
    //     };
    // }
    SDL_RWclose(rw);
}

void init_sdl(SDL_Window **window, SDL_Renderer **renderer)
{
    log_error(SDL_Init(SDL_INIT_VIDEO));
    *window = (SDL_Window *)check_ptr(SDL_CreateWindow("Text Editor",
                                                        SDL_WINDOWPOS_CENTERED,
                                                        SDL_WINDOWPOS_CENTERED,
                                                        SCREEN_WIDTH, SCREEN_HEIGHT,
                                                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));

    *renderer = (SDL_Renderer *) check_ptr(SDL_CreateRenderer(*window,-1,SDL_RENDERER_ACCELERATED));
}

int main(int argv, char** args)
{    
    ZoneScoped;

    SDL_Window   *window    = NULL;
    SDL_Renderer *renderer  = NULL;
    Uint32 start,elapsedTime;
    SDL_Color bg_color = {.r=33,.g=33,.b=33,.a=255};
    SDL_Rect cursor = {.x=0,.y=0,.w=0,.h=0};

    init_sdl(&window, &renderer);

    abuf buffer = ABUF_INIT();

    font_t font = {
        .renderer = renderer,
        .path = "./Font/charmap-oldschool_white_cropped.png",
        // .path = "./Font/Ubuntu.ttf",
    };

    init_font_image(&font);
    // init_font_ttf(&font);
    // init_font_ttf_stb(&font, 32);

    rendered_text_t text = {
        .font = &font,
        .text = &buffer,
        .pos = {.x = 0, .y = 0},
        .color = COLOR_WHITE,
        .scale = 1.0f,
        .curs = &cursor,
    };

    editor_t e;

    init_editor(&text,&e);
    editor_open(&e, "./test.txt");
    
    while(running)
    {
        start = SDL_GetTicks();

        poll_events(&text, &e);
        log_error(SDL_SetRenderDrawColor(renderer,bg_color.r,bg_color.g,bg_color.b,bg_color.a));
        log_error(SDL_RenderClear(renderer));
        editor_scroll(&e);
        render_n_text_file(&e,&text);
        render_line_number(&e,&text);
        render_cursor(&e, &text);

        SDL_RenderPresent(renderer);

        elapsedTime = SDL_GetTicks() - start;
        if(elapsedTime < FPS(60)){
            SDL_Delay(FPS(60)-elapsedTime);
        }
    }

    SDL_Quit();

    return 0;
}