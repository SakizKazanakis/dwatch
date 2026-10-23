#include "monolith_oem_lifetime_mock.h"
#define KEY "monolith"
int monolith_lifetime_open(monolith_lifetime_state *s,
                           const monolith_lifetime_hooks *h,
                           void *root) {
    if (!s || !h || !root || s->is_open || !h->get_user ||
        !h->view_init || !h->view_deinit || !h->free_user) return -1;
    void *u = h->get_user(KEY,28u);
    if (!u) return -1;
    s->user = u;
    h->view_init(u,root);
    s->is_open = 1;
    return 0;
}
int monolith_lifetime_close(monolith_lifetime_state *s,
                            const monolith_lifetime_hooks *h) {
    if (!s || !h || !s->is_open || !s->user ||
        !h->view_deinit || !h->free_user) return -1;
    h->view_deinit(s->user);
    h->free_user(KEY);
    s->user = 0;
    s->is_open = 0;
    return 0;
}
int monolith_lifetime_cleanup(monolith_lifetime_state *s,
                              const monolith_lifetime_hooks *h) {
    if (!s || !h || s->is_open || !h->destroy_resource_manager ||
        !h->free_resource_manager) return -1;
    if (s->resource_manager) {
        h->destroy_resource_manager(s->resource_manager);
        h->free_resource_manager(s->resource_manager);
        s->resource_manager = 0;
    }
    return 0;
}
