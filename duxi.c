#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <termios.h>
#include <ctype.h>

#define DEBUG_FILE "divinacommedia.txt"
#define TAB_SIZE 4

#define VT_GET_CURSORPOS    "\x1b[6n"
#define VT_HIDE_CURSOR      "\x1b[?25l"
#define VT_SHOW_CURSOR      "\x1b[?25h"
#define VT_HOME_CURSOR      "\x1b[H"
#define VT_CLEAR_LINE       "\x1b[0K"


enum KEYS {
    CTRL_Q = 17,
    ESC = '\x1b',
    ARROW_UP=65,
    ARROW_DOWN,
    ARROW_RIGHT,
    ARROW_LEFT,
    BACKSPACE=127
};

struct editor {
    int running;
    int cx, cy;
    int scrlx, scrly;
    int wndx, wndy;

    struct erow *erows;
    int nrows;
    char msg[80];
} E;

struct erow {
    char *str;
    int len;

    char *render;
    int renderlen;
};

struct termios orig_termios;

/* error */
void errExit(const char* str){
    perror(str);
    exit(-1);
}

/* append buffer */
struct abuf {
    char *buf;
    int s;
};

#define ABUF_INIT (struct abuf){NULL, 0}
void abAppend(struct abuf *ab, char *str, int n){
    char *new = realloc(ab->buf, ab->s+n);
    if(new == NULL) return;

    memcpy(new+ab->s, str, n);
    ab->buf = new;
    ab->s += n;
}
void abFree(struct abuf *ab){
    free(ab->buf);
    ab->buf = NULL;
    ab->s = 0;
}

/* terminal io */
int terminalGetCursorPosition(int *cy, int *cx){
    char buf[32];
    unsigned int i;

    if(write(STDOUT_FILENO, VT_GET_CURSORPOS, 4)!= 4)
        errExit("write");

    i=0; 
    while(i < sizeof(buf)){
        if(read(STDIN_FILENO, buf+i, 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    // note: buf+2 skip first 2 bytes of escape code
    if(sscanf(buf+2, "%d;%d", cy, cx) != 2){
        return -1;
    }

    return 0;
}

int terminalGetSize(int *cy, int *cx){
    struct winsize ws;
    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
        /* ioctl failed */
        int old_y, old_x;
        char buf[32];
        int n;

        if(terminalGetCursorPosition(&old_y, &old_x) == -1) 
            return -1;

        write(STDOUT_FILENO, "\x1b[999;999H", 11);

        if(terminalGetCursorPosition(cy, cx) == -1)
            return -1;

        n = snprintf(buf, 32, "\x1b[%d;%dH", old_y, old_x);
        if(write(STDOUT_FILENO, buf, n) == -1) { /* lool */}
        return 0;
    }

    *cy = ws.ws_row;
    *cx = ws.ws_col;
    return 0;
}

int terminalResetMode(){
    tcsetattr(STDIN_FILENO,TCSAFLUSH, &orig_termios);
    return 0;
}

int terminalSetRawMode(){
    struct termios raw;

    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) 
        return -1;

    raw = orig_termios;  /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - choing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN] = 0; /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; 

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) 
        return -1;

    return 0;
}

char terminalReadKey(){
    char c, seq[3];
    int n;
    while((n = read(STDIN_FILENO, &c, 1)) == 0);
    if(n == -1) 
        errExit("read");

    while(1){
        switch(c){
        case ESC:
            if(read(STDIN_FILENO, seq, 1) == 0) break;
            if(read(STDIN_FILENO, seq+1, 1) == 0) break;

            if(seq[0] == '['){
                switch(seq[1]){
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                }
            }
            break;
        default:
            return c;
        }
    }
}

void editorInsertCharacter(){
    // render_offset is an offset in render text
    int render_offset = E.cx + E.scrlx;
    // get current row
    struct erow er = E.erows[E.cy+E.scrly];

    int i = 0;
    while(render_offset > 0){
        if(er.str[i] == '\t'){
            render_offset -= TAB_SIZE;
        } else{
            render_offset--;
        }
        i++;
    }

    // render new text!
}

void editorDeleteCharacter(){
    // delete character in raw text

    // render new text
}

void editorProcessKey(){
    int real_x, real_y, rl;
    real_x = E.cx + E.scrlx;
    real_y = E.cy + E.scrly;

    char c = terminalReadKey();
    switch(c){
    case CTRL_Q:
        E.running = 0;
        break;
    case ARROW_UP:
        if(E.cy == 0){
            if(E.scrly) {
                E.scrly--;
            }
            break;
        }
        E.cy--;
        break;
    case ARROW_DOWN:
        if(E.cy == E.wndy-2){
            if(E.cy + E.scrly + 1 < E.nrows){
                E.scrly++;
            }
            break;
        }
        E.cy++;
        break;
    case ARROW_LEFT:
        if(E.cx == 0){
            if(E.scrlx){
                E.scrlx--;
            }
            break;
        }
        E.cx--;
        break;
    case ARROW_RIGHT:
        if(E.cx + E.scrlx + 1 > E.erows[E.cy+E.scrly].renderlen)
            break;

        if(E.cx == E.wndx-1){
            if(E.cx + E.scrlx + 1 > E.erows[E.cy+E.scrly].renderlen){
                // se vado fuori break
                break;
            }
            E.scrlx++;
            break;
        }
        E.cx++;
        break;

    case 
    
    default:
        editorInsertCharacter(c);
        break;
    }

    // controlla il cursor e' sopra il testo
    struct erow er;
    er = E.erows[E.cy+E.scrly];

    if(!er.renderlen) {
        E.cx = 0;
        goto cursor_ok;
    }

    if(E.cx + E.scrlx > er.renderlen){
        if(!E.scrlx){
            E.cx = er.renderlen;
            goto cursor_ok;
        }

        if(E.cx > er.renderlen - E.scrlx){
            E.cx = er.renderlen - E.scrlx;
            goto cursor_ok;
        }

        E.scrlx = E.erows[E.cy+E.scrly].renderlen;
        E.cx = 0;
    }

cursor_ok: ;
}

/* rendering */
int editorSetMessage(const char* format, ...){
    va_list args;
    va_start(args, format);
    int n = vsnprintf(E.msg, 80, format, args);
    va_end(args);
    return n;
}

void editorRefreshScreen(){
    struct abuf ab = ABUF_INIT;
    char buf[32];
    int n, y;
    
    abAppend(&ab, VT_HIDE_CURSOR, 6);
    abAppend(&ab, VT_HOME_CURSOR, 3);

    // render lines
    for(y = 0; y < E.wndy-1; y++){
        int real_y = y + E.scrly;
        if (real_y < E.nrows && E.erows[real_y].renderlen > 0){
            int rl = E.erows[real_y].renderlen;

            // nothing to render
            if(E.scrlx > rl) goto clearln;

            // apply horizontal scroll
            char *from = E.erows[real_y].render + E.scrlx; 
            int n = rl - E.scrlx;

            // check and correct overflow
            if(n > E.wndx) n = E.wndx;

            abAppend(&ab, from, n);
        }

clearln: 
        abAppend(&ab, VT_CLEAR_LINE, 4);
        abAppend(&ab, "\r\n", 2);

    } 

    // render message
    n = editorSetMessage("INFO: cy: %d, cx: %d, scrly: %d, scrlx: %d", E.cy, E.cx, E.scrly, E.scrlx);
    abAppend(&ab, E.msg, n);

    // restore cursor position
    n = snprintf(buf, 32, "\x1b[%d;%dH", E.cy+1, E.cx+1);
    abAppend(&ab, buf, n);
    abAppend(&ab, VT_SHOW_CURSOR, 6);

    write(STDOUT_FILENO, ab.buf, ab.s);
    abFree(&ab);
}

void editorRenderRow(struct erow *e){
    char *s;
    int rlen = 0;

    for(s = e->str; *s != '\0'; s++){
        if(*s == '\t') rlen += TAB_SIZE - (rlen % TAB_SIZE);
        else if(isprint(*s)) rlen++;
    }

    // Questo!!!!
    char *r = malloc(rlen+1);
    rlen = 0;
    for(s = e->str; *s != '\0'; s++){
        if(*s == '\t'){
            int t = TAB_SIZE - (rlen % TAB_SIZE);
            for(; t > 0; t--) r[rlen+t] = ' '; 
            rlen+=t;
        } else if (isprint(*s)){
            r[rlen++] = *s;
        }
    }
    r[rlen] = '\0';

    e->render = r;
    e->renderlen = rlen;
}

void editorAppendRow(const char* str, int n){
    E.erows = realloc(E.erows, (E.nrows+1) * sizeof(struct erow));
    struct erow *e = E.erows+E.nrows;

    /* n+1: make room for \0 */
    e->str = malloc(n+1);
    memcpy(e->str, str, n+1);
    e->len = n;

    editorRenderRow(e);

    E.nrows++;
}

/* file io*/
int editorReadFile(char *path){
    FILE *fp = fopen(path, "r");
    if(fp == NULL) errExit("fopen");

    char *buf = NULL;
    size_t bufsize = 0;
    ssize_t n;
    while((n = getline(&buf, &bufsize, fp)) != -1){
        if(n && (buf[n-1] == '\r' || buf[n-1] == '\n')) {
            buf[--n] = '\0';
        }

        editorAppendRow(buf, n);
    }

    free(buf);
    fclose(fp);
    return 0;
}

/* atexit */
void editorFree(){
    int y;
    for(y = 0; y < E.nrows; y++){
        free(E.erows[y].str);
        if(E.erows[y].render) free(E.erows[y].render);
    }
    free(E.erows);
}

void editorOnExit(){
    terminalResetMode();
}

int main(){
    E = (struct editor){
        .running = 1,
        .cx = 0, .cy = 0,
        .wndx = 0, .wndy = 0,
        .msg = ""
    };

    if (terminalSetRawMode() == -1)
        errExit("terminalSetRawMode");

    if (terminalGetSize(&E.wndy, &E.wndx) == -1)
        errExit("terminalGetSize");

    if (editorReadFile(DEBUG_FILE) == -1)
        errExit("editorReadFile");

    while(E.running){
        editorRefreshScreen();
        editorProcessKey();
    }

    editorFree();

    return 0;
}