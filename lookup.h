// DFA keyword, reserved word, noise word matcher for SIMPLE language 
// Returns: 0 = NOT A KEYWORD, 1 = KEYWORD, 2 = RESERVED, 3 = NOISE

#ifndef LOOKUP_H
#define LOOKUP_H

#include <string.h>
#include <ctype.h>

// helper: lowercase a copy of s into buf (buf must be large enough)
static void tolower_copy(const char *s, char *buf, int bufsize) {
    int i = 0;
    while (s[i] != '\0' && i < bufsize-1) {
        buf[i] = (char) tolower((unsigned char)s[i]);
        i++;
    }
    buf[i] = '\0';
}

static int lookupKeyword(const char *s_in) {
    char s[256];
    tolower_copy(s_in, s, sizeof(s));

    // index into s
    int i = 0;
    char c = s[i];

    if (c == '\0') return 0;

    // Start state: branch on first character
    switch (c) {
        case 'l': goto Q_L0;
        case 's': goto Q_S0;
        case 'i': goto Q_I0;
        case 'f': goto Q_F0;
        case 'c': goto Q_C0;
        case 't': goto Q_T0;
        case 'b': goto Q_B0;
        case 'd': goto Q_D0;
        case 'a': goto Q_A0;
        case 'g': goto Q_G0;
        case 'e': goto Q_E0;
        case 'n': goto Q_N0;
        case 'r': goto Q_R0;
        case 'h': goto Q_H0;
        case 'o': goto Q_O0;
        case 'm': goto Q_M0;
        case 'p': goto Q_P0;
        default: return 0;
    }

    // ---------- LET / LOCAL ----------
Q_L0:
    // s[0] == 'l'
    i = 1; c = s[i];
    if (c == 'e') goto Q_LE1;   // let
    if (c == 'o') goto Q_LO1;   // local
    return 0;

Q_LE1:
    i = 2; c = s[i];
    if (c == 't') { // "let"
        i = 3;
        if (s[i] == '\0') return 1;
    }
    return 0;

Q_LO1:
    i = 2; c = s[i];
    if (c == 'c') goto Q_LOC2;
    return 0;

Q_LOC2:
    i = 3; c = s[i];
    if (c == 'a') goto Q_LOCA3;
    return 0;

Q_LOCA3:
    i = 4; c = s[i];
    if (c == 'l') {
        i = 5;
        if (s[i] == '\0') return 1; // "local"
    }
    return 0;

    // ---------- STORE / SHOW / STRING / SECURE / SYSTEM ----------
Q_S0:
    // s[0] == 's'
    i = 1; c = s[i];
    if (c == 't') goto Q_ST1;   // store, string, sth...
    if (c == 'h') goto Q_SH1;   // show
    if (c == 'e') goto Q_SE1;   // secure
    if (c == 'y') goto Q_SY2;   // system  <-- FIX: handle "sy..." branch here
    return 0;

Q_ST1:
    i = 2; c = s[i];
    if (c == 'o') goto Q_STO2;  // store, stoi...
    if (c == 'r') goto Q_STR2;  // string (s t r ...)
    return 0;

Q_STO2:
    i = 3; c = s[i];
    if (c == 'r') goto Q_STOR3;
    return 0;

Q_STOR3:
    i = 4; c = s[i];
    if (c == 'e') {
        i = 5;
        if (s[i] == '\0') return 1; // "store"
    }
    return 0;

Q_STR2:
    i = 3; c = s[i];
    if (c == 'i') goto Q_STRI3;
    return 0;

Q_STRI3:
    i = 4; c = s[i];
    if (c == 'n') goto Q_STRIN4;
    return 0;

Q_STRIN4:
    i = 5; c = s[i];
    if (c == 'g') {
        i = 6;
        if (s[i] == '\0') return 1; // "string"
    }
    return 0;

Q_SH1:
    i = 2; c = s[i];
    if (c == 'o') goto Q_SHO2;
    return 0;

Q_SHO2:
    i = 3; c = s[i];
    if (c == 'w') {
        i = 4;
        if (s[i] == '\0') return 1; // "show"
    }
    return 0;

Q_SE1:
    i = 2; c = s[i];
    if (c == 'c') goto Q_SEC2; // secure
    return 0;

Q_SEC2:
    i = 3; c = s[i];
    if (c == 'u') goto Q_SECU3;
    return 0;

Q_SECU3:
    i = 4; c = s[i];
    if (c == 'r') goto Q_SECUR4;
    return 0;

Q_SECUR4:
    i = 5; c = s[i];
    if (c == 'e') {
        i = 6;
        if (s[i] == '\0') return 1; // "secure"
    }
    return 0;

Q_SY2:
    i = 2; c = s[i];
    if (c == 's') goto Q_SYS3;
    return 0;

Q_SYS3:
    i = 3; c = s[i];
    if (c == 't') goto Q_SYST4;
    return 0;

Q_SYST4:
    i = 4; c = s[i];
    if (c == 'e') goto Q_SYSTE5;
    return 0;

Q_SYSTE5:
    i = 5; c = s[i];
    if (c == 'm') {
        i = 6;
        if (s[i] == '\0') return 2; // "system" reserved
    }
    return 0;

    // ---------- INT / IF ----------
Q_I0:
    i = 1; c = s[i];
    if (c == 'n') goto Q_IN1;
    if (c == 'f') { // "if"
        i = 2;
        if (s[i] == '\0') return 1;
    }
    return 0;

Q_IN1:
    i = 2; c = s[i];
    if (c == 't') {
        i = 3;
        if (s[i] == '\0') return 1; // "int"
    }
    return 0;

    // ---------- FLOAT / FOR ----------
Q_F0:
    i = 1; c = s[i];
    if (c == 'l') goto Q_FL1; // float
    if (c == 'o') goto Q_FO1; // for
    return 0;

Q_FL1:
    i = 2; c = s[i];
    if (c == 'o') goto Q_FLO2;
    return 0;

Q_FLO2:
    i = 3; c = s[i];
    if (c == 'a') goto Q_FLOA3;
    return 0;

Q_FLOA3:
    i = 4; c = s[i];
    if (c == 't') {
        i = 5;
        if (s[i] == '\0') return 1; // "float"
    }
    return 0;

Q_FO1:
    i = 2; c = s[i];
    if (c == 'r') {
        i = 3;
        if (s[i] == '\0') return 2; // "for" reserved
    }
    return 0;

    // ---------- CHAR / COLLECTION ----------
Q_C0:
    i = 1; c = s[i];
    if (c == 'h') goto Q_CH1;      // char
    if (c == 'o') goto Q_CO1;      // collection
    return 0;

Q_CH1:
    i = 2; c = s[i];
    if (c == 'a') goto Q_CHA2;
    return 0;

Q_CHA2:
    i = 3; c = s[i];
    if (c == 'r') {
        i = 4;
        if (s[i] == '\0') return 1; // "char"
    }
    return 0;

Q_CO1:
    i = 2; c = s[i];
    if (c == 'l') goto Q_COL2;
    return 0;

Q_COL2:
    i = 3; c = s[i];
    if (c == 'l') goto Q_COLL3;
    return 0;

Q_COLL3:
    i = 4; c = s[i];
    if (c == 'e') goto Q_COLLE4;
    return 0;

Q_COLLE4:
    i = 5; c = s[i];
    if (c == 'c') goto Q_COLLEC5;
    return 0;

Q_COLLEC5:
    i = 6; c = s[i];
    if (c == 't') goto Q_COLLECT6;
    return 0;

Q_COLLECT6:
    i = 7; c = s[i];
    if (c == 'i') goto Q_COLLECTI7;
    return 0;

Q_COLLECTI7:
    i = 8; c = s[i];
    if (c == 'o') goto Q_COLLECTIO8;
    return 0;

Q_COLLECTIO8:
    i = 9; c = s[i];
    if (c == 'n') {
        i = 10;
        if (s[i] == '\0') return 1; // "collection"
    }
    return 0;

    // ---------- TEXT / TIME / TIMESTAMP / TRY / TO / THEN ----------
Q_T0:
    i = 1; c = s[i];
    if (c == 'e') goto Q_TE1;   // text, temp...
    if (c == 'i') goto Q_TI1;   // time, timestamp
    if (c == 'r') goto Q_TR1;   // try
    if (c == 'o') goto Q_TO1;   // to  (noise)
    if (c == 'h') goto Q_TH1;   // then (t h e n) noise
    return 0;

Q_TO1:
    i = 2; 
    c = s[i];

    // Case 1: "to" alone → NOISE (3)
    if (c == '\0') return 3;

    // Case 2: expect space → beginning of "to do"
    if (c == ' ') goto Q_TODO_SPACE;

    return 0;

Q_TODO_SPACE:
    i = 3;
    c = s[i];
    if (c == 'd') goto Q_TODO_D;
    return 0;

Q_TODO_D:
    i = 4;
    c = s[i];
    if (c == 'o') goto Q_TODO_O;
    return 0;

Q_TODO_O:
    i = 5;
    if (s[i] == '\0') return 1;   // "to do" → KEYWORD
    return 0;


Q_TE1:
    i = 2; c = s[i];
    if (c == 'x') goto Q_TEX2; // text
    return 0;

Q_TEX2:
    i = 3; c = s[i];
    if (c == 't') {
        i = 4;
        if (s[i] == '\0') return 1; // "text"
    }
    return 0;

Q_TI1:
    i = 2; c = s[i];
    if (c == 'm') goto Q_TIM2; // time, timestamp
    return 0;

Q_TIM2:
    i = 3; c = s[i];
    if (c == 'e') {
        i = 4;
        if (s[i] == '\0') return 1; // "time"
        if (s[i] == 's') goto Q_TIMESTAMP_S4;
        return 0;
    }
    return 0;

Q_TIMESTAMP_S4:
    i = 4; c = s[i];
    if (c != 's') return 0;
    i = 5; c = s[i]; if (c != 't') return 0;
    i = 6; c = s[i]; if (c != 'a') return 0;
    i = 7; c = s[i]; if (c != 'm') return 0;
    i = 8; c = s[i]; if (c != 'p') return 0;
    i = 9; c = s[i];
    if (c == '\0') return 1; // "timestamp"
    return 0;

Q_TR1:
    i = 2; c = s[i];
    if (c == 'y') {
        i = 3;
        if (s[i] == '\0') return 1; // "try"
    }
    return 0;

Q_TH1:
    i = 2; c = s[i];
    if (c == 'e') goto Q_THE2;
    return 0;

Q_THE2:
    i = 3; c = s[i];
    if (c == 'n') {
        i = 4;
        if (s[i] == '\0') return 3; // "then" noise
    }
    return 0;

    // ---------- BOOL ----------
Q_B0:
    i = 1; c = s[i];
    if (c == 'o') goto Q_BO1;
    return 0;

Q_BO1:
    i = 2; c = s[i];
    if (c == 'o') goto Q_BOO2;
    return 0;

Q_BOO2:
    i = 3; c = s[i];
    if (c == 'l') {
        i = 4;
        if (s[i] == '\0') return 1; // "bool"
    }
    return 0;

    // ---------- DO / DATE ----------
Q_D0:
    i = 1; c = s[i];
    if (c == 'o') {
        i = 2;
        if (s[i] == '\0') return 1; // "do" keyword
        return 0;
    }
    if (c == 'a') goto Q_DA1; // date
    return 0;

Q_DA1:
    i = 2; c = s[i];
    if (c == 't') goto Q_DAT2;
    return 0;

Q_DAT2:
    i = 3; c = s[i];
    if (c == 'e') {
        i = 4;
        if (s[i] == '\0') return 1; // "date"
    }
    return 0;

    // ---------- ARRAY ----------
Q_A0:
    i = 1; c = s[i];
    if (c == 'r') goto Q_AR1;
    return 0;

Q_AR1:
    i = 2; c = s[i];
    if (c == 'r') goto Q_ARR2;
    return 0;

Q_ARR2:
    i = 3; c = s[i];
    if (c == 'a') goto Q_ARRA3;
    return 0;

Q_ARRA3:
    i = 4; c = s[i];
    if (c == 'y') {
        i = 5;
        if (s[i] == '\0') return 1; // "array"
    }
    return 0;

    // ---------- GET / GLOBAL ----------
Q_G0:
    i = 1; c = s[i];
    if (c == 'e') goto Q_GE1; // get, global
    return 0;

Q_GE1:
    i = 2; c = s[i];
    if (c == 't') {
        i = 3;
        if (s[i] == '\0') return 1; // "get"
        return 0;
    }
    if (c == 'l') goto Q_GL1; // global
    return 0;

Q_GL1:
    i = 2; c = s[i];
    if (c == 'o') goto Q_GLO2;
    return 0;

Q_GLO2:
    i = 3; c = s[i];
    if (c == 'b') goto Q_GLOB3;
    return 0;

Q_GLOB3:
    i = 4; c = s[i];
    if (c == 'a') goto Q_GLOBA4;
    return 0;

Q_GLOBA4:
    i = 5; c = s[i];
    if (c == 'l') {
        i = 6;
        if (s[i] == '\0') return 1; // "global"
    }
    return 0;

    // ---------- ELSE / END / ERROR ----------
Q_E0:
    i = 1; c = s[i];
    if (c == 'l') goto Q_EL1; // else
    if (c == 'n') goto Q_EN1; // end
    if (c == 'r') goto Q_ER1; // error
    return 0;

Q_EL1:
    i = 2; c = s[i];
    if (c == 's') goto Q_ELS2;
    return 0;

Q_ELS2:
    i = 3; c = s[i];
    if (c == 'e') {
        i = 4;
        if (s[i] == '\0') return 1; // "else"
    }
    return 0;

Q_EN1:
    i = 2; c = s[i];
    if (c == 'd') {
        i = 3;
        if (s[i] == '\0') return 1; // "end"
    }
    return 0;

Q_ER1:
    i = 2; c = s[i];
    if (c == 'r') goto Q_ERR2;
    return 0;

Q_ERR2:
    i = 3; c = s[i];
    if (c == 'o') goto Q_ERRO3;
    return 0;

Q_ERRO3:
    i = 4; c = s[i];
    if (c == 'r') {
        i = 5;
        if (s[i] == '\0') return 2; // "error" reserved
    }
    return 0;

    // ---------- NEXT / NULL ----------
Q_N0:
    i = 1; c = s[i];
    if (c == 'e') goto Q_NE1; // next
    if (c == 'u') goto Q_NU1; // null
    return 0;

Q_NE1:
    i = 2; c = s[i];
    if (c == 'x') goto Q_NEX2;
    return 0;

Q_NEX2:
    i = 3; c = s[i];
    if (c == 't') {
        i = 4;
        if (s[i] == '\0') return 1; // "next"
    }
    return 0;

Q_NU1:
    i = 2; c = s[i];
    if (c == 'l') goto Q_NUL2;
    return 0;

Q_NUL2:
    i = 3; c = s[i];
    if (c == 'l') {
        i = 4;
        if (s[i] == '\0') return 2; // "null" reserved
    }
    return 0;

    // ---------- RETURN ----------
Q_R0:
    i = 1; c = s[i];
    if (c == 'e') goto Q_RE1;
    return 0;

Q_RE1:
    i = 2; c = s[i];
    if (c == 't') goto Q_RET2;
    return 0;

Q_RET2:
    i = 3; c = s[i];
    if (c == 'u') goto Q_RETU3;
    return 0;

Q_RETU3:
    i = 4; c = s[i];
    if (c == 'r') goto Q_RETUR4;
    return 0;

Q_RETUR4:
    i = 5; c = s[i];
    if (c == 'n') {
        i = 6;
        if (s[i] == '\0') return 1; // "return"
    }
    return 0;

    // ---------- HANDLE ----------
Q_H0:
    i = 1; c = s[i];
    if (c == 'a') goto Q_HA1;
    return 0;

Q_HA1:
    i = 2; c = s[i];
    if (c == 'n') goto Q_HAN2;
    return 0;

Q_HAN2:
    i = 3; c = s[i];
    if (c == 'd') goto Q_HAND3;
    return 0;

Q_HAND3:
    i = 4; c = s[i];
    if (c == 'l') goto Q_HANDL4;
    return 0;

Q_HANDL4:
    i = 5; c = s[i];
    if (c == 'e') {
        i = 6;
        if (s[i] == '\0') return 1; // "handle"
    }
    return 0;

    // ---------- OBJECT (reserved) ----------
Q_O0:
    i = 1; c = s[i];
    if (c == 'b') goto Q_OB1;
    return 0;

Q_OB1:
    i = 2; c = s[i];
    if (c == 'j') goto Q_OBJ2;
    return 0;

Q_OBJ2:
    i = 3; c = s[i];
    if (c == 'e') goto Q_OBJE3;
    return 0;

Q_OBJE3:
    i = 4; c = s[i];
    if (c == 'c') goto Q_OBJEC4;
    return 0;

Q_OBJEC4:
    i = 5; c = s[i];
    if (c == 't') {
        i = 6;
        if (s[i] == '\0') return 2; // "object" reserved
    }
    return 0;

    // ---------- MAIN (reserved) ----------
Q_M0:
    i = 1; c = s[i];
    if (c == 'a') goto Q_MA1;
    return 0;

Q_MA1:
    i = 2; c = s[i];
    if (c == 'i') goto Q_MAI2;
    return 0;

Q_MAI2:
    i = 3; c = s[i];
    if (c == 'n') {
        i = 4;
        if (s[i] == '\0') return 2; // "main" reserved
    }
    return 0;

    // ---------- PLEASE (noise) ----------
Q_P0:
    i = 1; c = s[i];
    if (c == 'l') goto Q_PL1;
    return 0;

Q_PL1:
    i = 2; c = s[i];
    if (c == 'e') goto Q_PLE2;
    return 0;

Q_PLE2:
    i = 3; c = s[i];
    if (c == 'a') goto Q_PLEA3;
    return 0;

Q_PLEA3:
    i = 4; c = s[i];
    if (c == 's') goto Q_PLEAS4;
    return 0;

Q_PLEAS4:
    i = 5; c = s[i];
    if (c == 'e') {
        i = 6;
        if (s[i] == '\0') return 3; // "please" noise
    }
    return 0;

    // fallback
    return 0;
}

#endif // LOOKUP_H
