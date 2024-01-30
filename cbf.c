#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#define DEFAULT_DATA_SIZE 30000
#define DEFAULT_CODE_SIZE 1024

#define DEFAULT_STACK_CAP 32

/* a stack to store the position of opening brackets. */
typedef struct stack {
    int cap;
    int len;
    int *data;
} stack;

stack nesting = {
    0, 0, NULL,
};

void stack_push(int item) {
    /* if item is less than zero, fail silently */
    if (item < 0) return;

    if (nesting.len + 1 > nesting.cap) {
        nesting.data = realloc(nesting.data, nesting.cap * 2);
        if (!nesting.data) {
            perror("error while pushing to stack");
            exit(2);
        }
    }

    nesting.data[nesting.len] = item;
    nesting.len++;
}

int stack_peek() {
    if (nesting.len < 0)
        return -1;
    
    return nesting.data[(nesting.len)-1];
}

int stack_pop() {
    if (nesting.len < 0) return -1;

    int ret = nesting.data[nesting.len-1];
    nesting.len--;

    return ret;
}

/* our code */
char *code;
int insptr = 0;
int codesize = DEFAULT_CODE_SIZE;

/* our data */
char *data;
int dataptr = 0;

enum errtype {SYSTEM = 1, BAD_CODE = 2};
void die(char *errmsg, enum errtype errcode) {
    if (errcode == SYSTEM) {
        fprintf(stderr, "ERROR: %s\n", errmsg);
    } else if (errcode == BAD_CODE) {
        fprintf(stderr, "ERROR@[Pos %d: '%c']: %s\n", insptr, code[insptr], errmsg);
    }
    exit(errcode);
}

char valid_chars[] = {'>', '<', '+', '-', ',', '.', '[', ']'};
bool is_valid(char c) {
    for (int i = 0; i < 8; i++) {
        if (c == valid_chars[i]) return true;
    }

    return false;
}

void read_code_from_file(FILE *codefile) {
    // assume code pointer has already been allocated

    int n = 0;
    int c;
    // read in code from file
    while ((c = fgetc(codefile)) != EOF) {
        // set code to c
        if (is_valid(c)) {
            code[n++] = (char) c;
        }
        // if we reach the edge of our allocated space
        // resize to double the given size
        if (n+1 == codesize) {
            codesize *= 2;
            code = realloc(code, codesize);
            if (code == NULL)
                die("failed to allocate memory", SYSTEM);
        }
    }

    code[n] = '\0';
    //printf("got code: (%s)\n", code);
}

/* Jumps to the command after the matching closing bracket. */
void jump_to_after() {
    int current = nesting.len;
    while (code[insptr] != '\0') {
        switch (code[insptr]) {
            case '[':
            current++;
            break;

            case ']':
            current--;
            if (current == nesting.len) {
                insptr++;
                return;
            }
            break;
        }

        if (current < nesting.len) {
            die("malformed code, missing matching bracket", BAD_CODE);
        }

        insptr++;
    }
}

void execute() {

    char c;
    while ((c = code[insptr]) != '\0') {
        if (insptr < 0) {
            die("instruction pointer went below 0", BAD_CODE);
        }
        if (dataptr < 0) {
            die("data pointer went below 0", BAD_CODE);
        }
        //printf("current instruction: %c at position %d\n", c, insptr+1);
        switch (c) {
            /* increment data pointer */
            case '>':
            dataptr++;
            break;

            /* decrement data pointer */
            case '<':
            dataptr--;
            break;

            /* increment current byte */
            case '+':
            data[dataptr]++;
            break;

            /* decrement current byte */
            case '-':
            data[dataptr]--;
            break;

            /* read one byte from stdin, set current byte */
            case ',': {
                char in = getc(stdin);
                data[dataptr] = in;
            };
            break;

            /* write current byte to stdout */
            case '.':
            putc((int) data[dataptr], stdout);
            fflush(stdout);
            break;

            // if the current byte is zero,
            // jump to command after *matching* closing bracket
            case '[': {
                if (data[dataptr] == 0) {
                    /* skip this bracket */
                    jump_to_after();
                } else {
                    /* we are entering this bracket, so increment our nesting level */
                    stack_push(insptr);
                }
            }; break;

            // if current byte is not zero,
            // jump to command after *matching* opening bracket
            case ']': {
                int opening = stack_peek();
                if (opening < 0)
                    die("mismatched closing bracket", BAD_CODE);

                if (data[dataptr] != 0) {
                    insptr = opening;
                } else {
                    /* exit our loop, go down one level of nesting */
                    stack_pop();
                }
            }; break;
        }

        /* advance instruction pointer */
        insptr++;

        if (insptr+1 == codesize) {
            abort();
        }
    }
}

void initialize() {
    data = calloc(sizeof(char), DEFAULT_DATA_SIZE);
    code = malloc(sizeof(char) * DEFAULT_CODE_SIZE);
    nesting.data = malloc(DEFAULT_STACK_CAP * sizeof(int));
    nesting.cap = DEFAULT_STACK_CAP;

    if (data == NULL || code == NULL || nesting.data == NULL) {
        perror("error while setting up");
        exit(SYSTEM);
    }
}

void teardown() {
    free(data);
    free(code);
    free(nesting.data);
}


int main(int argc, char **argv) {
    initialize();

    if (argc == 1) {
        printf("usage: %s FILE\n", argv[0]);
        return EXIT_SUCCESS;
    }

    char *filename = argv[1];

    FILE *codefile = fopen(filename, "r");
    if (!codefile) {
        perror("error when opening file");
        return SYSTEM;
    }

    //printf("reading code from file %s\n", argv[1]);
    read_code_from_file(codefile);

    fclose(codefile);

    //printf("executing code\n");
    execute();

    teardown();
    return EXIT_SUCCESS;

}