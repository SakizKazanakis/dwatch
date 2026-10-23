#ifndef MONOLITH_OEM_LIFETIME_MOCK_H
#define MONOLITH_OEM_LIFETIME_MOCK_H
/* OFFLINE MODEL ONLY. C prototypes are not the OEM firmware headers. */
typedef struct {
    void *(*get_user)(const char *key, unsigned int bytes);
    void (*view_init)(void *user, void *root);
    void (*view_deinit)(void *user);
    void (*free_user)(const char *key);
    void (*destroy_resource_manager)(void *manager);
    void (*free_resource_manager)(void *manager);
} monolith_lifetime_hooks;
typedef struct {
    void *user;
    void *resource_manager;
    unsigned int is_open;
} monolith_lifetime_state;
/* Returns 0 on successful simulated event0, -1 on bad preconditions. */
int monolith_lifetime_open(monolith_lifetime_state *s,
                           const monolith_lifetime_hooks *h,
                           void *root);
/* Event3: deinitialize user views before releasing userdata. */
int monolith_lifetime_close(monolith_lifetime_state *s,
                            const monolith_lifetime_hooks *h);
/* module_cleanup: refuses cleanup with an active view. */
int monolith_lifetime_cleanup(monolith_lifetime_state *s,
                              const monolith_lifetime_hooks *h);
#endif
