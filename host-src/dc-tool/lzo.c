/* This is hacky, but we effectively make a copy the minilzo source here so
 * that parallel builds don't experience errors when they build minilzo for
 * the host and for the target at the same time.
 */
#include <minilzo.c>
