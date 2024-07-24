
// To Silence warnings from external libraries
#include "include/util.h"
#include <corecrt.h>
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

// #include "./external/tracy-0.10/public/tracy/Tracy.hpp"


#pragma GCC diagnostic pop

#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

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
    SDL_Rect*   curs;             // later animate it.
}rendered_text_t;

typedef struct cursor_t{
    size_t x;
    size_t y;
}cursor_t;

typedef struct line_t{
    char*       data;
    size_t      size;                   // actual text size inside the line
    size_t      cap;                    // actual memory allocated to the line.
    size_t      end_row;                // last row of current line, used in alot of calculations.
    size_t      row_off;                // offset within line, when the first screen row is a continuation of a previous line
}line_t;

typedef struct editor_t{
    size_t      screen_rows;             // should change according to scale
    size_t      screen_cols;
    line_t*     l;                       // array of lines
    size_t      l_num;                   // number of actual lines
    size_t      l_cap;                   // length of allocated array of lines
    size_t      curr_l;                  // current line
    size_t      l_off;                   // first line currently scrolled to, start rendering from here
    cursor_t    curs;                    // cursor position
    size_t      pad;                     // padding to the left of the screen, where line number is.
}editor_t;

typedef struct marker_t{
    size_t line;
    size_t idx;
}marker_t;

typedef struct selection_t{
    marker_t start;
    marker_t end;
}selection_t;

typedef enum action_t{

}action_t;

typedef struct history_t{
    char ch;
    action_t action;
    size_t line;
    size_t idx;
}history_t;

/* Global Flags */
bool running = true;
bool resize  = true;
bool rescale = true;
bool render  = true;
bool selection = false;

// Animation
float duration = 0.5f;
Uint32 startTime ;

selection_t slct = {};

void set_screen_dimensions(editor_t *e , rendered_text_t *text);
void insert_text_at(editor_t *e, const char*text,size_t text_size);
void render_n_text_file(editor_t *e, rendered_text_t *text);
size_t get_index_in_line(editor_t *e);
size_t get_line_from_row(editor_t *e, size_t row);
void editor_append_line(editor_t *e, char *s, size_t len);
bool snap_cursor(editor_t *e);
void editor_delete_line(editor_t *e);
void update_current_line(editor_t *e); 


/*---------------- LINE --------------------------- */

// initialize a line
void line_constructor(line_t *l, char *s, size_t len)
{
    // Populate line 
    l->size = len;
    l->cap  = len+1;
    l->data = (char *)check_ptr(malloc(sizeof(*(l->data))*l->cap));
    memcpy(l->data, s, len);
    l->data[len] = '\0';
    l->row_off=0;
    l->end_row=0;
}

void line_insert_at(line_t *l, size_t idx, const char *s, size_t n)
{
    // Expand allocated memory to line when we exceed the capacity.
    while((l->size + n) >= l->cap){
        l->cap*=2;
        l->data = (char *)realloc(l->data, l->cap);
        if(!l->data){
            DEBUG_PRT("realloc failed\n");
            exit(1);
        }
    }
    // move everything after index to create a gap for inserted text
    // dont forget the null termination ! 
    memmove(&l->data[idx+n], &l->data[idx], l->size-idx+1);
    // copy text into the created gap
    memcpy(&l->data[idx],s,n);
    
    // update size
    l->size+=n;
}

void line_append(line_t *l, const char *s, size_t n)
{

}

void set_screen_dimensions(editor_t *e , rendered_text_t *text)
{
    //ZoneScoped;
    int w,h;
    SDL_Window *window = SDL_RenderGetWindow(text->font->renderer);
    SDL_GetWindowSize(window,&w,&h);
    e->screen_rows  =  (size_t)floorf(((float)h / ((float)text->font->font_char_height * text->scale)));
    e->screen_cols  =  (size_t)floorf(((float)w / ((float)text->font->font_char_width  * text->scale) - (float)e->pad));
}

size_t get_first_screen_row(editor_t *e)
{
    // first screen row is, first screen line row + row offset within this line  
    size_t line_first_row = (e->l_off > 0) ? (e->l[e->l_off-1].end_row+1) : 0;
    return line_first_row + e->l[e->l_off].row_off;
}

size_t get_last_screen_row(editor_t *e)
{
    //  last screen row is first screen row + screen rows
    size_t line_first_row = (e->l_off > 0) ? (e->l[e->l_off-1].end_row+1) : 0;
    return line_first_row + e->l[e->l_off].row_off + e->screen_rows;
}

/*  ---------------- EDITOR --------------------------- */
void init_editor(rendered_text_t *text, editor_t *e)
{
    set_screen_dimensions(e, text);

    // start with at least 10 allocated lines
    e->l_cap        =  10;
    e->l_num        =   0;
    e->l            =  (line_t *) check_ptr(malloc(sizeof(*(e->l))*e->l_cap));

    e->curr_l       =   0; 
    e->l_off        =   0;

    e->curs.x       =   0;
    e->curs.y       =   0;

    e->pad          =   3;         
}

// Open a file inside the editor
void editor_open(editor_t *e, const char *file_path)
{
    FILE *file =(FILE *) check_ptr(fopen(file_path, "rb"));
    
    char *line;

    size_t  line_capacity = 0;
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

void editor_append_line(editor_t *e, char *s, size_t len)
{
    // Expand lines array when we exceed capacity 
    if(e->l_num >= e->l_cap){
        e->l_cap*=2;
        e->l = (line_t *)check_ptr(realloc(e->l,sizeof (line_t) * e->l_cap));
    }

    line_t *line = &e->l[e->l_num];

    line_constructor(line, s, len);
    e->l_num++;
}

// Delete current line 
void editor_delete_line(editor_t *e)
{
    // if current line is zero do nothing at all
    if(e->curr_l > 0){
        free(e->l[e->curr_l].data);
        // move rest of lines back.
        memmove(&e->l[e->curr_l],&e->l[e->curr_l+1],sizeof(line_t) * (e->l_num - e->curr_l));
        e->l_num--;
    }
}

void editor_create_line(editor_t *e)
{
    e->l_num++;
    // Expand lines array when we exceed capacity 
    if(e->l_num >= e->l_cap){
        e->l_cap*=2;
        e->l = (line_t *)check_ptr(realloc(e->l,sizeof (line_t) * e->l_cap));
    }
    memmove(&e->l[e->curr_l+2],&e->l[e->curr_l+1],sizeof(line_t) * (e->l_num - e->curr_l+1));

    // reset line 
    e->l[e->curr_l].data[0] = '\0'; 
    e->l[e->curr_l].size = 1; 
}

void editor_to_file(editor_t *e, const char* path)
{
    FILE *file = (FILE *) check_ptr(fopen(path, "w"));
    for (size_t i=0 ; i<e->l_num; i++){
        fprintf(file, "%s\n",e->l[i].data);
    }
    fclose(file);
}

// TODO: Find a simpler way.
void editor_scroll(editor_t *e)
{
    //ZoneScoped;
    // first row rendered on screen
    size_t first_screen_row = get_first_screen_row(e);

    // scroll up if cursor y position is smaller than first screen row 
    if(e->curs.y < first_screen_row){
        // scroll up remaining of wrapped line rows
        if(e->l[e->l_off].row_off > 0){
            e->l[e->l_off].row_off--;
        }else{
            // scroll up a new line
            e->l_off--;

            size_t line_first_row   = (e->l_off > 0) ? (e->l[e->l_off-1].end_row+1) : 0;
            size_t rows_per_line    = e->l[e->l_off].end_row - line_first_row;

            // set line offset to last row of previous line
            e->l[e->l_off].row_off        = rows_per_line;
        }
    }

    // Upcoming row when we scroll down 
    size_t last_screen_row = get_last_screen_row(e);

    if(e->curs.y >= last_screen_row){
        size_t line_first_row        = (e->l_off > 0) ? (e->l[e->l_off-1].end_row+1) : 0;
        size_t rows_per_line =       (e->l[e->l_off].end_row - line_first_row);

        // scroll down remaining of wrapped line rows
        if(e->l[e->l_off].row_off < rows_per_line){
            e->l[e->l_off].row_off++;
        }else{
            // scroll down a new line.
            e->l_off++;
            e->l[e->l_off].row_off=0;
        }
    }
}

void insert_text_at(editor_t *e, const char* text, size_t text_size)
{
    //ZoneScoped;
    size_t idx = get_index_in_line(e);
    line_insert_at(&(e->l[e->curr_l]), idx, text, text_size);    
}

void append_text_to(editor_t *e, const char* text, size_t text_size, size_t line)
{
    //ZoneScoped;
    size_t size     = e->l[line].size;
    
    while(size + text_size >= e->l[line].cap){
        e->l[line].cap*=2;
        e->l[line].data = (char *)realloc(e->l[line].data, sizeof(char) * e->l[line].cap);
    }
    memcpy(&e->l[line].data[size], text, text_size+1);
    e->l[line].size+=text_size;
}

void text_buffer_backspace(editor_t *e)
{
    // current index in line
    size_t idx = get_index_in_line(e);
    // size of the entire line
    size_t size = e->l[e->curr_l].size;

    // Backspacing at the start of a line
    if(e->curs.x == 0 && idx == 0 && e->curr_l > 1){
        // append rest of line to the previous line
        append_text_to(e, &e->l[e->curr_l].data[idx], size-idx, e->curr_l-1);
        // delete current line
        editor_delete_line(e);
    }
    else if(e->curs.x > 0 && size > 0)
    {
        memmove(&e->l[e->curr_l].data[idx-1],&e->l[e->curr_l].data[idx],size-idx+1);
        e->l[e->curr_l].size-=1;
    }
}

void text_buffer_enter(editor_t *e)
{
    // current index in line
    size_t idx = get_index_in_line(e);
    // size of the entire line
    size_t size = e->l[e->curr_l].size;

    editor_create_line(e);
    // append rest of line to the previous line
    append_text_to(e, &e->l[e->curr_l].data[idx], size-idx, e->curr_l+1);
    // delete current line
}

// Binary search to line from row
size_t get_line_from_row(editor_t *e, size_t row) 
{
    size_t high = e->l_num - 1;
    size_t low = 0;
    size_t mid;
    size_t item;
    size_t target = 0;
    

    if (e->l_num == 0){
        return 0;
    }

    if(row > e->l[high].end_row){
        return e->l_num-1;
    }
     
    if (e->l[high].end_row < row) {
        return high;
    }

    while (low <= high) 
    {
        mid = (low + high) / 2;

        item = (mid > 0) ? e->l[mid-1].end_row:0;

        if (item > row) {
            high = mid - 1;
        } else if (item < row) {
            target = mid;
            low = mid + 1;
        } else {
            return mid-1;
        }
    }
    return target;
}

void update_current_line(editor_t *e) 
{
    e->curr_l = get_line_from_row(e, e->curs.y);
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
        if(e->curs.y > e->l[e->l_num-1].end_row){
            e->curs.y = e->l[e->l_num-1].end_row;
        }
    }
}


/* -----------------------  Cursor Movement------------------- */
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
    if(e->curs.y < e->l[e->l_num-1].end_row){
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
    // first row of current line
    size_t line_first_row = (e->curr_l > 0) ? (e->l[e->curr_l-1].end_row+1) : 0;
    // row we are currently at from the line rows
    size_t line_row = e->curs.y - line_first_row ;
    // index = row*cols+col
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
    while(e->curs.y < e->l[e->l_num-1].end_row){
        e->curs.y++;
        editor_scroll(e);
    }
    update_current_line(e);
    snap_cursor(e);
}

void move_cursor_page_up(editor_t *e)
{
    size_t idx = (e->l_off > 0) ? e->l_off-1:0;
    size_t rows = (e->l[idx].end_row+1) + (e->l[e->l_off].row_off);
    while(e->curs.y > rows){
        move_cursor_up(e);
    }
}

void move_cursor_page_down(editor_t *e)
{
    size_t idx = (e->l_off > 0) ? e->l_off-1:0;
    size_t rows = (e->l[idx].end_row+1) + (e->l[e->l_off].row_off);
    while(e->curs.y < (rows + e->screen_rows -1))
    {
        move_cursor_down(e);
    }
}


/* ----------------  Events -------------------- */
void poll_events(rendered_text_t *text, editor_t *e)
{
    //ZoneScoped;

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
                    case SDLK_ESCAPE:
                        if(selection){
                            selection = false;
                            slct.start.line = 0;
                            slct.start.idx = 0;
                            slct.end.line = 0;
                            slct.end.idx = 0;
                        }
                        break;
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

                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                    {
                        if(!selection){
                            selection = true;
                            slct.start.line = e->curr_l;
                            slct.start.idx = get_index_in_line(e);
                        }
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
                        if(e->curs.y < e->l[e->l_num-1].end_row){
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

            case SDL_KEYUP:{
                switch(event.key.keysym.sym){
                    case SDLK_RSHIFT:
                    case SDLK_LSHIFT:
                    if(selection){
                        // selection = false;
                        slct.end.line = e->curr_l;
                        slct.end.idx = get_index_in_line(e);
                        break;
                    }
                    default:
                        break;
                }
                    
                
            }break;
        }
    }
}

/* ----------------  Rendering -------------------- */
void set_text_mode(rendered_text_t *text)
{
    log_error(SDL_SetTextureColorMod(text->font->font_texture, text->color.r, text->color.g, text->color.b));
    log_error(SDL_SetTextureAlphaMod(text->font->font_texture,text->color.a));
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
    //ZoneScoped;
    if (text->ch < ASCII_LOW || text->ch > ASCII_HIGH){
        if(text->ch != '\n'){
            DEBUG_PRT("Unknown character!");
            exit(1);
        }
    }

    const size_t index = text->ch - ASCII_LOW; // ascii to index

    const SDL_Rect dst_rect = {
        .x = (int) floorf((text->pos.x + e->pad) * text->font->font_char_width * text->scale),
        .y = (int) floorf(text->pos.y * text->font->font_char_height * text->scale),
        .w = (int) floorf(text->font->font_char_width * text->scale),
        .h = (int) floorf(text->font->font_char_height * text->scale),
    };
    {
        //ZoneScoped;
        log_error(SDL_RenderCopy(text->font->renderer, text->font->font_texture, &(text->font->glyph_table[index]), &dst_rect));
    }
}


void render_n_string(editor_t *e,rendered_text_t *text,char *string, size_t text_size)
{
    //ZoneScoped;

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
        if(text->ch == '<' || text->ch == '>'){
            if(highlight == true && text->ch == '>'){
                text->color.r=255;
                text->color.g=255;
                text->color.b=255;
                set_text_mode(text);
                highlight = false;
            }else if (text->ch == '<'){
                highlight = true;
                text->color.r=199;
                text->color.g=146;
                text->color.b=234;
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

void render_selection(editor_t *e, rendered_text_t *text)
{
    if (!selection || (slct.end.line == 0 && slct.end.idx == 0)){
        return;
    }

    if(slct.start.line == slct.end.line && slct.start.idx == slct.end.idx){
        return;
    }

    // start line always smaller to make calculations easier
    // as  you can select left or right
    if(slct.start.line > slct.end.line){
        size_t tmp = slct.start.line;
        slct.start.line = slct.end.line;
        slct.end.line = tmp;

        tmp = slct.start.idx;
        slct.start.idx = slct.end.idx;
        slct.end.idx = tmp;
    }

    size_t x = 0;
    size_t y = 0;

    size_t rectx = 0;
    size_t rectxend = 0;

    size_t idx_in_line = 0;

    size_t idx = (e->l_off > 0) ? e->l_off-1:0;
    size_t screen_start_row;

    if(e->l[idx].end_row == 0){
        screen_start_row = (e->l[e->l_off].row_off);
    }else{
        screen_start_row = (e->l[idx].end_row+1) + (e->l[e->l_off].row_off);
    }

    if(get_line_from_row(e, screen_start_row) > slct.end.line){
        return;
    }

    // iterate screen rows
    for(size_t i = 0; i < e->screen_rows; i++)
    {
        x= 0;
        y= i;
        size_t current_row = screen_start_row+i;
        size_t line = get_line_from_row(e, current_row);
        size_t line_first_row =  (line > 0) ? e->l[line-1].end_row+1 : 0;

        if(e->l[line].end_row < current_row){
            break;
        }

        if(line < slct.start.line){
            continue;
        }

        if(line == slct.start.line && line == slct.end.line)
        {
            if(current_row == line_first_row){
                idx_in_line = 0;
            }
            for (size_t j = 0; j< e->screen_cols; j++) {
                if(idx_in_line == slct.start.idx){
                    rectx = x;
                }else if (idx_in_line == slct.end.idx){
                    rectxend = x;
                }
                idx_in_line++;
                x++;
            }
            const SDL_Rect dst_rect = {
                .x = (int) floorf((rectx + e->pad) * text->font->font_char_width * text->scale),
                .y = (int) floorf(y * text->font->font_char_height * text->scale),
                .w = (int) floorf((rectxend-rectx) * text->font->font_char_width * text->scale),
                .h = (int) floorf(text->font->font_char_height * text->scale),
            };
            log_error(SDL_SetRenderDrawColor(text->font->renderer, 76,88,99,255));
            log_error(SDL_RenderFillRect(text->font->renderer, &dst_rect));
        }

    }
}


void render_line_number(editor_t *e, rendered_text_t *text)
{
    //ZoneScoped;

    size_t max_width =0;
    size_t x = 0;
    size_t y = 0;

    size_t idx = (e->l_off > 0) ? e->l_off-1:0;
    size_t screen_start_row;

    if(e->l[idx].end_row == 0){
        screen_start_row = (e->l[e->l_off].row_off);
    }else{
        screen_start_row = (e->l[idx].end_row+1) + (e->l[e->l_off].row_off);
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
    //ZoneScoped;

    // If window is resized recompute screen columns and rows
    if(resize){
        set_screen_dimensions(e, text);
        resize=false;
    }

    size_t rows = 0;
    text->pos.x = 0;
    text->pos.y = 0;

    // Iterate over each line in screen
    for(size_t i = e->l_off; i < e->l_num; i++)
    {
        // only render lines in screen 
        size_t len = e->l[i].size;

        // absolute starting row of current line, its just the end of the previous line + 1
        // A first pass is required to calculate all the end rows when line offset is zero
        size_t line_first_row = (i > 0) ? (e->l[i-1].end_row+1) : (size_t)text->pos.y;

        // Wrap around if line length is larger than number of screen columns
        if(len > e->screen_cols)
        {
            // if length of text is larger than screen columns 
            while(len > e->screen_cols){
                // start rendering from not only line offset but also offset within current wrapped line
                if(e->l[i].row_off > rows){
                    rows++;
                    len -= e->screen_cols;
                }else{
                    len -= e->screen_cols;
                    // render visible rows of current line
                    if (render)
                        render_n_string(e, text, &e->l[i].data[e->screen_cols*rows], e->screen_cols);
                    // rendered a row, reset x position and increment y position
                    text->pos.x = 0;
                    text->pos.y++;
                    rows++;
                }
            }
            // render remaining part of line
            if (render)
                render_n_string(e, text,&e->l[i].data[e->screen_cols*rows],len);

            // absolute row of the end of the current line
            e->l[i].end_row = line_first_row + rows;

            text->pos.x = 0;
            text->pos.y++;
            rows = 0;
        }
        else // line fits entirely in screen no wrapping required.
        {
            if (render)
                render_n_string(e, text,e->l[i].data, len);

            // end row same as start row
            e->l[i].end_row = line_first_row;
            text->pos.x = 0;
            text->pos.y++;
        }

        // when the entire screen is full of text just break, no need to go further.
        // unless a scale occurs , then we must iterate through rest of rows to calculate
        // end row size But we disable rendering text
        if (((text->pos.y > e->screen_rows))){
            // disable rendering
            render = false;
            // if no rescale occured just break
            if(rescale == false){
                render = true;
                break;
            }
        }
        // done recalculation
        if(i == e->l_num-1){
            rescale = false;
            render = true;
        }
    }
}

float lerp(float a, float b, float t){ 
    return a + t * (b - a);
}

void move_cursor_rect(editor_t *e, rendered_text_t *text)
{
    size_t screen_row_start = (e->l_off > 0) ? (e->l[e->l_off-1].end_row + 1) : 0;

    assert(e->curs.y >= (screen_row_start + e->l[e->l_off].row_off));

    // cursor position on screen is cursor absolute position minus the offset
    size_t cursor_screen_pos = e->curs.y - (screen_row_start + e->l[e->l_off].row_off);

    text->curs->x = (int) floorf(text->font->font_char_width  * text->scale *  (float)(e->curs.x + e->pad));  // x is same as we do text wrapping
    text->curs->y = (int) floorf(text->font->font_char_height * text->scale * (float)cursor_screen_pos);
    text->curs->w = (int) floorf((text->font->font_char_width * text->scale) / 4);
    text->curs->h = (int) floorf(text->font->font_char_height * text->scale);
}

void move_cursor_rect_animated(editor_t *e, rendered_text_t *text)
{
    static size_t prev_x = e->curs.x;
    static size_t prev_y = e->curs.y;

    if(prev_x != e->curs.x){
        startTime = SDL_GetTicks();
        prev_x = e->curs.x;
    }
    if(prev_y != e->curs.y){
        startTime = SDL_GetTicks();
        prev_y = e->curs.y;
    }

    Uint32 currentTime = SDL_GetTicks();
    float elapsedTime = (currentTime - startTime) / 1000.0f;
    float t = elapsedTime / duration;

    // Clamp t to [0, 1]
    if (t > 0.98f){ 
        t = 1.0f;
    }
        

    size_t screen_row_start = (e->l_off > 0) ? (e->l[e->l_off-1].end_row + 1) : 0;

    assert(e->curs.y >= (screen_row_start + e->l[e->l_off].row_off));

    // cursor position on screen is cursor absolute position minus the offset
    size_t cursor_screen_pos = e->curs.y - (screen_row_start + e->l[e->l_off].row_off);


    text->curs->x = (int) floorf(lerp(text->curs->x,text->font->font_char_width* text->scale*(float)(e->curs.x + e->pad),t));  // x is same as we do text wrapping
    text->curs->y = (int) floorf(lerp(text->curs->y,text->font->font_char_height * text->scale * (float)cursor_screen_pos,t));
    text->curs->w = (int) floorf((text->font->font_char_width * text->scale) / 4);
    text->curs->h = (int) floorf(text->font->font_char_height * text->scale);
}

void render_cursor(editor_t *e, rendered_text_t *text)
{
    //ZoneScoped;
    move_cursor_rect(e, text);
    log_error(SDL_SetRenderDrawColor(text->font->renderer, 255,203,100,255));
    log_error(SDL_RenderFillRect(text->font->renderer, text->curs));
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

    // Precompute glyph table
    for(size_t i = ASCII_LOW; i <= ASCII_HIGH; i++)
    {
        const size_t index  = i - ASCII_LOW;        // ascii to index
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
    char *ascii_chars  = (char *)malloc(sizeof(char)*size+1);
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
	unsigned char* buffer = (unsigned char *)malloc(sizeof(unsigned char)*(size_t)file_size);

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

    Uint32* pixels = (Uint32*)malloc((size_t)w * (size_t)h * sizeof(Uint32));

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

    SDL_SetRenderDrawBlendMode(*renderer, SDL_BLENDMODE_BLEND);
}

int main(int argv, char** args)
{   
    //ZoneScoped;

    (void) argv;
    (void) args;

    Uint32 start,elapsedTime;

    SDL_Window*   window    = NULL;
    SDL_Renderer* renderer  = NULL;
    SDL_Color     bg_color  = {.r=33,.g=33,.b=33,.a=255};
    SDL_Rect      cursor    = {.x=0,.y=0,.w=0,.h=0};

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

    init_editor(&text, &e);
    editor_open(&e, "./test.txt");

    startTime = SDL_GetTicks();
    
    while(running)
    {
        start = SDL_GetTicks();

        poll_events(&text, &e);

        log_error(SDL_SetRenderDrawColor(renderer,bg_color.r,bg_color.g,bg_color.b,bg_color.a));
        log_error(SDL_RenderClear(renderer));

        editor_scroll(&e);

        render_selection(&e, &text);
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