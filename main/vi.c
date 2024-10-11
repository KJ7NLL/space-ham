//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Library General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// 
//  Copyright (C) 2022- by Ezekiel Wheeler, KJ7NLL and Eric Wheeler, KJ7LNW.
//  All rights reserved.
//
//  The official website and doumentation for space-ham is available here:
//    https://www.kj7nll.radio/

// This is a very hacked up AI generated vi clone.

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "ff.h"
#include "fatfs-util.h"

// Define buffer size and other constants
#define BUFFER_SIZE (1024 * 1)  // 1 KB buffer
#define BOTTOM_ROW 24            // Adjust if your terminal has a different number of rows

// Define escape sequences
#define ESC '\x1b'
#define CSI '['
#define SS3 'O'
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_HOME "\x1b[H"
#define ERASE_LINE "\x1b[2K"

// Define key codes
enum KeyCode {
    KEY_NONE,
    KEY_ESC,
    KEY_CTRL_C,
    KEY_ARROW_UP,
    KEY_ARROW_DOWN,
    KEY_ARROW_RIGHT,
    KEY_ARROW_LEFT,
    KEY_HOME,
    KEY_END,
    KEY_INSERT,
    KEY_DELETE,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_BACKSPACE,
    KEY_UNKNOWN,
};

// Data structure for escape codes
typedef struct {
    char seq[8];
    enum KeyCode code;
} EscapeSequence;

// Command state for finite state automata
typedef enum {
    STATE_NORMAL,
    STATE_OPERATOR_PENDING,
} State;

typedef struct {
    State state;
    int operator;
} CommandState;

// Struct to store the last command for repetition
typedef struct {
    int command;  // The command key (e.g., 'd', 'y', 'p', 'x', 'O', 'P')
    int motion;   // The motion key if applicable
} LastCommand;

// Static global variables
static EscapeSequence escape_sequences[] = {
    { "[A", KEY_ARROW_UP },
    { "[B", KEY_ARROW_DOWN },
    { "[C", KEY_ARROW_RIGHT },
    { "[D", KEY_ARROW_LEFT },
    { "[H", KEY_HOME },
    { "[F", KEY_END },
    { "[1~", KEY_HOME },
    { "[4~", KEY_END },
    { "[2~", KEY_INSERT },
    { "[3~", KEY_DELETE },
    { "[5~", KEY_PAGE_UP },
    { "[6~", KEY_PAGE_DOWN },
    { "OA", KEY_ARROW_UP },
    { "OB", KEY_ARROW_DOWN },
    { "OC", KEY_ARROW_RIGHT },
    { "OD", KEY_ARROW_LEFT },
    { "OH", KEY_HOME },
    { "OF", KEY_END },
    { "", KEY_NONE },
};

static char *buffer = NULL;                // Main text buffer
static char *yank_buffer = NULL;           // Yank buffer for 'y' and 'p' commands
static int yank_len = 0;                   // Length of yanked text
static LastCommand last_command = {0, 0};  // Last command for '.'

static int serial_read_char();
static int get_current_time_ms();
static int read_key();
static void clear_screen();
static void move_cursor(int row, int col);
static int get_buffer_pos(char *buffer, int cursor_row, int cursor_col);
static int get_line_length(char *buffer, int target_row);
static int get_total_lines(char *buffer);
static void refresh_screen(char *buffer);
static void update_cursor_position(int cursor_row, int cursor_col);
static void update_status_line(const char *status);
static void parse_motion(char *buffer, int buffer_len, int cursor_row, int cursor_col, int motion_key, int *target_row, int *target_col, int *end_pos);
static void delete_motion(char *buffer, int *buffer_len, int *cursor_row, int *cursor_col, int motion_key);
static void yank_motion(char *buffer, int buffer_len, int cursor_row, int cursor_col, int motion_key);
static void delete_current_line(char *buffer, int *buffer_len, int *cursor_row, int *cursor_col);
static void yank_current_line(char *buffer, int buffer_len, int cursor_row);
static void delete_char_at_cursor(char *buffer, int *buffer_len, int cursor_row, int cursor_col);
void vi(char *filename);

// Helper function to check if input is a known prefix
static int is_known_prefix(const char *seq) {
    for (int j = 0; escape_sequences[j].code != KEY_NONE; j++) {
        if (strncmp(escape_sequences[j].seq, seq, strlen(seq)) == 0) {
            return 1;
        }
    }
    return 0;
}

// Function to get current time in milliseconds
static int get_current_time_ms() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// Function to read a key with escape sequence processing
static int read_key() {
    static char input_buffer[16];
    int input_len = 0;
    int c;
    int timeout = 100; // Timeout in milliseconds

    c = serial_read_char();
    if (c == -1) {
        return KEY_NONE;
    }

    if (c != ESC) {
        if (c == 127 || c == '\b') {
            return KEY_BACKSPACE;
        }
        return c;
    }

    // Start building the escape sequence
    input_buffer[input_len++] = c;
    input_buffer[input_len] = '\0';

    // Implement a timeout mechanism
    int start_time = get_current_time_ms();

    while (input_len < sizeof(input_buffer) - 1) {
        c = serial_read_char();
        if (c == -1) {
            if (get_current_time_ms() - start_time >= timeout) {
                break; // Timeout
            }
            continue;
        }
        input_buffer[input_len++] = c;
        input_buffer[input_len] = '\0';

        // Check if the sequence matches any known sequences
        for (int j = 0; escape_sequences[j].code != KEY_NONE; j++) {
            if (strcmp(&input_buffer[1], escape_sequences[j].seq) == 0) {
                return escape_sequences[j].code;
            }
        }

        // If current input_buffer matches any known prefixes
        if (!is_known_prefix(&input_buffer[1])) {
            break;
        }
    }

    return KEY_ESC;
}

// Function to clear the screen
static void clear_screen() {
    printf("%s%s", CLEAR_SCREEN, CURSOR_HOME);
    fflush(stdout);
}

// Function to move the cursor
static void move_cursor(int row, int col) {
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    printf("\x1b[%d;%dH", row, col);
    fflush(stdout);
}

// Function to get buffer position from cursor position
static int get_buffer_pos(char *buffer, int cursor_row, int cursor_col) {
    int pos = 0;
    int row = 1, col = 1;
    while (buffer[pos] != '\0') {
        if (row == cursor_row && col == cursor_col) {
            break;
        }
        if (buffer[pos] == '\n') {
            row++;
            col = 1;
        } else {
            col++;
        }
        pos++;
    }
    return pos;
}

// Function to get the length of a specific line
static int get_line_length(char *buffer, int target_row) {
    int pos = 0;
    int row = 1;
    int length = 0;
    while (buffer[pos] != '\0') {
        if (row == target_row) {
            if (buffer[pos] == '\n') {
                break;
            }
            length++;
        }
        if (buffer[pos] == '\n') {
            row++;
            if (row > target_row) {
                break;
            }
        }
        pos++;
    }
    return length > 0 ? length : 1;
}

// Function to get the total number of lines in the buffer
static int get_total_lines(char *buffer) {
    int pos = 0;
    int lines = 1;
    while (buffer[pos] != '\0') {
        if (buffer[pos] == '\n') {
            lines++;
        }
        pos++;
    }
    return lines;
}

// Function to refresh the screen
static void refresh_screen(char *buffer) {
    clear_screen();
    // Print buffer contents
    int row = 1;
    int col = 1;
    int i = 0;
    char c;
    while ((c = buffer[i]) != '\0') {
        move_cursor(row, col);
        if (c == '\n') {
            row++;
            col = 1;
        } else {
            putchar(c);
            col++;
        }
        i++;
    }
    fflush(stdout);
}

// Function to update the cursor position
static void update_cursor_position(int cursor_row, int cursor_col) {
    move_cursor(cursor_row, cursor_col);
    fflush(stdout);
}

// Function to update the status line
static void update_status_line(const char *status) {
    move_cursor(BOTTOM_ROW, 1);
    printf("%s%s", ERASE_LINE, status);
    fflush(stdout);
}

// Function to parse motion commands
static void parse_motion(char *buffer, int buffer_len, int cursor_row, int cursor_col, int motion_key, int *target_row, int *target_col, int *end_pos) {
    int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
    *target_row = cursor_row;
    *target_col = cursor_col;
    int line_length;

    if (motion_key == 'w') {
        // Move forward one word
        while (buffer[pos] != '\0' && isspace((unsigned char)buffer[pos])) {
            if (buffer[pos] == '\n') {
                (*target_row)++;
                *target_col = 1;
            } else {
                (*target_col)++;
            }
            pos++;
        }
        while (buffer[pos] != '\0' && !isspace((unsigned char)buffer[pos])) {
            if (buffer[pos] == '\n') {
                (*target_row)++;
                *target_col = 1;
            } else {
                (*target_col)++;
            }
            pos++;
        }
    } else if (motion_key == '$') {
        // Move to end of line
        line_length = get_line_length(buffer, *target_row);
        *target_col = line_length;
        pos = get_buffer_pos(buffer, *target_row, *target_col);
    } else if (motion_key == 'h') {
        // Move left
        if (*target_col > 1) {
            (*target_col)--;
            pos--;
        }
    } else if (motion_key == 'l') {
        // Move right
        line_length = get_line_length(buffer, *target_row);
        if (*target_col < line_length) {
            (*target_col)++;
            pos++;
        }
    } else if (motion_key == '0') {
        // Move to beginning of line
        *target_col = 1;
        pos = get_buffer_pos(buffer, *target_row, *target_col);
    } else {
        // Unsupported motion
        return;
    }

    *end_pos = pos;
}

// Function to delete text based on motion
static void delete_motion(char *buffer, int *buffer_len, int *cursor_row, int *cursor_col, int motion_key) {
    int start_pos = get_buffer_pos(buffer, *cursor_row, *cursor_col);
    int end_pos;
    int target_row, target_col;

    parse_motion(buffer, *buffer_len, *cursor_row, *cursor_col, motion_key, &target_row, &target_col, &end_pos);

    // Ensure start_pos and end_pos are in correct order
    if (end_pos < start_pos) {
        int temp = start_pos;
        start_pos = end_pos;
        end_pos = temp;
    }

    // Delete text between start_pos and end_pos
    memmove(&buffer[start_pos], &buffer[end_pos], *buffer_len - end_pos + 1);
    *buffer_len -= (end_pos - start_pos);

    // Update cursor position
    *cursor_row = target_row;
    *cursor_col = target_col;
}

// Function to yank text based on motion
static void yank_motion(char *buffer, int buffer_len, int cursor_row, int cursor_col, int motion_key) {
    int start_pos = get_buffer_pos(buffer, cursor_row, cursor_col);
    int end_pos;
    int target_row, target_col;

    parse_motion(buffer, buffer_len, cursor_row, cursor_col, motion_key, &target_row, &target_col, &end_pos);

    // Ensure start_pos and end_pos are in correct order
    if (end_pos < start_pos) {
        int temp = start_pos;
        start_pos = end_pos;
        end_pos = temp;
    }

    // Yank text between start_pos and end_pos
    yank_len = end_pos - start_pos;
    memcpy(yank_buffer, &buffer[start_pos], yank_len);
}

// Function to delete the current line
static void delete_current_line(char *buffer, int *buffer_len, int *cursor_row, int *cursor_col) {
    int start_pos = get_buffer_pos(buffer, *cursor_row, 1);
    int end_pos = start_pos;
    while (buffer[end_pos] != '\n' && buffer[end_pos] != '\0') {
        end_pos++;
    }
    if (buffer[end_pos] == '\n') end_pos++;
    memmove(&buffer[start_pos], &buffer[end_pos], *buffer_len - end_pos + 1);
    *buffer_len -= (end_pos - start_pos);
    // Adjust cursor position
    int total_lines = get_total_lines(buffer);
    if (*cursor_row > total_lines) {
        (*cursor_row) = total_lines;
    }
    *cursor_col = 1;
}

// Function to yank the current line
static void yank_current_line(char *buffer, int buffer_len, int cursor_row) {
    int start_pos = get_buffer_pos(buffer, cursor_row, 1);
    int end_pos = start_pos;
    while (buffer[end_pos] != '\n' && buffer[end_pos] != '\0') {
        end_pos++;
    }
    if (buffer[end_pos] == '\n') {
        end_pos++;
    }
    yank_len = end_pos - start_pos;
    memcpy(yank_buffer, &buffer[start_pos], yank_len);
}

// Function to delete character at cursor
static void delete_char_at_cursor(char *buffer, int *buffer_len, int cursor_row, int cursor_col) {
    int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
    if (buffer[pos] == '\0') return;
    memmove(&buffer[pos], &buffer[pos + 1], *buffer_len - pos);
    (*buffer_len)--;
}

// The vi editor function
void vi(char *filename) {
    FRESULT fr;
    FIL file;
    UINT br, bw;

    // Allocate the buffers dynamically
    buffer = (char *)malloc(BUFFER_SIZE);
    yank_buffer = (char *)malloc(BUFFER_SIZE);

    if (buffer == NULL || yank_buffer == NULL) {
        printf("Failed to allocate buffers\r\n");
        return;
    }

    int cursor_row = 1;
    int cursor_col = 1;

    fr = f_open(&file, filename, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (fr != FR_OK) {
        printf("Failed to open file (%d)\r\n", fr);
        free(buffer);
        free(yank_buffer);
        return;
    }

    fr = f_read(&file, buffer, BUFFER_SIZE - 1, &br);
    if (fr != FR_OK) {
        printf("Failed to read file (%d)\r\n", fr);
        f_close(&file);
        free(buffer);
        free(yank_buffer);
        return;
    }
    buffer[br] = '\0';

    f_close(&file);

    int buffer_len = strlen(buffer);
    int insert_mode = 0;

    cursor_row = 1;  // Move cursor to the top-left corner when the file is first opened
    cursor_col = 1;

    refresh_screen(buffer);
    update_cursor_position(cursor_row, cursor_col);

    CommandState cmd_state = { STATE_NORMAL, 0 };

    // Display initial status line
    if (insert_mode) {
        update_status_line("-- INSERT --");
    } else {
        update_status_line("");
    }

    while (1) {
        int key = read_key();

        if (key == KEY_NONE) {
            vTaskDelay(100 / portTICK_PERIOD_MS);  // Add 100ms delay if no input is provided
            continue;
        }

        int need_refresh = 0;

        if (insert_mode) {
            if (key == KEY_ESC) {
                insert_mode = 0;
                update_status_line("");
                int line_length = get_line_length(buffer, cursor_row) + 1;
                if (cursor_col > line_length) {
                    cursor_col = line_length - 1;
                }
            }
            // Handle special keys in insert mode
            else if (key == KEY_BACKSPACE || key == KEY_DELETE) {
                // Handle backspace in insert mode
                if (buffer_len > 0) {
                    int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                    if (pos > 0) {
                        if (cursor_col > 1) {
                            // Normal backspace within a line
                            memmove(&buffer[pos - 1], &buffer[pos], buffer_len - pos + 1);
                            buffer_len--;
                            cursor_col--;
                        } else {
                            // Backspace at the start of a line
                            int prev_line_length = get_line_length(buffer, cursor_row - 1);
                            delete_char_at_cursor(buffer, &buffer_len, cursor_row, cursor_col);
                            cursor_row--;
                            cursor_col = prev_line_length;
                        }
                        need_refresh = 1;
                    }
                }
            } else if (key == KEY_ARROW_UP) {
                if (cursor_row > 1) {
                    cursor_row--;
                    int line_length = get_line_length(buffer, cursor_row);
                    if (cursor_col > line_length) {
                        cursor_col = line_length;
                    }
                }
            } else if (key == KEY_ARROW_DOWN) {
                int total_lines = get_total_lines(buffer);
                if (cursor_row < total_lines) {
                    cursor_row++;
                    int line_length = get_line_length(buffer, cursor_row);
                    if (cursor_col > line_length) {
                        cursor_col = line_length;
                    }
                }
            } else if (key == KEY_ARROW_RIGHT) {
                int line_length = get_line_length(buffer, cursor_row);
                if (cursor_col < line_length) {
                    cursor_col++;
                }
            } else if (key == KEY_ARROW_LEFT) {
                if (cursor_col > 1) {
                    cursor_col--;
                }
            } else if (key == KEY_HOME) {
                cursor_col = 1;
            } else if (key == KEY_END) {
                cursor_col = get_line_length(buffer, cursor_row);
            }
            // Any other key is inserted into the buffer
            else {
                if (buffer_len < BUFFER_SIZE - 1) {
                    int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                    memmove(&buffer[pos + 1], &buffer[pos], buffer_len - pos + 1);
                    buffer[pos] = (char)key;
                    buffer_len++;
                    cursor_col++;
                    need_refresh = 1;
                }
            }
        } else {
            // Normal mode handling
            if (cmd_state.state == STATE_NORMAL) {
                // Process normal mode keys
                if (key == 'i') {
                    insert_mode = 1;
                    update_status_line("-- INSERT --");
                } else if (key == 'a') {
                    int line_length = get_line_length(buffer, cursor_row) + 1;
                    if (cursor_col < line_length) {
                        cursor_col++;
                    }
                    insert_mode = 1;
                    update_status_line("-- INSERT --");
                } else if (key == 'o') {
                    // Open a new line below and enter insert mode
                    int pos = get_buffer_pos(buffer, cursor_row, get_line_length(buffer, cursor_row) + 1);
                    if (buffer_len < BUFFER_SIZE - 1) {
                        memmove(&buffer[pos + 1], &buffer[pos], buffer_len - pos + 1);
                        buffer[pos] = '\n';
                        buffer_len++;
                        cursor_row++;
                        cursor_col = 1;
                        insert_mode = 1;
                        update_status_line("-- INSERT --");
                        need_refresh = 1;
                    }
                } else if (key == 'O') {
                    // Open a new line above and enter insert mode
                    int pos = get_buffer_pos(buffer, cursor_row, 1);
                    if (buffer_len < BUFFER_SIZE - 1) {
                        memmove(&buffer[pos + 1], &buffer[pos], buffer_len - pos + 1);
                        buffer[pos] = '\n';
                        buffer_len++;
                        cursor_col = 1;
                        insert_mode = 1;
                        update_status_line("-- INSERT --");
                        need_refresh = 1;
                        // Move to the new line
                        cursor_row++;
                        // Swap the new line with the current line
                        int temp_pos = pos;
                        while (buffer[temp_pos] != '\n' && temp_pos > 0) {
                            char temp = buffer[temp_pos];
                            buffer[temp_pos] = buffer[temp_pos - 1];
                            buffer[temp_pos - 1] = temp;
                            temp_pos--;
                        }
                        cursor_row--;
                    }
                } else if (key == 'd' || key == 'y') {
                    cmd_state.state = STATE_OPERATOR_PENDING;
                    cmd_state.operator = key;
                } else if (key == 'p') {
                    if (yank_len > 0 && buffer_len + yank_len < BUFFER_SIZE) {
                        int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                        memmove(&buffer[pos + yank_len], &buffer[pos], buffer_len - pos + 1);
                        memcpy(&buffer[pos], yank_buffer, yank_len);
                        buffer_len += yank_len;

                        // Adjust cursor position
                        int newlines = 0;
                        for (int i = 0; i < yank_len; i++) {
                            if (yank_buffer[i] == '\n') {
                                newlines++;
                            }
                        }
                        cursor_row += newlines;
                        need_refresh = 1;
                    }
                    // Update last command
                    last_command.command = 'p';
                    last_command.motion = 0;
                } else if (key == 'P') {
                    if (yank_len > 0 && buffer_len + yank_len < BUFFER_SIZE) {
                        int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                        memmove(&buffer[pos + yank_len], &buffer[pos], buffer_len - pos + 1);
                        memcpy(&buffer[pos], yank_buffer, yank_len);
                        buffer_len += yank_len;
                        need_refresh = 1;
                    }
                    // Update last command
                    last_command.command = 'P';
                    last_command.motion = 0;
                } else if (key == '.') {
                    // Repeat last command
                    if (last_command.command == 'd') {
                        if (last_command.motion == 0) {
                            // 'dd'
                            delete_current_line(buffer, &buffer_len, &cursor_row, &cursor_col);
                        } else {
                            delete_motion(buffer, &buffer_len, &cursor_row, &cursor_col, last_command.motion);
                        }
                        need_refresh = 1;
                    } else if (last_command.command == 'y') {
                        if (last_command.motion == 0) {
                            // 'yy'
                            yank_current_line(buffer, buffer_len, cursor_row);
                        } else {
                            yank_motion(buffer, buffer_len, cursor_row, cursor_col, last_command.motion);
                        }
                    } else if (last_command.command == 'p') {
                        // Repeat 'p'
                        if (yank_len > 0 && buffer_len + yank_len < BUFFER_SIZE) {
                            int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                            memmove(&buffer[pos + yank_len], &buffer[pos], buffer_len - pos + 1);
                            memcpy(&buffer[pos], yank_buffer, yank_len);
                            buffer_len += yank_len;

                            // Adjust cursor position
                            int newlines = 0;
                            for (int i = 0; i < yank_len; i++) {
                                if (yank_buffer[i] == '\n') {
                                    newlines++;
                                }
                            }
                            cursor_row += newlines;
                            need_refresh = 1;
                        }
                    } else if (last_command.command == 'x') {
                        delete_char_at_cursor(buffer, &buffer_len, cursor_row, cursor_col);
                        need_refresh = 1;
                    } else if (last_command.command == 'O') {
                        // Repeat 'O' command
                        // Similar to pressing 'O' again
                        int pos = get_buffer_pos(buffer, cursor_row, 1);
                        if (buffer_len < BUFFER_SIZE - 1) {
                            memmove(&buffer[pos + 1], &buffer[pos], buffer_len - pos + 1);
                            buffer[pos] = '\n';
                            buffer_len++;
                            cursor_col = 1;
                            insert_mode = 1;
                            update_status_line("-- INSERT --");
                            need_refresh = 1;
                            cursor_row++;
                            int temp_pos = pos;
                            while (buffer[temp_pos] != '\n' && temp_pos > 0) {
                                char temp = buffer[temp_pos];
                                buffer[temp_pos] = buffer[temp_pos - 1];
                                buffer[temp_pos - 1] = temp;
                                temp_pos--;
                            }
                            cursor_row--;
                        }
                    } else if (last_command.command == 'P') {
                        // Repeat 'P' command
                        if (yank_len > 0 && buffer_len + yank_len < BUFFER_SIZE) {
                            int pos = get_buffer_pos(buffer, cursor_row, cursor_col);
                            memmove(&buffer[pos + yank_len], &buffer[pos], buffer_len - pos + 1);
                            memcpy(&buffer[pos], yank_buffer, yank_len);
                            buffer_len += yank_len;
                            need_refresh = 1;
                        }
                    }
                } else if (key == ':') {
                    // Handle command line input
                    int saved_cursor_row = cursor_row;
                    int saved_cursor_col = cursor_col;

                    move_cursor(BOTTOM_ROW, 1);
                    printf(":");
                    fflush(stdout);

                    char command[256];
                    int cmd_len = 0;
                    while (1) {
                        key = read_key();
                        if (key == KEY_NONE) {
                            continue;
                        }
                        if (key == '\r' || key == '\n') {
                            command[cmd_len] = '\0';
                            break;
                        } else if (key == KEY_BACKSPACE) {
                            if (cmd_len > 0) {
                                cmd_len--;
                                printf("\b \b");
                                fflush(stdout);
                            }
                        } else if (key >= 32 && key <= 126) {
                            if (cmd_len < sizeof(command) - 1) {
                                command[cmd_len++] = key;
                                putchar(key);
                                fflush(stdout);
                            }
                        }
                    }

                    move_cursor(BOTTOM_ROW, 1);
                    printf("%s", ERASE_LINE);
                    fflush(stdout);

                    cursor_row = saved_cursor_row;
                    cursor_col = saved_cursor_col;

                    if (strcmp(command, "w") == 0) {
                        fr = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
                        if (fr != FR_OK) {
                            printf("Failed to open file for writing (%d)\r\n", fr);
                        } else {
                            fr = f_write(&file, buffer, buffer_len, &bw);
                            if (fr != FR_OK || bw < buffer_len) {
                                printf("Failed to write file (%d)\r\n", fr);
                            } else {
                                move_cursor(BOTTOM_ROW, 1);
                                printf("File saved.");
                                fflush(stdout);
                            }
                            f_close(&file);
                        }
                    } else if (strcmp(command, "q") == 0) {
                        break;
                    } else if (strcmp(command, "wq") == 0) {
                        fr = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
                        if (fr != FR_OK) {
                            printf("Failed to open file for writing (%d)\r\n", fr);
                        } else {
                            fr = f_write(&file, buffer, buffer_len, &bw);
                            if (fr != FR_OK || bw < buffer_len) {
                                printf("Failed to write file (%d)\r\n", fr);
                            } else {
                                move_cursor(BOTTOM_ROW, 1);
                                printf("File saved.");
                                fflush(stdout);
                            }
                            f_close(&file);
                        }
                        break;
                    }
                } else if (key == KEY_ARROW_UP || key == 'k') {
                    if (cursor_row > 1) {
                        cursor_row--;
                        int line_length = get_line_length(buffer, cursor_row);
                        if (cursor_col > line_length) {
                            cursor_col = line_length;
                        }
                        update_cursor_position(cursor_row, cursor_col);
                    }
                } else if (key == KEY_ARROW_DOWN || key == 'j') {
                    int total_lines = get_total_lines(buffer);
                    if (cursor_row < total_lines) {
                        cursor_row++;
                        int line_length = get_line_length(buffer, cursor_row);
                        if (cursor_col > line_length) {
                            cursor_col = line_length;
                        }
                        update_cursor_position(cursor_row, cursor_col);
                    }
                } else if (key == KEY_ARROW_RIGHT || key == 'l') {
                    int line_length = get_line_length(buffer, cursor_row);
                    if (cursor_col < line_length) {
                        cursor_col++;
                        update_cursor_position(cursor_row, cursor_col);
                    }
                } else if (key == KEY_ARROW_LEFT || key == 'h') {
                    if (cursor_col > 1) {
                        cursor_col--;
                        update_cursor_position(cursor_row, cursor_col);
                    }
                } else if (key == KEY_HOME || key == '0') {
                    cursor_col = 1;
                    update_cursor_position(cursor_row, cursor_col);
                } else if (key == KEY_END || key == '$') {
                    cursor_col = get_line_length(buffer, cursor_row);
                    update_cursor_position(cursor_row, cursor_col);
                } else if (key == 'x') {
                    delete_char_at_cursor(buffer, &buffer_len, cursor_row, cursor_col);
                    need_refresh = 1;
                    // Update last command
                    last_command.command = 'x';
                    last_command.motion = 0;
                }
            } else if (cmd_state.state == STATE_OPERATOR_PENDING) {
                // Handle operator pending state
                if (key == 'd' || key == 'y') {
                    if (key == cmd_state.operator) {
                        // Handle 'dd' or 'yy'
                        if (cmd_state.operator == 'd') {
                            delete_current_line(buffer, &buffer_len, &cursor_row, &cursor_col);
                            last_command.command = 'd';
                            last_command.motion = 0;
                        } else if (cmd_state.operator == 'y') {
                            yank_current_line(buffer, buffer_len, cursor_row);
                            last_command.command = 'y';
                            last_command.motion = 0;
                        }
                        need_refresh = 1;
                        cmd_state.state = STATE_NORMAL;
                        cmd_state.operator = 0;
                    } else {
                        // Invalid sequence
                        cmd_state.state = STATE_NORMAL;
                        cmd_state.operator = 0;
                    }
                } else if (key == 'w' || key == '$' || key == 'h' || key == 'l' || key == '0') {
                    // Handle motions
                    if (cmd_state.operator == 'd') {
                        delete_motion(buffer, &buffer_len, &cursor_row, &cursor_col, key);
                        last_command.command = 'd';
                        last_command.motion = key;
                    } else if (cmd_state.operator == 'y') {
                        yank_motion(buffer, buffer_len, cursor_row, cursor_col, key);
                        last_command.command = 'y';
                        last_command.motion = key;
                    }
                    need_refresh = 1;
                    cmd_state.state = STATE_NORMAL;
                    cmd_state.operator = 0;
                } else {
                    // Invalid key, reset state
                    cmd_state.state = STATE_NORMAL;
                    cmd_state.operator = 0;
                }
            }
        }

        // Prevent cursor from moving beyond the end of the line
        int line_length = get_line_length(buffer, cursor_row);
        if (cursor_col > line_length) {
            cursor_col = line_length;
        }

        // Prevent cursor from moving beyond the last line
        int total_lines = get_total_lines(buffer);
        if (cursor_row > total_lines) {
            cursor_row = total_lines;
        }

        if (need_refresh) {
            refresh_screen(buffer);
        }

        update_cursor_position(cursor_row, cursor_col);
    }

    // Free the buffers
    free(buffer);
    free(yank_buffer);
}
