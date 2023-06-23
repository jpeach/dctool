#ifndef __UTILS_H__
#define __UTILS_H__

void log_error( const char * prefix );

/* wsa_initialize starts the Windows sockets subsystem (if necessary). It
 * exits with a fatal error if that fails.
 */
void wsa_initialize(void);

#endif /* __UTILS_H__ */
