#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strmanip.h"

#define MAXMATCH        10

static unsigned int substitute_internal(char* source, regex_t* prog,
  char* result, char *sub, int global);

char* substitute_re(char *source, unsigned int sourcelen, char *regex,
  char* sub, int ignore_case, int global, int *found, unsigned int *newlen)
    {
    char        errbuf[512];    /* Buffer to hold error messages */
    int         error;          /* Error code from regex functions */
    char *      new_source;     /* Copy of source with null terminator */
    regex_t     prog;           /* Compiled regular expression */
    char*       result;         /* Calculated result */
    unsigned int result_len;    /* Length of substituted result */

    /*
     * If the source contains any internal null bytes, assume that it's
     * binary and just return a copy.
     */
    if (memchr(source, '\0', sourcelen) != NULL)
        {
        if (newlen != NULL)
            *newlen = sourcelen;
        result = malloc(sourcelen);
        if (result == NULL)
            {
            (void) fprintf(stderr,
              "Regular expression error: couldn't allocate space "
                "for %u result bytes\n",
              sourcelen);
            exit(1);
            }
        memcpy(result, source, sourcelen);
        if (found != NULL)
            *found = 0;
        return result;
        }
    error = regcomp(&prog, regex,
      ignore_case ? REG_EXTENDED | REG_ICASE : REG_EXTENDED);
    if (error)
        {
        /*
         * We print and exit here, rather than returning NULL, so that
         * students won't miss the problem and waste debugging time.
         */
        regerror(error, &prog, errbuf, sizeof errbuf);
        (void) fprintf(stderr, "Regular expression error: %s\n", errbuf);
        exit(1);
        }

    /*
     * The regular-expression functions assume a null-terminated string,
     * so we have to arrange that.
     */
    new_source = malloc(sourcelen + 1);
    if (new_source == NULL)
        {
        (void) fprintf(stderr,
          "Regular expression error: couldn't allocate space "
            "for %u source bytes\n",
          sourcelen + 1);
        exit(1);
        }
    memcpy(new_source, source, sourcelen);
    new_source[sourcelen] = '\0';
    /*
     * Figure out how long the result needs to be by doing the substitution
     * in "trial" mode.
     */
    result_len = substitute_internal(new_source, &prog, NULL, sub, global);
    if (result_len == -1)
        {
        /*
         * No match; return the malloc'ed copy of the original.
         */
        regfree(&prog);
        if (newlen != NULL)
            *newlen = sourcelen;
        return new_source;
        }

    /*
     * Make space for the result.
     */
    result = malloc(result_len + 1);    /* +1 for the null byte */
    if (result == NULL)
        {
        (void) fprintf(stderr,
          "Regular expression error: couldn't allocate space "
            "for %u result bytes\n",
          result_len + 1);
        exit(1);
        }

    /*
     * Do the actual substitution.  We already know it will succeed.
     */
    substitute_internal(new_source, &prog, result, sub, global);
    regfree(&prog);
    free(new_source);
    if (newlen != NULL)
        *newlen = result_len;
    if (found != NULL)
        *found = 1;
    return result;
    }

static unsigned int substitute_internal(char* source, regex_t* prog,
  char* result, char *sub, int global)
    {
    int         error;          /* Error code from regex functions */
    int         group_no;       /* Number of current match group */
    regmatch_t  match[MAXMATCH]; /* List of matched subexpressions */
    unsigned int result_len = 0;        /* Length of result */
    char*       subp;           /* Pointer into substitution string */

    do
        {
        error = regexec(prog, source, MAXMATCH, match, 0);
        if (error != 0)
            {
            if (result_len == 0)
                {
                /*
                 * No match; return -1 to indicate that fact.  The
                 * caller can stick with the original string if they
                 * so desire.
                 */
                return -1;
                }
            else
                {
                /*
                 * No match, but there were previous matches (due to
                 * global being true).  We're done.
                 */
                break;
                }
            }
        /*
         * We successfully matched the regular expression.  Copy everything
         * prior to the match into the result.
         */
        if (result != NULL)
            memcpy(result + result_len, source, match[0].rm_so);

        result_len += match[0].rm_so;
        /*
         * Now do the substitution.  Note that this can fail if the
         * substitution refers to sub-matches that don't exist; in
         * that case we return -1 and the result is only partial.
         *
         * Per regex standards, "&" refers to the entire match, as
         * does "\0".  Thus, "\&" generates an ampersand, and of
         * course "\\" produces a backslash.
         */
        for (subp = sub;  *subp != '\0';  subp++)
            {
            if (*subp == '&')
                {
                if (result != NULL)
                    memcpy(result + result_len, source + match[0].rm_so,
                      match[0].rm_eo - match[0].rm_so);
                result_len += match[0].rm_eo - match[0].rm_so;
                }
            else if (*subp == '\\'  &&  subp[1] != '\0')
                {
                subp++;
                if (isdigit(*subp))
                    {
                    group_no = *subp - '0';
                    if (match[group_no].rm_so == -1)
                        {
                        /*
                         * Again, we print and exit here to protect students.
                         */
                        (void) fprintf(stderr,
                          "Regular expression error: "
                            "match group %d does not exist\n",
                          group_no);
                        exit(1);
                        }
                    if (result != NULL)
                        memcpy(result + result_len,
                          source + match[group_no].rm_so,
                          match[group_no].rm_eo - match[group_no].rm_so);
                    result_len += match[group_no].rm_eo - match[group_no].rm_so;
                    }
                else if (*subp == '&'  ||  *subp == '\\')
                    {
                    if (result != NULL)
                        result[result_len] = *subp;
                    result_len += 1;
                    }
                else
                    {
                    if (result == NULL)
                        result_len += 2;
                    else
                        {
                        result[result_len++] = '\\';
                        result[result_len++] = *subp;
                        }
                    }
                }
            else
                {
                if (result != NULL)
                    result[result_len] = *subp;
                result_len++;
                }
            }
        /*
         * Skip over the matched section.  If it was empty, skip one
         * character.
         */
        if (match[0].rm_eo == 0)
            source++;
        else
            source += match[0].rm_eo;
        }
      while (global  &&  *source != '\0');
    /*
     * We're done substituting, either because we only match once or
     * because we ran out of matches.  Copy the final bytes.
     */
    if (result != NULL)
        strcpy(result + result_len, source);
    result_len += strlen(source);
    return result_len;
    }

#if 0
/*
 * Test program for substitute_re
 */
int main(int argc, char *argv[])
    {
    char *result;
    if (argc != 5)
        {
        (void) fprintf(stderr, "Usage: strmanip source regex sub global\n");
        return 1;
        }
    result =
      substitute_re(argv[1], argv[2], argv[3], 1, atoi(argv[4]), NULL, NULL);
    if (result == NULL)
        (void) printf("No result!\n");
    else
        (void) printf("Result is '%s'\n", result);
    return 0;
    }
#endif /* 0 */

