/* nslibc/ctype.h: clasificacion pura en C (sin CRT). */
#ifndef F32NS_CTYPE_H
#define F32NS_CTYPE_H

int tolower(int c);
int toupper(int c);

#define ns_isdigit(c) ((c) >= '0' && (c) <= '9')
#define ns_isalpha(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define ns_isalnum(c) (ns_isalpha(c) || ns_isdigit(c))
#define ns_isxdigit(c) (ns_isdigit(c) || ((c) >= 'A' && (c) <= 'F') || ((c) >= 'a' && (c) <= 'f'))
#define ns_isupper(c) ((c) >= 'A' && (c) <= 'Z')
#define ns_islower(c) ((c) >= 'a' && (c) <= 'z')
#define ns_isspace(c) ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\v' || (c) == '\f')

#define isdigit(c) ns_isdigit(c)
#define isalpha(c) ns_isalpha(c)
#define isalnum(c) ns_isalnum(c)
#define isxdigit(c) ns_isxdigit(c)
#define isupper(c) ns_isupper(c)
#define islower(c) ns_islower(c)
#define isspace(c) ns_isspace(c)

#endif
