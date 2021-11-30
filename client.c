#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "json_commands.h"
#include "y.tab.h"

const int EDIT_FIELD_LENGTH = 150;
const int EDIT_LINES = 10;
const int RESPONSE_LINES = 22;

const int CONTAINER_WIDTH = 1500;
const int CONTAINER_HEIGHT = 800;
const int EDIT_WIDTH = 1400;
const int EDIT_HEIGHT = 200;
const int RESPONSE_HEIGHT = 450;
const int RESPONSE_WIDTH = 1400;
const int SYSTEM_MESSAGE_X = 50;
const int SYSTEM_MESSAGE_Y = 540;

Display* display;
int screen;
Colormap colormap;
XColor grey, black;
XSetWindowAttributes attrs;
Atom wm_delete;
Window container_window;
Window edit_window;
Window response_window;
GC gc;
GC edit_gc;
GC response_gc;


bool gui_mode = true;
char response_text[1024][1024];
char system_message[1024]= "";
int response_number_of_lines;
int edit_number_of_lines = 0;
int edit_current_top_line = 0;
int edit_current_bottom_line = EDIT_LINES-1;

int response_current_top_line = 0;
int response_current_bottom_line = RESPONSE_LINES-1;

static const char *event_names[] = {
        "",
        "",
        "KeyPress",
        "KeyRelease",
        "ButtonPress",
        "ButtonRelease",
        "MotionNotify",
        "EnterNotify",
        "LeaveNotify",
        "FocusIn",
        "FocusOut",
        "KeymapNotify",
        "Expose",
        "GraphicsExpose",
        "NoExpose",
        "VisibilityNotify",
        "CreateNotify",
        "DestroyNotify",
        "UnmapNotify",
        "MapNotify",
        "MapRequest",
        "ReparentNotify",
        "ConfigureNotify",
        "ConfigureRequest",
        "GravityNotify",
        "ResizeRequest",
        "CirculateNotify",
        "CirculateRequest",
        "PropertyNotify",
        "SelectionClear",
        "SelectionRequest",
        "SelectionNotify",
        "ColormapNotify",
        "ClientMessage",
        "MappingNotify"
};

/*
void append(char subject[], const char insert[], int pos) {
    char buf[100] = {};
    strncpy(buf, subject, pos);
    int len = strlen(buf);
    strcpy(buf+len, insert);
    len += strlen(insert);
    strcpy(buf+len, subject+pos);
    strcpy(subject, buf);
}*/

Window create_window(Display* display, Window parent, int x, int y, unsigned width, unsigned height, unsigned border_width, unsigned border_color, unsigned background_color){
    // Create editWindow
    Window window = XCreateSimpleWindow(display, parent,
                                        x,
                                        y,
                                        width,
                                        height,
                                        border_width,
                                        border_color,
                                        background_color);
    // Select editWindow events

    // Make editWindow visible
    XMapWindow(display, window);
    return window;
}

GC create_gc(Window win, int reverse_video)
{
    GC gc;				/* handle of newly created GC.  */
    unsigned long valuemask = 0;		/* which values in 'values' to  */
    /* check when creating the GC.  */
    XGCValues values;			/* initial values for the GC.   */
    unsigned int line_width = 2;		/* line width for the GC.       */
    int line_style = LineSolid;		/* style for lines drawing and  */
    int cap_style = CapButt;		/* style of the line's edje and */
    int join_style = JoinBevel;		/*  joined lines.		*/
    int screen_num = DefaultScreen(display);

    gc = XCreateGC(display, win, valuemask, &values);
    if (gc < 0) {
        fprintf(stderr, "XCreateGC: \n");
    }

    /* allocate foreground and background colors for this GC. */
    if (reverse_video) {
        XSetForeground(display, gc, WhitePixel(display, screen_num));
        XSetBackground(display, gc, BlackPixel(display, screen_num));
    }
    else {
        XSetForeground(display, gc, BlackPixel(display, screen_num));
        XSetBackground(display, gc, WhitePixel(display, screen_num));
    }

    /* define the style of lines that will be drawn using this GC. */
    XSetLineAttributes(display, gc,
                       line_width, line_style, cap_style, join_style);

    /* define the fill style for the GC. to be 'solid filling'. */
    XSetFillStyle(display, gc, FillSolid);

    return gc;
}

void init_container_window(){
    long event_mask = ExposureMask
                      | KeyPressMask
                      | KeyReleaseMask
                      | ButtonPressMask
                      | ButtonReleaseMask
                      | FocusChangeMask;
    unsigned long attrs_mask = CWEventMask | NoEventMask | CWBackPixel;
    attrs.event_mask
            = SubstructureRedirectMask // handle child edit_window requests      (MapRequest)
              | SubstructureNotifyMask   // handle child edit_window notifications (DestroyNotify)
              | StructureNotifyMask      // handle container notifications    (ConfigureNotify)
              | ExposureMask             // handle container redraw           (Expose)
            ;
    attrs.do_not_propagate_mask = 0; // do not hide any events from child edit_window
    attrs.background_pixel = grey.pixel; // background color

    container_window = XCreateWindow(display, RootWindow(display, screen), 0, 0, CONTAINER_WIDTH, CONTAINER_HEIGHT, 1, CopyFromParent, InputOutput, CopyFromParent, attrs_mask, &attrs);
    XMapWindow(display, container_window);
    XStoreName(display, container_window, "XLIB container");

    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(display, container_window, &wm_delete, 1);

    gc = DefaultGC(display, screen);

    XSelectInput(display, container_window, event_mask);
}

void init_colors(){
    colormap = DefaultColormap(display, screen);
    XAllocNamedColor(display, colormap, "grey", &grey, &grey);
    XAllocNamedColor(display, colormap, "black", &black, &black);
}

void clear_response_window(){
    for (int i = 0; i < response_number_of_lines; i++)
        memset(response_text[i],0,strlen(response_text[i]));
    response_number_of_lines = 0;
    response_current_top_line = 0;
    response_current_bottom_line = 0;
}

void print_message_in_lines(const char *text, Window window) {
    int count = 1;
    for (int i = edit_current_top_line; i <= strlen(text) / EDIT_FIELD_LENGTH; i++) {
        char line[1024] = "";
        char* ret;
        memcpy(line, text + i * EDIT_FIELD_LENGTH, EDIT_FIELD_LENGTH);
        XDrawString(display, window, gc, 10, 20 * (count++), line, strlen(line));
    }
}

void print_response_array_in_lines() {

    int count = 1;
    for (int i = response_current_top_line; i <= response_number_of_lines; i++){
        XDrawString(display, response_window, gc, 10, 20 * (count++), response_text[i], strlen(response_text[i]));
    }
}

void render_edit_window() {
    int number_of_pages = edit_number_of_lines / EDIT_LINES + 1;
    int current_page = edit_current_bottom_line / EDIT_LINES;
    XSetForeground(display, edit_gc, grey.pixel);
    XDrawLine(display, edit_window, edit_gc, EDIT_WIDTH-10, 0, EDIT_WIDTH-10, EDIT_HEIGHT);
    XSetForeground(display, edit_gc, black.pixel);
    XFillRectangle(display, edit_window, edit_gc, EDIT_WIDTH-10, EDIT_HEIGHT/number_of_pages * current_page, 10, EDIT_HEIGHT/(number_of_pages));

    /*printf("number of lines %d \n", edit_number_of_lines);
    printf("current top line: %d \n", edit_current_top_line);
    printf("current bottom line: %d \n", edit_current_bottom_line);
    printf("number of pages: %d \n", number_of_pages);
    printf("current page: %d \n", current_page);
    printf("starting height: %d \n", EDIT_HEIGHT/number_of_pages * current_page);
    printf("height: %d \n", EDIT_HEIGHT/(number_of_pages));*/
}

void render_response_window() {
    print_response_array_in_lines();
    int number_of_pages = response_number_of_lines / RESPONSE_LINES + 1;
    int current_page = response_current_bottom_line / RESPONSE_LINES;
    printf("Current top line: %d\n", response_current_top_line);
    printf("Current bottom line: %d\n", response_current_bottom_line);
    printf("Current number_of_pages: %d\n", number_of_pages);
    printf("Current page: %d\n", current_page);
    XSetForeground(display, response_gc, grey.pixel);
    XDrawLine(display, response_window, response_gc, RESPONSE_WIDTH - 10, 0, RESPONSE_WIDTH - 10, RESPONSE_HEIGHT);
    XSetForeground(display, response_gc, black.pixel);
    XFillRectangle(display, response_window, response_gc, RESPONSE_WIDTH - 10, RESPONSE_HEIGHT / number_of_pages * current_page, 10, RESPONSE_HEIGHT / (number_of_pages));
}

void render_system_message(){
    printf("System message: %s", system_message);
    XDrawString(display, container_window, gc, SYSTEM_MESSAGE_X, SYSTEM_MESSAGE_Y, system_message, strlen(system_message));
}

void clear_system_message(){
    XClearWindow(display, container_window);
    memset(system_message,0,strlen(system_message));
}

void scan_string(const char * str);

static bool error_response(struct json_object * response) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("error", key) == 0) {
            printf("Error: %s.\n", json_object_get_string(val));
            if (gui_mode) {
                char *string;
                asprintf(&string, "Error: %s.", json_object_get_string(val));
                clear_system_message();
                strcpy(system_message, string);
            }
            return true;
        }
    }

    return false;
}

static struct json_object * get_success_response(struct json_object * response) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("success", key) == 0) {
            struct json_object * answer = json_object_get(val);
            json_object_put(response);
            return answer;
        }
    }

    printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
    return NULL;
}

static void print_response_with_amount(struct json_object * response, const char * action) {
    json_object_object_foreach(response, key, val) {
        if (strcmp("amount", key) == 0) {
            printf("%lu rows was %s.\n", json_object_get_uint64(val), action);
            if (gui_mode) {
                char *string;
                asprintf(&string, "%lu rows was %s.", json_object_get_uint64(val), action);
                clear_system_message();
                strcpy(system_message, string);
            }
            return;
        }
    }

    printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
}


static void print_separator(unsigned int columns_length, const int * columns_width) {
    for (int i = 0; i < columns_length; ++i) {
        putchar('+');
        if (gui_mode)
            strcat(response_text[response_number_of_lines], "+");

        for (int j = 0; j < columns_width[i] + 2; ++j) {
            putchar('-');
            if (gui_mode)
                strcat(response_text[response_number_of_lines], "-");
        }
    }

    puts("+");
    if (gui_mode)
        strcat(response_text[response_number_of_lines++], "+");
}

static void print_table(struct json_object * response) {
    struct json_object * columns = NULL;
    struct json_object * values = NULL;

    json_object_object_foreach(response, key, val) {
        if (strcmp("columns", key) == 0) {
            columns = val;
            continue;
        }

        if (strcmp("values", key) == 0) {
            values = val;
            continue;
        }
    }

    if (columns == NULL || values == NULL) {
        printf("Bad answer: %s\n", json_object_to_json_string_ext(response, JSON_C_TO_STRING_PRETTY));
        return;
    }

    clear_response_window();
    unsigned int rows_length = json_object_array_length(values);
    unsigned int columns_length = json_object_array_length(columns);
    int columns_width[columns_length];

    for (int i = 0; i < columns_length; ++i) {
        columns_width[i] = (int) strlen(json_object_get_string(json_object_array_get_idx(columns, i)));
    }

    for (int i = 0; i < rows_length; ++i) {
        struct json_object * row = json_object_array_get_idx(values, i);

        for (int j = 0; j < columns_length; ++j) {
            int width = (int) strlen(json_object_to_json_string(json_object_array_get_idx(row, j)));
            columns_width[j] = columns_width[j] > width ? columns_width[j] : width;
        }
    }

    print_separator(columns_length, columns_width);

    for (int i = 0; i < columns_length; ++i) {
        printf("| %*s ", columns_width[i], json_object_get_string(json_object_array_get_idx(columns, i)));
        if (gui_mode) {
            char *string;
            if (0 > asprintf(&string, "| %*s ", columns_width[i],
                             json_object_get_string(json_object_array_get_idx(columns, i)))) {
                printf("A problem occured.");
                exit(0);
            }
            strcat(response_text[response_number_of_lines], string);
        }
    }


    puts("|");

    if (gui_mode)
        strcat(response_text[response_number_of_lines++], "|");

    for (int i = 0; i < rows_length; ++i) {
        struct json_object * row = json_object_array_get_idx(values, i);

        print_separator(columns_length, columns_width);

        for (int j = 0; j < columns_length; ++j) {
            printf("| %*s ", columns_width[j], json_object_to_json_string(json_object_array_get_idx(row, j)));
            if (gui_mode) {
                char *string;
                if (0 > asprintf(&string, "| %*s ", columns_width[j],
                                 json_object_to_json_string(json_object_array_get_idx(row, j)))) {
                    printf("A problem occured.");
                    exit(0);
                }
                strcat(response_text[response_number_of_lines], string);
            }
        }

        puts("|");
        if (gui_mode)
            strcat(response_text[response_number_of_lines++], "|");
    }

    print_separator(columns_length, columns_width);
}

static void print_response(enum json_api_action action, struct json_object * response) {
    if (!response) {
        printf("Server didn't understand request.\n");
        if (gui_mode) {
            clear_system_message();
            strcpy(system_message, "Server didn't understand request.");
        }

        return;
    }

    if (error_response(response)) {
        return;
    }

    response = get_success_response(response);
    if (!response) {
        return;
    }

    switch (action) {
        case JSON_API_TYPE_CREATE_TABLE:
            printf("Table was created.\n");
            if (gui_mode) {
                clear_system_message();
                strcpy(system_message, "Table was created.");
            }
            break;

        case JSON_API_TYPE_DROP_TABLE:
            printf("Table was dropped.\n");
            if (gui_mode) {
                clear_system_message();
                strcpy(system_message, "Table was dropped.");
            }
            break;

        case JSON_API_TYPE_INSERT:
            printf("Inserted row.\n");
            if (gui_mode) {
                clear_system_message();
                strcpy(system_message, "Inserted row.");
            }
            break;

        case JSON_API_TYPE_DELETE:
            print_response_with_amount(response, "deleted");
            break;

        case JSON_API_TYPE_SELECT:
            print_table(response);
            break;

        case JSON_API_TYPE_UPDATE:
            print_response_with_amount(response, "updated");
            break;

        default:
            return;
    }
}

static bool process_request(int socket, struct json_object * request) {
    const char * request_string = json_object_to_json_string_ext(request, 0);

    ssize_t wrote;
    size_t remaining = strlen(request_string);
    while (remaining > 0) {
        wrote = write(socket, request_string, remaining);

        if (wrote < 0) {
            return false;
        }

        request_string += wrote;
        remaining -= wrote;
    }

    char buffer[64 * 1024];
    ssize_t was_read = read(socket, buffer, sizeof(buffer) / sizeof(*buffer));
    if (was_read <= 0) {
        return false;
    }

    if (was_read == sizeof(buffer) / sizeof(*buffer)) {
        buffer[sizeof(buffer) / sizeof(*buffer) - 1] = '\0';
    } else {
        buffer[was_read] = '\0';
    }

    enum json_tokener_error response_error;
    struct json_object * response = json_tokener_parse_verbose(buffer, &response_error);
    if (response_error == json_tokener_success) {
        print_response(json_api_get_action(request), response);
    } else {
        printf("Bad answer (%s): %s.\n", json_tokener_error_desc(response_error), buffer);
    }

    return true;
}

static bool process_command(int socket, const char * command) {
    struct json_object * request = NULL;
    char * error = NULL;

    scan_string(command);
    if (yyparse(&request, &error) != 0) {
        printf("Error during parsing: %s.\n", error);
        if (gui_mode) {
            char *string;
            asprintf(&string, "Error during parsing: %s.", error);
            clear_system_message();
            strcpy(system_message, string);
        }
        return true;
    }

    if (!request) {
        return true;
    }

    return process_request(socket, request);
}

void start_event_loop(char *input_text, char *key, int client_socket) {
    bool working = true;
    while(working) {
        XEvent event;
        XNextEvent(display, &event);
        printf("got event: %s\n", event_names[event.type]);

        // Keyboard
        if (event.type == KeyPress) {
            int len = XLookupString(&event.xkey, key, sizeof(key) - 1, 0, 0);
            key[len] = 0;
            printf( "KeyPress: %x\n", event.xkey.keycode );

            switch(event.xkey.keycode){
                //backspace
                case 0x16:
                    input_text[strlen(input_text) - 1] = '\0';
                    break;
                //enter
                case 0x24:
                    clear_system_message();
                    working = process_command(client_socket, input_text);
                    XClearWindow(display, response_window);
                    print_response_array_in_lines();
                    render_system_message();
                    render_response_window();
                    break;

                default:
                    strcat(input_text, key);
                    //strlen(input_text);
            }
        }

        //mouse buttons
        if (event.type == ButtonPress) {
            if (response_number_of_lines > RESPONSE_LINES) {
                XClearWindow(display, response_window);

                if (event.xbutton.button == Button4) {
                    printf("Up scroll was done\n");
                    if (response_current_top_line > 0)
                        response_current_top_line--;
                    if (response_current_bottom_line > 9)
                        response_current_bottom_line--;
                }
                if (event.xbutton.button == Button5) {
                    printf("Down scroll was done\n");
                    if (response_current_top_line < response_number_of_lines - RESPONSE_LINES)
                        response_current_top_line++;
                    if (response_current_bottom_line < response_number_of_lines)
                        response_current_bottom_line++;
                }

                //print_message_in_lines(response_text, response_window);
                print_response_array_in_lines();
                render_system_message();
                render_response_window();
            }
        }

        // Refresh
        if (event.type == KeyPress || event.type == Expose) {
            XClearWindow(display, edit_window);
            int previous_number_of_lines = edit_number_of_lines;

            print_message_in_lines(input_text, edit_window);

            edit_number_of_lines = strlen(input_text) / EDIT_FIELD_LENGTH;
            if (previous_number_of_lines < edit_number_of_lines){
                if (edit_number_of_lines > 9)
                    edit_current_top_line++;
                if (edit_current_bottom_line < edit_number_of_lines)
                    edit_current_bottom_line++;
            }

            if (previous_number_of_lines > edit_number_of_lines){
                if (edit_current_top_line > 0)
                    edit_current_top_line--;
                if (edit_current_bottom_line > 9)
                    edit_current_bottom_line--;
            }
            render_edit_window();
        }


        // Close button
        if (event.type == ClientMessage) {
            if (event.xclient.data.l[0] == wm_delete) {
                break;
            }
        }
    }
}

int main(int argc, char** argv) {

    if (argc < 2){
        printf("No arguments passed. Specify 'cli' or 'gui' as an argument.\n");
        exit(1);
    }

    if (strcmp(argv[1], "gui") != 0 && strcmp(argv[1], "cli") !=0 ){
        printf("Incorrect argument. Specify 'cli' or 'gui' as an argument.\n");
        exit(1);
    }
    // create a socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // specify an address for the socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(9002);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // check for error with the connection
    if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) != 0) {
        perror("There was an error making a connection to the remote socket");
        return -1;
    }
    if (strcmp(argv[1], "gui") == 0){
        XInitThreads();
        display = XOpenDisplay(NULL);
        if (display == NULL) {
            fprintf(stderr, "Error: can't open display\n");
            exit(1);
        }
        screen = DefaultScreen(display);

        init_colors();
        init_container_window();

        //create edit window and graphic context
        edit_window = create_window(display, container_window, 45, 550, EDIT_WIDTH, EDIT_HEIGHT, 2, BlackPixel(display, screen), WhitePixel(display, screen));
        edit_gc = create_gc(edit_window, 0);

        //create response window and graphic context
        response_window = create_window(display, container_window, 45, 50, RESPONSE_WIDTH, RESPONSE_HEIGHT, 2, BlackPixel(display, screen), WhitePixel(display, screen));
        response_gc = create_gc(response_window, 0);
        XSync(display, False);

        char input_text[1024] = "";
        char key[32];
        start_event_loop(input_text, key, client_socket);

        XCloseDisplay(display);
    }

    else{
        gui_mode = false;
        bool working = true;
        while (working) {
            size_t command_capacity = 0;
            char * command = NULL;

            printf("> ");
            fflush(stdout);

            ssize_t was_read = getline(&command, &command_capacity, stdin);
            if (was_read <= 0) {
                free(command);
                break;
            }

            command[was_read] = '\0';
            working = process_command(client_socket, command);
        }
    }
    close(client_socket);
    printf("Shutting down.\n");
    return 0;
}
