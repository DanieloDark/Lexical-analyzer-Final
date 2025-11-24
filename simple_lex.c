// simple_lex.c

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

#include "lookup.h"  // user-provided DFA keyword matcher - must exist

#define MAX_LEX 4096
#define MAX_SYMBOLS 40000
#define MAX_ERRORS 4096

typedef enum {
    T_NEWLINE, T_WHITESPACE, T_COMMENT,
    T_STRING, T_TEXT, T_SECURE, T_CHAR,
    T_FLOAT, T_INT, T_BOOL, T_TIME, T_DATE, T_TIMESTAMP,
    T_ARRAY, T_COLLECTION,
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

/* (for unary detection) */
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

/* mapping from SymType to name used in symbol table & summary */
static const char *token_names[T_COUNT] = {
    "NEWLINE", "WHITESPACE", "COMMENT",
    "STRING", "TEXT", "SECURE", "CHAR",
    "FLOAT", "INT", "BOOL", "TIME", "DATE", "TIMESTAMP",
    "ARRAY", "COLLECTION",
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
        if (t >= 0 && t < T_COUNT) tok = token_names[t];
        else tok = "UNKNOWN";

        fprintf(f, "%6d | %6d | %-15s | %s\n",
                symtab[i].line, symtab[i].col, tok, symtab[i].lex);
    }

    long counts[T_COUNT];
    for (int i = 0; i < T_COUNT; ++i) counts[i] = 0;
    for (int i = 0; i < symcount; ++i) counts[symtab[i].type]++;

    fprintf(f, "\n--- Token Summary ---\n");

    /* Print the most important tokens (selective) */
    const SymType order[] = {
        T_COMMENT, T_NEWLINE, T_KEYWORD, T_WHITESPACE, T_IDENTIFIER,
        T_REL_OP, T_INT, T_FLOAT, T_CHAR, T_STRING,
        T_DATATYPE, T_ARRAY, T_COLLECTION, T_LEX_ERROR, T_RESERVED
    };
    const int order_len = sizeof(order)/sizeof(order[0]);

    for (int i = 0; i < order_len; ++i)
        fprintf(f, "%-12s: %ld\n", token_names[ order[i] ], counts[ order[i] ]);

    long total_incl = symcount;
    long total_excl = total_incl - counts[T_WHITESPACE] - counts[T_NEWLINE];

    fprintf(f, "\nTotal tokens (including whitespace/newlines): %ld\n", total_incl);
    fprintf(f, "Total tokens (excluding whitespace/newlines): %ld\n\n", total_excl);

    fprintf(f, "Errors (%d):\n", errcount);
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

/* Known datatypes as keywords for declarations (still recognized separately as DATATYPE token) */
static bool is_datatype(const char *s) {
    const char *types[] = {"int", "float", "char", "string", "text", "secure", "bool", "time", "date", "timestamp", "array", "collection", NULL};
    for (int i = 0; types[i]; ++i)
        if (strcmp(s, types[i]) == 0)
            return true;
    return false;
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

/* Helper: check if string is "true" or "false" (case-insensitive) */
static bool is_bool_literal(const char *s) {
    if (!s) return false;
    if (strcasecmp(s, "true") == 0) return true;
    if (strcasecmp(s, "false") == 0) return true;
    return false;
}

/* helpers to validate date/time patterns minimally */
static bool looks_like_time(const char *s) {
    // Accept HH:MM or HH:MM:SS (basic)
    int n = strlen(s);
    if (n < 4) return false;
    int colon_count = 0;
    for (int i = 0; i < n; ++i) if (s[i] == ':') colon_count++;
    return (colon_count == 1 || colon_count == 2);
}
static bool looks_like_date_iso(const char *s) {
    // Very basic ISO-like YYYY-MM-DD (digits and '-' at positions)
    int n = strlen(s);
    if (n < 8) return false;
    int dash_count = 0;
    for (int i = 0; i < n; ++i) if (s[i] == '-') dash_count++;
    return dash_count == 2;
}

/* Read until matching quote/delimiter with basic escape handling.
   Returns dynamically allocated string (caller must free) containing inner content (without outer quotes/brackets).
   If not closed properly, returns NULL.
*/
static char *read_until(FILE *f, int *out_line, int *out_col, int delim, int allow_escape) {
    // This helper assumes getch() and file are in proper state; but we won't reuse it heavily because we need to use getch/ungetch and current pos.
    (void)f; // unused but kept for signature consistency
    // Not used in this code - kept for possible extension.
    return NULL;
}

int main(void) {
    char filename[PATH_MAX];
    printf("\n === SIMPLE Lexical Analyzer ===\n\n");
    printf("Enter SIMPLE source file: ");
    if (!fgets(filename, sizeof(filename), stdin)) return 1;

    filename[strcspn(filename, "\r\n")] = 0;

    infile = fopen(filename, "r");
    if (!infile) { perror("Cannot open file\nOnly .simp file extension will be read"); return 1; }

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

        /* COMMENTS and special handling for '/=' etc */
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
                add_symbol("/", T_ARITH_OP, start_line, start_col);
                continue;
            }
        }

        /* TRIPLE-QUOTED TEXT (""" ... """) */
        if (c == '"' ) {
            int p1 = peekch();
            int p2 = EOF;
            if (p1 != EOF) {
                getch(); // consume p1
                p2 = peekch();
                ungetch(p1); // restore p1
            }
            if (p1 == '"' && p2 == '"') {
                // consume two extra quotes
                getch(); getch(); // now we've consumed three quotes total (first c and the two)
                // read until triple quote
                char buf[MAX_LEX]; int bi = 0;
                int closed = 0;
                int ch;
                while ((ch = getch()) != EOF) {
                    if (ch == '"' ) {
                        int q1 = peekch();
                        if (q1 == '"') {
                            getch();
                            int q2 = peekch();
                            if (q2 == '"') {
                                getch();
                                closed = 1;
                                break;
                            } else {
                                // uncommon case: only two quotes, put them back and continue
                                ungetch(q2);
                                ungetch('"');
                                continue;
                            }
                        } else {
                            // a single quote inside text; accept
                            if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
                        }
                    } else {
                        if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
                    }
                }
                buf[bi] = '\0';
                if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
                else add_symbol(buf, T_TEXT, start_line, start_col);
                continue;
            }
        }

        /* STRING LITERAL "..." (single-line preferred) */
        if (c == '"') {
            char buf[MAX_LEX];
            int bi = 0;
            int closed = 0;
            int ch;
            while ((ch = getch()) != EOF) {
                if (ch == '\\') {
                    int e = getch();
                    if (e == EOF) break;
                    // store escaped char as-is (we store inner content)
                    if (bi < MAX_LEX - 2) { buf[bi++] = '\\'; buf[bi++] = (char)e; }
                    continue;
                }
                if (ch == '"') { closed = 1; break; }
                if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';
            if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
            else add_symbol(buf, T_STRING, start_line, start_col);
            continue;
        }

        /* SECURE literal: backtick-delimited with NO SPACES inside */
        if (c == '`') {
            char buf[MAX_LEX];
            int bi = 0;
            int ch;
            int closed = 0;
            bool has_space = false;
            while ((ch = getch()) != EOF) {
                if (ch == '`') { closed = 1; break; }
                if (isspace(ch)) has_space = true;
                if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';
            if (!closed || has_space) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
            else add_symbol(buf, T_SECURE, start_line, start_col);
            continue;
        }

        /* CHAR literal: 'A' (we will return lexeme as A without quotes) */
        if (c == '\'') {
            char buf[8];
            int bi = 0;
            int ch = getch();
            if (ch == EOF) { add_symbol("", T_LEX_ERROR, start_line, start_col); continue; }
            if (ch == '\\') {
                int esc = getch();
                if (esc == EOF) { add_symbol("", T_LEX_ERROR, start_line, start_col); continue; }
                // accept escaped single char like '\n', '\'', '\\'
                if (bi < (int)sizeof(buf)-1) buf[bi++] = (char)ch, buf[bi++] = (char)esc;
                ch = getch(); // should be closing '
                if (ch != '\'') { add_symbol("", T_LEX_ERROR, start_line, start_col); continue; }
                buf[bi] = '\0';
                add_symbol(buf, T_CHAR, start_line, start_col);
                continue;
            } else {
                // single character then expect closing '
                int closing = getch();
                if (closing != '\'') {
                    // malformed char literal (maybe multi-char) -> treat as identifier or error
                    // we will roll back (put back any extra read) and treat starting quote as lex error
                    // But since we've consumed next chars, the simplest is to mark lex error
                    add_symbol("", T_LEX_ERROR, start_line, start_col);
                    continue;
                } else {
                    // ch is the character
                    char out[4] = {0};
                    out[0] = (char)ch;
                    out[1] = '\0';
                    add_symbol(out, T_CHAR, start_line, start_col);
                    continue;
                }
            }
        }

        /* NUMBER / DATE / TIME / TIMESTAMP / ARRAY / COLLECTION handling */

        /* ARRAYS: [ ... ] => ARRAY (inner content as lexeme) */
        if (c == '[') {
            char buf[MAX_LEX];
            int bi = 0;
            int depth = 1;
            int ch;
            bool closed = false;
            while ((ch = getch()) != EOF) {
                if (ch == '[') { depth++; if (bi < MAX_LEX-1) buf[bi++] = (char)ch; }
                else if (ch == ']') { depth--; if (depth == 0) { closed = true; break; } else if (bi < MAX_LEX-1) buf[bi++] = (char)ch; }
                else {
                    if (bi < MAX_LEX-1) buf[bi++] = (char)ch;
                }
            }
            buf[bi] = '\0';
            if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
            else add_symbol(buf, T_ARRAY, start_line, start_col);
            continue;
        }

        /* COLLECTIONS: { ... } => COLLECTION (inner content as lexeme) */
        if (c == '{') {
            char buf[MAX_LEX];
            int bi = 0;
            int depth = 1;
            int ch;
            bool closed = false;
            while ((ch = getch()) != EOF) {
                if (ch == '{') { depth++; if (bi < MAX_LEX-1) buf[bi++] = (char)ch; }
                else if (ch == '}') { depth--; if (depth == 0) { closed = true; break; } else if (bi < MAX_LEX-1) buf[bi++] = (char)ch; }
                else {
                    if (bi < MAX_LEX-1) buf[bi++] = (char)ch;
                }
            }
            buf[bi] = '\0';
            if (!closed) add_symbol(buf, T_LEX_ERROR, start_line, start_col);
            else add_symbol(buf, T_COLLECTION, start_line, start_col);
            continue;
        }

        /* If starts with digit => could be INT, FLOAT, TIME, DATE, TIMESTAMP */
        if (isdigit(c)) {
            char buf[MAX_LEX];
            int bi = 0;
            int ch = c;
            buf[bi++] = (char)c;

            // collect digits, :, -, ., space as possible (stop on other chars)
            while ((ch = peekch()) != EOF && (isdigit(ch) || ch == '.' || ch == ':' || ch == '-' || ch == ' ')) {
                ch = getch();
                if (bi < MAX_LEX - 1) buf[bi++] = (char)ch;
            }
            buf[bi] = '\0';

            // Now determine classification
            // Trim trailing spaces
            int end = bi - 1;
            while (end >= 0 && isspace((unsigned char)buf[end])) { buf[end] = '\0'; end--; }

            // If contains '-' and looks like YYYY-MM-DD possibly followed by space+time -> DATE or TIMESTAMP
            if (strchr(buf, '-') != NULL && looks_like_date_iso(buf)) {
                // check if there is a space + time part -> timestamp
                char *sp = strchr(buf, ' ');
                if (sp != NULL) {
                    // left is date, right is time -> TIMESTAMP
                    char left[64]; left[0] = '\0';
                    char right[128]; right[0] = '\0';
                    size_t li = sp - buf;
                    if (li >= sizeof(left)) li = sizeof(left)-1;
                    strncpy(left, buf, li); left[li] = '\0';
                    strncpy(right, sp+1, sizeof(right)-1); right[sizeof(right)-1] = '\0';
                    if (looks_like_date_iso(left) && looks_like_time(right)) {
                        // Return lexeme as "YYYY-MM-DD HH:MM[:SS]" (we have full buf)
                        add_symbol(buf, T_TIMESTAMP, start_line, start_col);
                        continue;
                    } else {
                        // If not match, fallback to lex error or date if date part ok
                        if (looks_like_date_iso(left)) {
                            add_symbol(left, T_DATE, start_line, start_col);
                            // put back remainder (right) into stream character by character
                            for (int i = (int)strlen(right)-1; i >= 0; --i) ungetch(right[i]);
                            continue;
                        } else {
                            add_symbol(buf, T_LEX_ERROR, start_line, start_col);
                            continue;
                        }
                    }
                } else {
                    // plain date
                    add_symbol(buf, T_DATE, start_line, start_col);
                    continue;
                }
            }

            // If contains ':' it's a TIME (HH:MM or HH:MM:SS)
            if (strchr(buf, ':') != NULL && looks_like_time(buf)) {
                add_symbol(buf, T_TIME, start_line, start_col);
                continue;
            }

            // If contains '.' => float
            if (strchr(buf, '.') != NULL) {
                // ensure there's at least one digit before/after dot
                add_symbol(buf, T_FLOAT, start_line, start_col);
                continue;
            }

            // Otherwise integer
            add_symbol(buf, T_INT, start_line, start_col);
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

            /* Recognize boolean literals explicitly */
            if (is_bool_literal(low)) {
                add_symbol(buf, T_BOOL, start_line, start_col);
                continue;
            }

            /* Recognize datatypes as their own token (declaration sites) */
            if (is_datatype(low)) {
                add_symbol(buf, T_DATATYPE, start_line, start_col);
                continue;
            }

            /* Use lookup for keywords/reserved/noise */
            int kclass = lookupKeyword(low); // returns 0=none,1=keyword,2=reserved,3=noise (assumed)
            if (kclass == 1) add_symbol(buf, T_KEYWORD, start_line, start_col);
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

        // UNARY ++ -- (two-char) 
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

        /* LOGICAL */
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
        /* First, treat multiplication, division, modulo, integer division ~, percent, etc. */
        if (c == '*' || c == '%' || c == '~') {
            char t[2] = {(char)c, '\0'};
            add_symbol(t, T_ARITH_OP, start_line, start_col);
            continue;
        }

        // For '/': handled above in comment block, but keep fallback
        if (c == '/') {
            add_symbol("/", T_ARITH_OP, start_line, start_col);
            continue;
        }

        /* PLUS and MINUS: decide unary vs binary */
        if (c == '+' || c == '-') {
            /* ++/-- already handled above */
            bool unary = prev_allows_unary();
            char t[2] = {(char)c, '\0'};
            if (unary) add_symbol(t, T_UNARY_OP, start_line, start_col);
            else add_symbol(t, T_ARITH_OP, start_line, start_col);
            continue;
        }

        /* DELIMITERS */
        if (c == ':') { add_symbol(":", T_COLON, start_line, start_col); continue; }
        if (c == ',') { add_symbol(",", T_COMMA, start_line, start_col); continue; }
        if (c == '(') { add_symbol("(", T_LPAREN, start_line, start_col); continue; }
        if (c == ')') { add_symbol(")", T_RPAREN, start_line, start_col); continue; }
        if (c == '[') { add_symbol("[", T_LBRACKET, start_line, start_col); continue; }
        if (c == ']') { add_symbol("]", T_RBRACKET, start_line, start_col); continue; }

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
