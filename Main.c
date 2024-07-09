#include "SDL2/SDL_rect.h"
#include "SDL2/SDL_scancode.h"
#include "SDL2/SDL_video.h"
#include "external/include/SDL2/SDL_keycode.h"
#include "external/include/SDL2/SDL_pixels.h"
#include "external/include/SDL2/SDL_TTF.h"
#include "external/include/SDL2/SDL_scancode.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_surface.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <assert.h>
#include <corecrt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "./include/util.h"


#define SCREEN_WIDTH        1280 
#define SCREEN_HEIGHT       720

#define FONT_ROWS           7   
#define FONT_COLS           18

#define ASCII_DISPLAY_LOW   32
#define ASCII_DISPLAY_HIGH  126

#define MAX_SIZE            2048


#define COLOR_RED           (SDL_Color){255, 0, 0, 255}
#define COLOR_GREEN         (SDL_Color){0, 255, 0, 255}
#define COLOR_BLUE          (SDL_Color){0, 0, 255, 255}
#define COLOR_WHITE         (SDL_Color){255, 255, 255, 255}
#define COLOR_BLACK         (SDL_Color){0, 0, 0, 255}
#define COLOR_YELLOW        (SDL_Color){255, 255, 0, 255}
#define COLOR_CYAN          (SDL_Color){0, 255, 255, 255}
#define COLOR_MAGENTA       (SDL_Color){255, 0, 255, 255}
#define COLOR_ORANGE        (SDL_Color){255, 165, 0, 255}
#define COLOR_PURPLE        (SDL_Color){128, 0, 128, 255}
#define COLOR_PINK          (SDL_Color){255, 192, 203, 255}
#define COLOR_GRAY          (SDL_Color){128, 128, 128, 255}
#define COLOR_LIGHT_GRAY    (SDL_Color){211, 211, 211, 255}
#define COLOR_DARK_GRAY     (SDL_Color){169, 169, 169, 255}
#define COLOR_BROWN         (SDL_Color){139, 69, 19, 255}
#define COLOR_NAVY          (SDL_Color){0, 0, 128, 255}
#define COLOR_LIME          (SDL_Color){0, 255, 0, 255}
#define COLOR_TEAL          (SDL_Color){0, 128, 128, 255}
#define COLOR_MAROON        (SDL_Color){128, 0, 0, 255}
#define COLOR_OLIVE         (SDL_Color){128, 128, 0, 255}

#define UNHEX(color) \
    ((color) >> (8 * 0)) & 0xFF, \
    ((color) >> (8 * 1)) & 0xFF, \
    ((color) >> (8 * 2)) & 0xFF, \
    ((color) >> (8 * 3)) & 0xFF 

/*
    When choosing the data structure I quickly realized the need of representing each line
    as we need to remember the ending position at each previous line
*/

typedef struct font_t{
    SDL_Renderer*    renderer;
    SDL_Texture*     font_texture;
    const char*      path;                                                          // path of either font or image
    SDL_Rect         glyph_table[ASCII_DISPLAY_HIGH - ASCII_DISPLAY_LOW + 1];
}font_t;

typedef struct rendered_text_t{
    font_t *font;
    abuf *text;
    char ch;                    // Current character getting rendered.
    vec2f_t pos;                // Rendering Position.
    SDL_Color color;            // Rendered text color.
    float scale;
}rendered_text_t;

typedef struct buffer_cursor_t{
    size_t x;
    size_t y;
}buffer_cursor_t;

typedef struct line_t{
    size_t size;
    char *data;
    size_t screen_row_end;
    size_t line_offset;             // offset within line
}line_t;

typedef struct editor_t{
    size_t screen_rows;             // should change according to scale
    size_t screen_columns;
    size_t num_rows;                // number of actual rows
    line_t *line;                   
    size_t lines_cap;               // length of allocated array of lines
    buffer_cursor_t cursor;         // cursor position
    size_t row_offset;              // row currently scrolled to
    size_t current_line;
}editor_t;

bool running = true;
bool resize  = true;
bool rescale = true;
bool render  = true;

size_t font_char_width;
size_t font_char_height;
size_t PAD = 3;


void set_screen_dimensions(editor_t *editor , rendered_text_t *text);

void init_editor(rendered_text_t *text, editor_t *editor)
{
    set_screen_dimensions(editor, text);
    editor->cursor.x     =   0;
    editor->cursor.y     =   0;
    editor->num_rows     =   0;
    editor->row_offset   =   0;
    editor->lines_cap    =  10;
    editor->current_line =   0; 
    // start with at least 10 allocated lines
    editor->line = malloc(sizeof(line_t)*editor->lines_cap);
    assert(editor->line);
}

// TODO: Find a simpler way.
void editor_scroll(editor_t *editor)
{
    size_t screen_row_start = (editor->row_offset > 0) ? (editor->line[editor->row_offset-1].screen_row_end+1) : 0;
    // first row rendered on screen
    size_t first_screen_row = screen_row_start + editor->line[editor->row_offset].line_offset;

    // scroll up if cursor y position is smaller than first screen row 
    if(editor->cursor.y < first_screen_row){
        // scroll up remaining of wrapped line rows
        if(editor->line[editor->row_offset].line_offset > 0){
            editor->line[editor->row_offset].line_offset--;
        }else{
            // scroll up a new line
            editor->row_offset--;

            size_t screen_row_start = (editor->row_offset > 0) ? (editor->line[editor->row_offset-1].screen_row_end+1) : 0;
            size_t rows_per_line    = editor->line[editor->row_offset].screen_row_end - screen_row_start;

            editor->line[editor->row_offset].line_offset=rows_per_line;
        }
    }

    // Upcoming row when we scroll down 
    size_t last_screen_row = screen_row_start + editor->line[editor->row_offset].line_offset + editor->screen_rows;

    if(editor->cursor.y >= last_screen_row){
        size_t rows_per_line = (editor->line[editor->row_offset].screen_row_end - screen_row_start);

        // scroll down remaining of wrapped line rows
        if(editor->line[editor->row_offset].line_offset < rows_per_line){
            editor->line[editor->row_offset].line_offset++;
        }else{
            // scroll down a new line.
            editor->row_offset++;
            editor->line[editor->row_offset].line_offset=0;
        }
    }
}

void editor_append_line(editor_t *editor, char *s, size_t len)
{
    if(editor->num_rows >= editor->lines_cap){
        editor->lines_cap*=2;
        editor->line = realloc(editor->line, sizeof(line_t)* editor->lines_cap);
    }
    int row = editor->num_rows;
    editor->line[row].size = len;
    editor->line[row].data = malloc(len + 1);
    memcpy(editor->line[row].data, s, len);
    editor->line[row].data[len] = '\0';
    editor->line[row].line_offset=0;
    editor->line[row].screen_row_end=0;
    editor->num_rows++;

}

void editor_open(editor_t *editor,const char *file_path)
{
    FILE *file =(FILE *) check_ptr(fopen(file_path, "rb"));
    char *line;
    size_t line_capacity= 0;
    ssize_t line_length;
    while((line_length = getline(&line,&line_capacity,file)) != -1){
        // remove new line characters.
        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r'))
        {
            line_length--;
        }
        editor_append_line(editor, line, line_length);
        line_capacity=0;
    }
    free(line);
    fclose(file);
}



// void buffer_insert_text_after_cursor(const char *text)
// {
//     size_t text_size = strlen(event.text.text);
//     const size_t free_space = MAX_SIZE - buffer_size;
//     if(text_size > free_space){
//         text_size = free_space;
//     }
//     memcpy(buffer + buffer_size, event.text.text, text_size);
//     buffer_size += text_size;
//     buffer_cursor.x += text_size;

// }

void set_screen_dimensions(editor_t *editor , rendered_text_t *text)
{
    int w,h;
    SDL_Window *window = SDL_RenderGetWindow(text->font->renderer);
    SDL_GetWindowSize(window,&w,&h);
    editor->screen_rows     =  h / (font_char_height  * text->scale);
    editor->screen_columns  =  w / (font_char_width   * text->scale) - PAD;
}

// Binary search to current line
void update_current_line(editor_t *editor) 
{
    ssize_t row = editor->cursor.y;

    ssize_t high = editor->num_rows - 1;
    ssize_t low = 0;
    ssize_t mid;
    ssize_t item;
    ssize_t target = -1;
    

    if (editor->num_rows == 0){
        editor->current_line = 0;
        return;
    }

    if(row > editor->line[high].screen_row_end){
        editor->current_line = editor->num_rows-1;
        return;
    }
     
    if (editor->line[high].screen_row_end < row) {
        editor->current_line =  high;
        return;
    }

    while (low <= high) 
    {
        mid = (low + high) / 2;

        item = editor->line[mid].screen_row_end;

        if (item > row) {
            high = mid - 1;
        } else if (item < row) {
            target = mid;
            low = mid + 1;
        } else {
            editor->current_line = mid;
            return;
        }
    }
    editor->current_line = target+1;
}

ssize_t get_line_from_row(editor_t *editor, size_t row) 
{
    ssize_t high = editor->num_rows - 1;
    ssize_t low = 0;
    ssize_t mid;
    ssize_t item;
    ssize_t target = -1;
    

    if (editor->num_rows == 0){
        return 0;
    }

    if(row > editor->line[high].screen_row_end){
        return editor->num_rows-1;
    }
     
    if (editor->line[high].screen_row_end < row) {
        return high;
    }

    while (low <= high) 
    {
        mid = (low + high) / 2;

        item = editor->line[mid].screen_row_end;

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

bool snap_cursor(editor_t *editor)
{
    if(editor->cursor.y == editor->line[editor->current_line].screen_row_end){
        // snap cursor to the end of line.
        size_t len = editor->line[editor->current_line].size % editor->screen_columns;
        if(editor->cursor.x > len){
            editor->cursor.x = len;
            return true;
        }
    }
    return false;
}


void move_cursor_up(editor_t *editor)
{
    if (editor->cursor.y > 0){
        editor->cursor.y-=1;
        update_current_line(editor);
        snap_cursor(editor);
    }
}

void move_cursor_down(editor_t *editor)
{
    if(editor->cursor.y < editor->line[editor->num_rows-1].screen_row_end){
        editor->cursor.y+=1;
        update_current_line(editor);
        snap_cursor(editor);
    }
}
void move_cursor_right(editor_t *editor)
{
    // TODO: limt to screen instead
    if (editor->cursor.x < editor->screen_columns){
        editor->cursor.x+=1;
        if(snap_cursor(editor)){
            editor->cursor.x=0;
            editor->cursor.y++;
            update_current_line(editor);
        }
    }else{
        editor->cursor.x=0;
        editor->cursor.y++;
        update_current_line(editor);
    }
}

void move_cursor_left(editor_t *editor)
{
    if (editor->cursor.x > 0){
        editor->cursor.x-=1;
    }else {
        if(editor->cursor.y > 0){
            editor->cursor.y--;
            update_current_line(editor);
            editor->cursor.x = editor->screen_columns;
            snap_cursor(editor);
        }
    }
}

size_t get_index_in_line(editor_t *editor)
{
    size_t screen_row_start = (editor->current_line > 0) ? (editor->line[editor->current_line-1].screen_row_end+1) : 0;
    size_t line_row = editor->cursor.y - screen_row_start;

    return line_row * editor->screen_columns + editor->cursor.x;
}

void move_cursor_to_next_word(editor_t *editor)
{
    size_t index_in_line = get_index_in_line(editor);
    while(editor->line[editor->current_line].data[index_in_line++] != ' ')
    {
        if(index_in_line > editor->line[editor->current_line].size){
            break;
        }
        move_cursor_right(editor);
    }

}

void move_cursor_file_start(editor_t *editor)
{
    while(editor->cursor.y > 0){
        editor->cursor.y--;
        editor_scroll(editor);
    }
    editor->cursor.x = 0;
    update_current_line(editor);
}

void move_cursor_file_end(editor_t *editor)
{
    while(editor->cursor.y < editor->line[editor->num_rows-1].screen_row_end){
        editor->cursor.y++;
        editor_scroll(editor);
    }
    update_current_line(editor);
    snap_cursor(editor);
}

void move_cursor_page_up(editor_t *editor)
{
    size_t idx = (editor->row_offset > 0) ? editor->row_offset-1:0;
    size_t rows = (editor->line[idx].screen_row_end+1) + (editor->line[editor->row_offset].line_offset);
    while(editor->cursor.y > rows){
        move_cursor_up(editor);
    }
}

void move_cursor_page_down(editor_t *editor)
{
    size_t idx = (editor->row_offset > 0) ? editor->row_offset-1:0;
    size_t rows = (editor->line[idx].screen_row_end+1) + (editor->line[editor->row_offset].line_offset);
    while(editor->cursor.y < (rows + editor->screen_rows -1))
    {
        move_cursor_down(editor);
    }
}

void editor_zoom_in(editor_t *editor, rendered_text_t *text)
{
    if(text->scale < 15.0f){
        text->scale+=0.5f;
        rescale = true;
        set_screen_dimensions(editor,text);
        update_current_line(editor);
        snap_cursor(editor);
        if(editor->cursor.x > editor->screen_columns){
            editor->cursor.x = editor->screen_columns;
        }
    }
}

void editor_zoom_out(editor_t *editor, rendered_text_t *text)
{
    if(text->scale >= 1.5f){
        text->scale-=0.5f;
        rescale = true;
        set_screen_dimensions(editor,text);
        update_current_line(editor);
        snap_cursor(editor);
        if(editor->cursor.y > editor->line[editor->num_rows-1].screen_row_end){
            editor->cursor.y = editor->line[editor->num_rows-1].screen_row_end;
        }
    }
}

void poll_events(rendered_text_t *text, editor_t *editor)
{
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
                const size_t free_space = MAX_SIZE - (text->text->len);
                if(text_size > free_space){
                    text_size = free_space;
                }
                buf_append(text->text, event.text.text, text_size);
                (text->text->len) += text_size;
                editor->cursor.x += text_size;
                }break;

            case SDL_KEYDOWN:{
                switch(event.key.keysym.sym){
                    case SDLK_BACKSPACE:
                        if((text->text->len)>0){
                            (text->text->len)-=1;
                            if(editor->cursor.x == 0){
                                editor->cursor.y--;
                            }else{
                                editor->cursor.x -= 1;
                            }
                        }
                        break;
                    case SDLK_RETURN:
                        if(text->text->len < MAX_SIZE){
                            buf_append(text->text, "\n", 1);
                            editor->cursor.y+=1;
                            editor->cursor.x=0;
                        }
                        break;

                    case SDLK_LEFT:
                        move_cursor_left(editor);
                        break;

                    case SDLK_RIGHT:
                        move_cursor_right(editor);
                        break;
                    case SDLK_UP:
                        move_cursor_up(editor);
                        break;
                    case SDLK_DOWN:
                        move_cursor_down(editor);
                        break;
                    case SDLK_PAGEUP:{
                        move_cursor_page_up(editor);
                        break;
                    }
                    case SDLK_PAGEDOWN:{
                        move_cursor_page_down(editor);
                        break;
                    }
                    case SDLK_HOME:{
                        editor->cursor.x = 0;
                    }break;

                    case SDLK_END:{
                        editor->cursor.x = editor->screen_columns;
                        snap_cursor(editor);
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
                    editor_zoom_in(editor, text);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && ((keyboard_state_array[SDL_SCANCODE_KP_MINUS]) || (keyboard_state_array[SDL_SCANCODE_KP_MINUS])))
                {
                    editor_zoom_out(editor, text);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_HOME]))
                {
                    move_cursor_file_start(editor);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_END]))
                {
                    move_cursor_file_end(editor);
                }
                else if((keyboard_state_array[SDL_SCANCODE_LCTRL]) && (keyboard_state_array[SDL_SCANCODE_RIGHT]))
                {
                    move_cursor_to_next_word(editor);
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

void render_char(rendered_text_t *text)
{
    assert(text->ch >= ASCII_DISPLAY_LOW  || text->ch == '\n');
    assert(text->ch <= ASCII_DISPLAY_HIGH || text->ch == '\n');

    const size_t index = text->ch - ASCII_DISPLAY_LOW; // ascii to index

    const SDL_Rect dst_rect = {
        .x = (int) floorf((text->pos.x + PAD) * font_char_width * text->scale),
        .y = (int) floorf(text->pos.y * font_char_height * text->scale),
        .w = (int) floorf(font_char_width * text->scale),
        .h = (int) floorf(font_char_height * text->scale),
    };

    log_error(SDL_RenderCopy(text->font->renderer, text->font->font_texture, &(text->font->glyph_table[index]), &dst_rect));
}

void set_text_mode(rendered_text_t *text)
{
    log_error(SDL_SetTextureColorMod(text->font->font_texture, text->color.r, text->color.g, text->color.b));
    log_error(SDL_SetTextureAlphaMod(text->font->font_texture,text->color.a));
}

// size
void render_n_text(rendered_text_t *text,size_t text_size)
{
    set_text_mode(text);

    vec2f_t origin   = text->pos;
    vec2f_t next_pos = text->pos;

    for(size_t i = 0; i < text_size; i++)
    {
        text->ch = text->text->b[i];
        text->pos = next_pos;
        render_char(text);
        next_pos.x += font_char_width * text->scale;
        if(text->ch == '\n'){
            next_pos.y += font_char_height * text->scale;  
            next_pos.x  = 0;  
        }
    }
    text->pos = origin;
}

void render_n_string(editor_t *editor,rendered_text_t *text,char *string,size_t text_size)
{
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
        render_char(text);
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

void render_line_number(editor_t *editor, rendered_text_t *text)
{
    size_t max_width =0;
    size_t x = 0;
    size_t y = 0;

    size_t idx = (editor->row_offset > 0) ? editor->row_offset-1:0;
    size_t screen_start_row = (editor->line[idx].screen_row_end+1) + (editor->line[editor->row_offset].line_offset);
    
    for(size_t i = 0; i < editor->screen_rows; i++)
    {
        x= 0;
        y= i;
        size_t current_row = screen_start_row+i;
        size_t line = get_line_from_row(editor, current_row);
        if(line == editor->current_line){
            text->color.r = 132;
            text->color.g = 132;
            text->color.b = 132;
        }else{
            text->color.r = 66;
            text->color.g = 66;
            text->color.b = 66;
        }
        set_text_mode(text);
        size_t change = editor->line[line].screen_row_end-current_row;
        i+= change;
        
        char number_str[21]; 
        sprintf(number_str, "%llu", line);

        for (int j = 0; number_str[j] != '\0'; j++) 
        {
            size_t index = number_str[j] - ASCII_DISPLAY_LOW; // ascii to index

            const SDL_Rect dst_rect = {
                .x = (int) floorf(x++ * font_char_width * text->scale),
                .y = (int) floorf(y * font_char_height * text->scale),
                .w = (int) floorf(font_char_width * text->scale),
                .h = (int) floorf(font_char_height * text->scale),
            };
            log_error(SDL_RenderCopy(text->font->renderer, text->font->font_texture, &(text->font->glyph_table[index]), &dst_rect));
            max_width=max(max_width, j);
        }
    }
    text->color.r = 255;
    text->color.g = 255;
    text->color.b = 255;
    set_text_mode(text);
    PAD = max_width+2;
}


// TODO : this does too much, it handles word wrapping and computing each line row end 
// Find a way to seperate each functions
void render_n_text_file(editor_t *editor, rendered_text_t *text)
{   
    // If window is resized recompute screen columns and rows
    if(resize){
        set_screen_dimensions(editor, text);
        resize=false;
    }

    size_t rows = 0;
    text->pos.x = 0;
    text->pos.y = 0;

    // Iterate over each line
    for(size_t i = 0; i < editor->num_rows; i++)
    {
        // only render lines in screen 
        size_t file_row = i + editor->row_offset;

        // didnt offset outside of lines
        if(file_row < editor->num_rows)
        {
            size_t len = editor->line[file_row].size;

            // absolute startinng row of current line, its just the end of the previous line + 1
            size_t screen_row_start = (file_row > 0) ? (editor->line[file_row-1].screen_row_end+1) : (int)text->pos.y;

            // handle word wrapping and scroll
            // Wrap around if line length is larger than number of screen columns
            if(len > editor->screen_columns)
            {
                // if length of text is larger than screen columns 
                while(len > editor->screen_columns){
                    // start rendering from not only line offset but also offset within current wrapped line
                    if(editor->line[file_row].line_offset > rows){
                        rows++;
                        len -= editor->screen_columns;
                    }else{
                        len -= editor->screen_columns;
                        if (render)
                            render_n_string(editor, text,editor->line[file_row].data+editor->screen_columns*rows, editor->screen_columns);
                        // rendered a row, reset x position and increment y position
                        text->pos.x = 0;
                        text->pos.y++;
                        rows++;
                    }
                }
                // render remaining part of line
                if (render)
                    render_n_string(editor, text,editor->line[file_row].data+editor->screen_columns*rows,len);

                // absolute row of the end of the current line
                editor->line[file_row].screen_row_end = screen_row_start + rows;

                text->pos.x = 0;
                text->pos.y++;
                rows = 0;
            }else{
                // line fits entirely in screen no wrapping required.
                if (render)
                    render_n_string(editor, text,editor->line[file_row].data, len);
                editor->line[file_row].screen_row_end = screen_row_start + rows;
                text->pos.x = 0;
                text->pos.y++;
            }
            // when the entire screen is full of text just break, no need to go further.
            // TODO: fix to when a scale occurs , then we must iterate through rest of rows to calculate end row size
            // But we dont want to render text
            if (((text->pos.y > editor->screen_rows))){
                render = false;
                if(rescale == false){
                    render = true;
                    break;
                }
            }
        }
        if(file_row == editor->num_rows-1){
            rescale = false;
            render = true;
        }
    }
}

// null terminated
void render_text(rendered_text_t *text)
{
    const size_t n = strlen(text->text->b);
    render_n_text(text,n);
}

void render_cursor(editor_t *editor, rendered_text_t *text)
{
    size_t screen_row_start = (editor->row_offset > 0) ? (editor->line[editor->row_offset-1].screen_row_end + 1) : 0;

    assert(editor->cursor.y >= (screen_row_start + editor->line[editor->row_offset].line_offset));

    // cursor position on screen is cursor absolute position minus the offset
    size_t cursor_screen_pos = editor->cursor.y - (screen_row_start + editor->line[editor->row_offset].line_offset);

    SDL_Rect cursor = (SDL_Rect){
        .x = (int) floorf(font_char_width * text->scale *  (editor->cursor.x + PAD)),  // x is same as we do text wrapping
        .y = (int) floorf(font_char_height * text->scale * cursor_screen_pos),
        .w = (font_char_width * text->scale) / 4,
        .h = font_char_height * text->scale,
    };

    log_error(SDL_SetRenderDrawColor(text->font->renderer, 255,203,100,255));
    log_error(SDL_RenderFillRect(text->font->renderer, &cursor));
}

void init_font_image(font_t *font)
{
    SDL_Surface *font_surface = surface_from_image(font->path);

    font_char_height =  font_surface->h/FONT_ROWS;
    font_char_width  =  font_surface->w/FONT_COLS;
    // remove black background
    SDL_SetColorKey(font_surface, SDL_TRUE, SDL_MapRGB(font_surface->format, 0x00, 0x00, 0x00));

    font->font_texture = (SDL_Texture *)check_ptr(SDL_CreateTextureFromSurface(font->renderer, font_surface));

    // not needed anymore
    stbi_image_free(font_surface->pixels);
    
    SDL_FreeSurface(font_surface);

    // precompute glyph table
    for(size_t i = ASCII_DISPLAY_LOW; i <= ASCII_DISPLAY_HIGH; i++)
    {
        const size_t index  = i - ASCII_DISPLAY_LOW; // ascii to index
        const size_t col    = index % FONT_COLS;
        const size_t row    = index / FONT_COLS;

        font->glyph_table[index] = (SDL_Rect){
            .x = col * font_char_width,
            .y = row * font_char_height,
            .w = font_char_width,
            .h = font_char_height,
        };
    }
}

void init_font_ttf(font_t *font)
{

    TTF_Init();
    TTF_Font* ttf_font = (TTF_Font*) check_ptr(TTF_OpenFont(font->path, 115));
    const char *ascii_chars =   " !\"#$%&\'()*+,-./01\n"
                                "23456789:;<=>?@ABC\n"
                                "DEFGHIJKLMNOPQRSTU\n"
                                "VWXYZ[\\]^_`abcdefg\n"
                                "hijklmnopqrstuvwxy\n"
                                "z{|}~\n"
                                "pad";

    SDL_Surface* font_surface = TTF_RenderText_Solid_Wrapped(ttf_font, ascii_chars, COLOR_WHITE,0);
    font_char_height =  (font_surface->h/FONT_ROWS);
    font_char_width  =  (font_surface->w/FONT_COLS);
    font->font_texture = (SDL_Texture *)check_ptr(SDL_CreateTextureFromSurface(font->renderer, font_surface));

    SDL_FreeSurface(font_surface);
    TTF_CloseFont(ttf_font); 
    TTF_Quit();

    // precompute glyph table
    for(size_t i = ASCII_DISPLAY_LOW; i <= ASCII_DISPLAY_HIGH; i++)
    {
        const size_t index  = i - ASCII_DISPLAY_LOW; // ascii to index
        const size_t col    = index % FONT_COLS;
        const size_t row    = index / FONT_COLS;

        font->glyph_table[index] = (SDL_Rect){
            .x = col * font_char_width,
            .y = row * font_char_height,
            .w = font_char_width,
            .h = font_char_height,
        };
    }
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
    SDL_Window   *window    = NULL;
    SDL_Renderer *renderer  = NULL;

    init_sdl(&window, &renderer);

    abuf buffer =  ABUF_INIT();

    font_t font ={
        .renderer = renderer,
        .path = "./Font/charmap-oldschool_white.png",
        // .path = "./Font/SourceCodePro.ttf",
    };

    init_font_image(&font);
    // init_font_ttf(&font);

    rendered_text_t text = {
        .font = &font,
        .text = &buffer,
        .pos = {.x = 0.0f, .y = 0.0f},
        .color = COLOR_WHITE,
        .scale = 1.0f,
    };

    editor_t editor;
    SDL_Color color = {.r=33,.g=33,.b=33,.a=255};

    init_editor(&text,&editor);
    editor_open(&editor, "./test.txt");

    while(running)
    {
        poll_events(&text, &editor);
        log_error(SDL_SetRenderDrawColor(renderer,color.r,color.g,color.b,color.a));
        log_error(SDL_RenderClear(renderer));
        editor_scroll(&editor);
        render_n_text_file(&editor,&text);
        render_line_number(&editor,&text);
        // render_n_text(&text,text.text->len);
        render_cursor(&editor, &text);

        SDL_RenderPresent(renderer);
    }

    SDL_Quit();

    return 0;
}