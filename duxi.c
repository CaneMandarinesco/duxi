#include <asm-generic/errno-base.h>
#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <termios.h>

#define VT_ASK_CURSORPOS "\x1b[6n"
#define CTRL_KEY(k) ((k) & 0x1f)

enum KEYS {
    ESC = 27,
    CTRL_Q = CTRL_KEY('q')
};

struct termios old_termios;
struct editor {
    int rawmode;
    int screencols;
    int screenrows;
    int running;
} E;

#define EDITOR_INIT (struct editor) {0,0,0,1}

void errExit(const char *str){
    perror(str);
    exit(-1);
}


/*** raw mode ***/
void terminalResetMode(void){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
}

void editorAtExit(void) {
    terminalResetMode();
}

void terminalSetRawMode(void){
    struct termios t;
    if(!isatty(STDIN_FILENO)) 
        errExit("isatty");
    if(tcgetattr(STDIN_FILENO, &t) == -1)
        errExit("tcgetattr");

    atexit(editorAtExit);

    old_termios = t;
    
    t.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t.c_oflag &= ~(OPOST);
    t.c_cflag |= (CS8);
    t.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 1;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == -1)
        errExit("tcsetattr");
}

/*** terminal control io ***/

int terminalGetCursorPos(int *rx, int *cx){
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, VT_ASK_CURSORPOS, 4) != -4) return -1;
    // Note: sizeof(buf-1), because we need space for '\0'
    while(i < sizeof(buf)-1){
        if (read(STDIN_FILENO, buf+i, 1) == -1 && errno != EAGAIN)
            errExit("read");
        if(buf[i] == 'R') break;

        i++;
    }
    buf[i] = '\0';

    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2, "%d;%d", rx, cx) != 2) return -1;

    return 0;
}

int terminalGetWindowSize(int *rx, int *cx){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws) == -1 || ws.ws_col == 0){
        int orig_row, orig_col, retval;

        retval = terminalGetCursorPos(&orig_row, &orig_col);
        if(retval == -1) errExit("terminalGetCursorPos");

        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        retval = terminalGetCursorPos(rx, cx);
        if(retval == -1) errExit("terminalGetCursorPos");

        char seq[32];
        snprintf(seq, 32, "\x1b[%d;%dH", orig_row, orig_col);
        if(write(STDOUT_FILENO, seq, strlen(seq)) == -1){
            /* oops... */
            return -1;
        }
        return 0;

    }

    *cx = ws.ws_col;
    *rx = ws.ws_row;
    return 0;
}

char terminalReadKey(){
    char c;
    int n;

    while((n= read(STDIN_FILENO, &c, 1)) <= 0){
        if(n == -1 && errno != EAGAIN) errExit("read"); /* TODO: check if it's good this checkkkk*/
    }

    switch(c){
    case ESC:
        break;
    default:
        return c;
    }

    return c;
}

void editorProcessKey(){
    char c = terminalReadKey();
    switch(c){
    case CTRL_Q:
        E.running = 0;
        break;
    default:
        write(STDIN_FILENO, &c, 1);
        break;
    }
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT (struct abuf) {NULL, 0}

void abAppend(struct abuf *ab, const char* s, int len){
    char *new = realloc(ab->b,ab->len + len);
    if(new == NULL) return;

    memcpy(new+ab->len, s, len);
    ab->len += len;
    ab->b = new;
}

void abFree(struct abuf *ab){
    free(ab->b);
    ab->len = 0;
}

/*** rendering ***/
void editorRenderScreen(){
    int i;
    struct abuf ab = ABUF_INIT;

    // TODO: implement
}

int main(int argc, char **argv){
    E = EDITOR_INIT;
    terminalSetRawMode();
    terminalGetWindowSize(&E.screenrows, &E.screencols); 
    
    while(E.running){
        editorRenderScreen();
        editorProcessKey();
    }

    return 0;
}