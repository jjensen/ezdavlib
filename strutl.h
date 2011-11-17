#ifndef __STRUTL_H__
#define __STRUTL_H__

char *strndup(const char *s, size_t len);
char *strnunqdup(const char *s, size_t len);
int strchrpos(const char *s, int c);
int strchrqpos(const char *s, int c);
void strclrws(const char **s);
char *strdup_url_encoded(const char *string);
char *strdup_url_decoded(const char *string);
char *strdup_base64(const char *string);

#ifdef WIN32
#define strcasecmp	_stricmp
#define strncasecmp _strnicmp
#endif

#ifndef strdup
#define __NO__STRDUP__
char *strdup(const char *s);
#endif

#endif