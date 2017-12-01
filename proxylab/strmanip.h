#ifndef _STRMANIP_H
#define _STRMANIP_H

/*
 * Perform regular-expression substitution on a string.
 *
 * In all cases, a malloc'ed string is returned.  It is the caller's
 * responsibility to free the returned memory.  In essence, this
 * function copies the "source" string to the new memory, possibly
 * replacing one or more substrings while the copy is happening.
 *
 * The function guarantees to never generate a buffer overflow.
 *
 * The source string (which is of length sourcelen) is searched for
 * the given extended regular expression (egrep syntax; see "man
 * egrep").  If the RE was not found, "*found" is set to zero and a
 * malloc'ed copy of the original string is returned (note that
 * "found" must be a POINTER to an integer; it is the caller's
 * responsibility to provide that integer).  If the RE was found,
 * "*found" is set nonzero, the string matched by the RE is replaced
 * by "sub" according to the rules below, and the result is returned
 * in the newly malloc'ed string mentioned above.  In all cases the
 * length of the returned string is placed in "*newlen".
 *
 * If the string contains any null bytes (\0), it is assumed to be a
 * binary string and-regular expression replacement is not performed.
 * Note that this means that "source" should NOT be terminated by a
 * null byte; instead its length should be given by "sourcelen".
 * However, for convenience the returned result has a null byte at the
 * end.
 *
 * If "found" and "newlen" are not needed, NULL pointers can be given for
 * either of these arguments.
 *
 * In all cases, it is the caller's responsibility to free the
 * returned string.  BEWARE: it is easy to forget to free the string
 * and wind up with memory leaks!
 *
 * If "ignore_case" is nonzero, case is ignored when matching the RE.
 *
 * If "global" is zero, only the first-found match for the RE is
 * replaced.  if "global" is nonzero, every (non-overlapping) match is
 * replaced.
 *
 * Certain characters in the substitution string are treated specially:
 *
 *      &       is replaced by the matched string.
 *      \0      is replaced by the matched string.
 *      \n      if n = 1-9, is replaced by the nth matched
 *              parenthesized substring.  It is an error to use \n if
 *              there are fewer than n matched subexpressions.
 *      \&      is replaced by &
 *      \\      is replaced by \
 *
 * NOTE: to make the lab simpler to debug, if any errors are found in
 * the regular expression an error message is printed and the program
 * is immediately terminated.
 *
 * Some examples, showing how to chain multiple substitutions:
 *
 *      char* result;
 *      result = substitute_re("Easy", 4, "ea.y", "hard", 1, 1, NULL, NULL);
 *      // result now contains "hard"
 *      free(result);                   // Don't forget to do this!
 *
 *      int newlen;
 *      result = substitute_re("easynoteasy", 8, "easy", "hard", 0, 1,
 *                             NULL, &newlen);
 *      // result now contains "hardnothard"; newlen is 11
 *
 *      char* result2;
 *      result2 = substitute_re(result, newlen, "not", " doesn't mean ", 0, 1,
 *                             NULL, &newlen);
 *      free(result);
 *      // result2 now contains "hard doesn't mean hard"; newlen is 22
 *
 *      result = substitute_re(result2, newlen, "hard", "gentle", 0, 0,
 *                             NULL, &newlen);
 *      free(result2);
 *      // result now contains "gentle doesn't mean hard"; newlen is 24
 *
 *      free(result);
 */
extern char* substitute_re(char* source, unsigned int sourcelen, char* regex,
  char* sub, int ignore_case, int global, int* found, unsigned int* newlen);

#endif /* _STRMANIP_H */
