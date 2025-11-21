
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <direct.h>
  #define getcwd _getcwd
  #define PATH_SEP '\\'
#else
  #include <unistd.h>
  #define PATH_SEP '/'
#endif

#include <limits.h>
#ifndef PATH_MAX
  #define PATH_MAX 1024
#endif

#include "lookup.h"  // keyword/noise word/reserved word matcher

#define MAX_LEX 1024
#define MAX_SYMBOLS 20000
#define MAX_ERRORS 1024

typedef enum {
    T_NEWLINE, T_WHITESPACE, T_COMMENT, T_STRING, T_NUMBER,
    T_DATATYPE, T_KEYWORD, T_RESERVED, T_NOISE, T_IDENTIFIER,
    T_UNARY_OP, T_EXP_OP, T_ASSIGN_OP, T_REL_OP, T_LOGICAL_OP, T_ARITH_OP,
    T_COLON, T_COMMA, T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET,
    T_LEX_ERROR, T_UNKNOWN,
    T_COUNT
} SymType;

typedef struct {
    char lex[MAX_LEX];
    SymType type;
    int line;
    int col;
} Symbol;

static Symbol symtab[MAX_SYMBOLS];
static int symcount = 0;

/* error record for summary */
typedef struct {
    char lex[MAX_LEX];
    int line;
    int col;
} LexError;
static LexError errors[MAX_ERRORS];
static int errcount = 0;

/* track previous token (for unary detection) */
static SymType prev_type = T_NEWLINE; // start-of-input acts like newline -> allows unary at start
static char prev_lexeme[128] = "";

static void update_prev_token(const char *lex, SymType type) {
    prev_type = type;
    if (lex && lex[0]) {
        strncpy(prev_lexeme, lex, sizeof(prev_lexeme) - 1);
        prev_lexeme[sizeof(prev_lexeme)-1] = '\0';
    } else {
        prev_lexeme[0] = '\0';
    }
}

static void add_symbol(const char *lex, SymType type, int line, int col) {
    if (!lex) return;
    if (symcount < MAX_SYMBOLS) {
        strncpy(symtab[symcount].lex, lex, MAX_LEX - 1);
        symtab[symcount].lex[MAX_LEX - 1] = '\0';
        symtab[symcount].type = type;
        symtab[symcount].line = line;
        symtab[symcount].col = col;
        ++symcount;
    }
    if (type == T_LEX_ERROR) {
        if (errcount < MAX_ERRORS) {
            strncpy(errors[errcount].lex, lex, MAX_LEX - 1);
            errors[errcount].lex[MAX_LEX - 1] = '\0';
            errors[errcount].line = line;
            errors[errcount].col = col;
            ++errcount;
        }
    }
    /* Update prev token only for meaningful tokens — don't let whitespace/newline/comment overwrite context */
    if (type != T_WHITESPACE && type != T_NEWLINE && type != T_COMMENT) {
        update_prev_token(lex, type);
    }
}

/* mapping from SymType to name used in symbol table & summary (used only for types not printed as lexeme) */
static const char *token_names[T_COUNT] = {
    "NEWLINE", "WHITESPACE", "COMMENT", "STRING", "NUMBER",
    "DATATYPE", "KEYWORD", "RESERVED", "NOISE", "IDENTIFIER",
    "UNARY_OP", "EXP_OP", "ASSIGN_OP", "REL_OP", "LOGICAL_OP", "ARITH_OP",
    "COLON", "COMMA", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET",
    "LEXICAL_ERROR", "UNKNOWN"
};

/* write symbol table */
static int write_symbol_table_to_path(const char *outpath) {
    FILE *f = fopen(outpath, "w");
    if (!f) return 0;

    fprintf(f, "=== SIMPLE LEXICAL ANALYZER OUTPUT ===\n\n");
    fprintf(f, "------------- SYMBOL TABLE -------------\n");
    fprintf(f, " Line |   Col | Token           | Lexeme\n");
    fprintf(f, "-------------------------------------------------------\n");

    for (int i = 0; i < symcount; ++i) {
        const char *tok;
        SymType t = symtab[i].type;

        /* STYLE-F: for keywords/reserved/noise, for ALL operator types, and for SIMPLE delimiters,
           print the token column as the lexeme itself (e.g. "if", "+", "==", "(", ",", ":"). */
        if (t == T_KEYWORD || t == T_RESERVED || t == T_NOISE ||
            t == T_ARITH_OP || t == T_UNARY_OP || t == T_REL_OP ||
            t == T_LOGICAL_OP || t == T_ASSIGN_OP || t == T_EXP_OP ||
            t == T_COLON || t == T_COMMA || t == T_LPAREN || t == T_RPAREN ||
            t == T_LBRACKET || t == T_RBRACKET) {

            tok = symtab[i].lex;
        } else if (t >= 0 && t < T_COUNT) {
            tok = token_names[t];
        } else {
            tok = "UNKNOWN";
        }

        fprintf(f, "%6d | %6d | %-15s | %s\n",
                symtab[i].line, symtab[i].col, tok, symtab[i].lex);
    }

    /* Token Summary removed as requested. Keep only error listing. */

    fprintf(f, "\nErrors (%d):\n", errcount);
    if (errcount == 0) fprintf(f, "  (none)\n");
    else {
        for (int i = 0; i < errcount; ++i)
            fprintf(f, "  - Invalid token '%s' at line %d, col %d\n",
                    errors[i].lex, errors[i].line, errors[i].col);
    }

    fclose(f);
    return 1;
}

/* scanning state */
static FILE *infile = NULL;
static int cur_line = 1;
static int cur_col  = 0;

static int getch(void) {
    int c = fgetc(infile);
    if (c == EOF) return EOF;
    if (c == '\n') {
        cur_line++;
        cur_col = 0;
    } else {
        cur_col++;
    }
    return c;
}

static void ungetch(int c) {
    if (c == EOF) return;
    if (ungetc(c, infile) == EOF) return;
    if (c == '\n') {
        if (cur_line > 1) cur_line--;
        cur_col = 0;
    } else {
        if (cur_col > 0) cur_col--;
    }
}

static int peekch(void) {
    int c = getch();
    if (c == EOF) return EOF;
    ungetch(c);
    return c;
}

static int is_datatype(const char *s) {
    const char *types[] = {"int", "float", "char", "bool", "array", NULL};
    for (int i = 0; types[i]; ++i)
        if (strcmp(s, types[i]) == 0)
            return 1;
    return 0;
}

/* Helper: decide whether previous token context allows a unary + or - */
static bool prev_allows_unary(void) {
    switch (prev_type) {
        case T_NEWLINE:
        case T_ASSIGN_OP:
        case T_ARITH_OP:
        case T_REL_OP:
        case T_LOGICAL_OP:
        case T_UNARY_OP:
        case T_COLON:
        case T_COMMA:
        case T_LPAREN:
        case T_LBRACKET:
            return true;
        default:
            break;
    }
    if (prev_lexeme[0] == '\0') return true;
    return false;
}

int main(void) {
    char filename[PATH_MAX];
    printf("\n === SIMPLE Lexical Analyzer ===\n\n");
    printf("Enter SIMPLE source file: ");
    if (!fgets(filename, sizeof(filename), stdin)) return 1;

    filename[strcspn(filename, "\r\n")] = 0;

    infile = fopen(filename, "r");
    if (!infile) { perror("Cannot open file\nOnly .simp file extension could be recognize"); return 1; }

    cur_line = 1;
    cur_col  = 0;
    symcount = 0;
    errcount = 0;
    prev_type = T_NEWLINE;
    prev_lexeme[0] = '\0';

    int c;
    while ((c = getch()) != EOF) {

        /* NEWLINE */
        if (c == '\n') {
            add_symbol("\\n", T_NEWLINE, cur_line - 1, 1);
            continue;
        }

        /* WHITESPACE */
        if (c == ' ' || c == '\t') {
            int start_col_ws = cur_col;
            char buf[256]; int bi = 0;
            buf[bi++] = (char)c;

            int ch;
            while ((ch = getch()) != EOF && (ch == ' ' || ch == '\t')) {
                if (bi < 255) buf[bi++] = (char)ch;
            }
            if (ch != EOF) ungetch(ch);

            buf[bi] = '\0';

            int start = start_col_ws - ((int)strlen(buf) - 1);
            if (start < 1) start = 1;

            add_symbol(buf, T_WHITESPACE, cur_line, start);
            continue;
        }

        int start_line = cur_line;
        int start_col  = cur_col;

        /* COMMENTS and / / /=
           handle "/=" as an assignment operator BEFORE emitting single '/'
        */
        if (c == '/') {
            int nxt = getch();
            if (nxt == '/') {
                char buf[2048]; int bi = 0;
                buf[bi++] = '/'; buf[bi++] = '/';
                int ch;
                while ((ch = getch()) != EOF && ch != '\n') {
                    if (bi < 2047) buf[bi++] = (char)ch;
                }
                buf[bi] = '\0';
                add_symbol(buf, T_COMMENT, start_line, start_col);
                if (ch == '\n') continue;
                continue;
            }
            else if (nxt == '*') {
                char buf[8192]; int bi = 0;
                buf[bi++] = '/'; buf[bi++] = '*';
                int ch;
                int prev = 0;
                int closed = 0;
                while ((ch = getch()) != EOF) {
                    if (bi < 8191) buf[bi++] = (char)ch;
                    if (prev == '*' && ch == '/') { closed = 1; break; }
                    prev = ch;
                }
                buf[bi] = '\0';
                if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
                else add_symbol(buf, T_COMMENT, start_line, start_col);
                continue;
            }
            else if (nxt == '=') {
                /* handle "/=" assignment */
                add_symbol("/=", T_ASSIGN_OP, start_line, start_col);
                continue;
            }
            else {
                if (nxt != EOF) ungetch(nxt);
                /* single '/' arithmetic operator (will be printed as lexeme) */
                add_symbol("/", T_ARITH_OP, start_line, start_col);
                continue;
            }
        }

        /* STRING LITERAL */
        if (c == '"') {
            char buf[2048];
            int bi = 0;
            int closed = 0;
            int ch;

            while ((ch = getch()) != EOF) {
                if (ch == '\\') {
                    int e = getch();
                    if (e == EOF) break;
                    if (bi < 2046) { buf[bi++] = '\\'; buf[bi++] = (char)e; }
                    continue;
                }
                if (ch == '"') { closed = 1; break; }
                if (bi < 2047) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';

            if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
            else add_symbol(buf, T_STRING, start_line, start_col);
            continue;
        }

        /* NUMBER */
        if (isdigit(c)) {
            char buf[256];
            int bi = 0;
            int dot = 0;
            int ch = c;
            buf[bi++] = (char)c;

            while ((ch = peekch()) != EOF && (isdigit(ch) || (ch == '.' && !dot))) {
                ch = getch();
                if (ch == '.') dot = 1;
                if (bi < 255) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';
            add_symbol(buf, T_NUMBER, start_line, start_col);
            continue;
        }

        /* IDENTIFIER / KEYWORD / DATATYPE / RESERVED / NOISE + SPECIAL CASE: "to do" */
        if (isalpha(c) || c == '_') {

            char buf[MAX_LEX];
            int bi = 0;
            int ch = c;
            buf[bi++] = (char)c;

            while ((ch = peekch()) != EOF && (isalnum(ch) || ch == '_')) {
                ch = getch();
                if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';

            /* SPECIAL MERGE RULE FOR "to do" */
            if (strcmp(buf, "to") == 0) {

                int wc = peekch();
                if (wc == ' ' || wc == '\t') {
                    int ws_count = 0;
                    int ww;

                    while ((ww = peekch()) == ' ' || ww == '\t') {
                        getch();
                        ws_count++;
                    }

                    char next[16] = {0};
                    int ni = 0;
                    int nc = peekch();
                    if (isalpha(nc)) {
                        while ((nc = peekch()) != EOF && isalpha(nc)) {
                            nc = getch();
                            next[ni++] = (char)nc;
                            if (ni >= 15) break;
                        }
                        next[ni] = '\0';
                    }

                    if (strcmp(next, "do") == 0) {
                        strcpy(buf, "to do");
                        add_symbol(buf, T_KEYWORD, start_line, start_col);
                        continue;
                    }

                    for (int k = ni - 1; k >= 0; k--) ungetch(next[k]);
                    for (int k = 0; k < ws_count; k++) ungetch(' ');
                }
            }

            char low[MAX_LEX];
            size_t L = strlen(buf);
            for (size_t i = 0; i < L; ++i) low[i] = tolower((unsigned char)buf[i]);
            low[L] = '\0';

            /* Use lowercase buffer for keyword lookup */
            int kclass = lookupKeyword(low);

            if (is_datatype(low)) add_symbol(buf, T_DATATYPE, start_line, start_col);
            else if (kclass == 1) add_symbol(buf, T_KEYWORD, start_line, start_col);
            else if (kclass == 2) add_symbol(buf, T_RESERVED, start_line, start_col);
            else if (kclass == 3) add_symbol(buf, T_NOISE, start_line, start_col);
            else add_symbol(buf, T_IDENTIFIER, start_line, start_col);

            continue;
        }

        /* TWO-CHAR LOOKAHEAD */
        int nxt = peekch();
        char twobuf[3] = {0};
        if (nxt != EOF) {
            twobuf[0] = (char)c;
            twobuf[1] = (char)nxt;
            twobuf[2] = '\0';
        }

        /* Multi-char unary ++ -- */
        if (strcmp(twobuf, "++") == 0 || strcmp(twobuf, "--") == 0) {
            getch(); getch();
            add_symbol(twobuf, T_UNARY_OP, start_line, start_col);
            continue;
        }

        /* EXP ^ */
        if (c == '^') {
            add_symbol("^", T_EXP_OP, start_line, start_col);
            continue;
        }

        /* RELATIONAL two-char (check before single '=') */
        if (strcmp(twobuf, "<=") == 0 || strcmp(twobuf, ">=") == 0 ||
            strcmp(twobuf, "==") == 0 || strcmp(twobuf, "!=") == 0) {
            getch(); getch();
            add_symbol(twobuf, T_REL_OP, start_line, start_col);
            continue;
        }

        /* ASSIGN two-char operators (+=, -=, *=, /=, %=, ~=) */
        if (strcmp(twobuf, "+=") == 0 || strcmp(twobuf, "-=") == 0 ||
            strcmp(twobuf, "*=") == 0 || strcmp(twobuf, "/=") == 0 ||
            strcmp(twobuf, "%=") == 0 || strcmp(twobuf, "~=") == 0) {
            getch(); getch();
            add_symbol(twobuf, T_ASSIGN_OP, start_line, start_col);
            continue;
        }

        /* single '=' assign (after checking '==') */
        if (c == '=') {
            add_symbol("=", T_ASSIGN_OP, start_line, start_col);
            continue;
        }

        /* single < or > */
        if (c == '<' || c == '>') {
            char t[2] = {(char)c, '\0'};
            add_symbol(t, T_REL_OP, start_line, start_col);
            continue;
        }

        /* LOGICAL two-char/operators */
        if (strcmp(twobuf, "&&") == 0 || strcmp(twobuf, "||") == 0) {
            getch(); getch();
            add_symbol(twobuf, T_LOGICAL_OP, start_line, start_col);
            continue;
        }
        if (c == '!') {
            /* '!=' handled above; single '!' logical NOT */
            add_symbol("!", T_LOGICAL_OP, start_line, start_col);
            continue;
        }

        /* ARITHMETIC and UNARY single-char handling */
        /* multiplication, modulo, integer division ~, percent, etc. */
        if (c == '*' || c == '%' || c == '~') {
            char t[2] = {(char)c, '\0'};
            add_symbol(t, T_ARITH_OP, start_line, start_col);
            continue;
        }

        /* PLUS and MINUS: decide unary vs binary (single char) */
        if (c == '+' || c == '-') {
            /* ++/-- already handled above */
            bool unary = prev_allows_unary();
            char t[2] = {(char)c, '\0'};
            if (unary) add_symbol(t, T_UNARY_OP, start_line, start_col);
            else add_symbol(t, T_ARITH_OP, start_line, start_col);
            continue;
        }

        /* DELIMITERS (SIMPLE-only): ( ) [ ] , :  -> token printed as lexeme */
        if (c == '(') { add_symbol("(", T_LPAREN, start_line, start_col); continue; }
        if (c == ')') { add_symbol(")", T_RPAREN, start_line, start_col); continue; }
        if (c == '[') { add_symbol("[", T_LBRACKET, start_line, start_col); continue; }
        if (c == ']') { add_symbol("]", T_RBRACKET, start_line, start_col); continue; }
        if (c == ':') { add_symbol(":", T_COLON, start_line, start_col); continue; }
        if (c == ',') { add_symbol(",", T_COMMA, start_line, start_col); continue; }

        /* UNKNOWN CHARACTER -> lexical error */
        {
            char tmp[2] = {(char)c, '\0'};
            add_symbol(tmp, T_LEX_ERROR, start_line, start_col);
            continue;
        }
    }

    fclose(infile);

    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    char outpath[PATH_MAX + 64];
    snprintf(outpath, sizeof(outpath), "%s%cSymbolTable.txt", cwd, PATH_SEP);

    if (!write_symbol_table_to_path(outpath))
        printf("Failed to write output.\n");
    else printf("Symbol Table saved to: %s\n", outpath);
    printf("Analysis Complete.\n");

    return 0;
}
