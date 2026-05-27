
#ifndef TCMG_WEBIF_H_
#define TCMG_WEBIF_H_

/* Start the webif HTTP server in a background pthread.
 * Returns 0 on success, -1 if webif_enabled == 0 or bind fails. */
int32_t webif_start(void);

/* Signal the webif thread to stop and join it.
 * Called from main() on shutdown. */
void    webif_stop(void);

#endif /* TCMG_WEBIF_H_ */
