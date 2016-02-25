/* region.c
   Rémi Attab (remi.attab@gmail.com), 25 Feb 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "region.h"


// -----------------------------------------------------------------------------
// config
// -----------------------------------------------------------------------------

static const size_t region_header_len = page_len;
static const size_t region_max_len = page_len * 1000 * 1000;


// -----------------------------------------------------------------------------
// utils
// -----------------------------------------------------------------------------

static bool make_shm_name(const char *name, char *dest, size_t dest_len)
{
    if (snprintf(dest, dest_len, "optics.%s", name) < n) return true;

    optics_fail("region name '%s' too long", name);
    return false;
}


// -----------------------------------------------------------------------------
// struct
// -----------------------------------------------------------------------------

struct region
{
    int fd;
    void *map;
    bool owned;

    atomic_size_t alloc;

    char name[NAME_MAX];
};


// -----------------------------------------------------------------------------
// open/close
// -----------------------------------------------------------------------------

static bool region_create(struct region *region, const char *name)
{
    memset(region, 0, sizeof(*region));
    region->owned = true;
    atomic_init(&region->alloc, region_header_len);

    if (!make_shm_name(name, region->name, sizeof(region->name)))
        goto fail_name;

    region->fd = shm_open(region->name, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (!region->fd) {
        optics_fail_errno("unable to create region '%s'", name);
        goto fail_open;
    }

    int ret = ftruncate(region->fd, region_max_len);
    if (ret == -1) {
        optics_fail_errno("unable to resize region '%s' to '%lu'", name, region_max_len);
        goto fail_truncate;
    }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    region->map = mmap(0, region_max_len, prot, flags, region->fd, 0);
    if (region->map == MAP_FAILED) {
        optics_fail_errno("unable to map region '%s'", name);
        goto fail_mmap;
    }

    return true;

    munmap(region->map, region_max_len);

  fail_mmap:
  fail_truncate:
    close(region->fd);
    shm_unlink(region->name);

  fail_open:
  fail_name:
    memset(region, 0, sizeof(*region));
    return false;
}


static bool region_open(struct region *region, const char *name)
{
    memset(region, 0, sizeof(*region));
    region->owned = false;

    if (!make_shm_name(name, region->name, sizeof(region->name)))
        goto fail_name;

    int flags = 0;
    region->fd = shm_open(region->name, O_RDWR, 0);
    if (!region->fd) {
        optics_fail_errno("unable to create region '%s'", name);
        goto fail_open;
    }

    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED;
    region->map = mmap(0, region_max_len, prot, flags, region->fd, 0);
    if (region->map == MAP_FAILED) {
        optics_fail_errno("unable to map region '%s'", name);
        goto fail_mmap;
    }

    return true;

    munmap(region->map, region_max_len);

  fail_mmap:
    close(region->fd);

  fail_open:
  fail_name:
    memset(region, 0, sizeof(*region));
    return false;
}


static bool region_close(struct region *region)
{
    if (munmap(region->map, region_max_len) == -1) {
        optics_fail_errno("unable to unmap region '%s'", region->name);
        return false;
    }

    if (close(region->fd) == -1) {
        optics_fail_errno("unable to close region '%s'", region->name);
        return false;
    }

    if (region->owned) {
        if (shm_unlink(region->name) == -1) {
            optics_fail_errno("unable to unlink region '%s'", region->name);
            return false;
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
// access
// -----------------------------------------------------------------------------

static void * region_ptr(struct region *region, optics_off_t off, size_t len)
{
    if (off + len > region_max_len) {
        optics_fail("out-of-region access in region '%s' at '%p' with len '%lu'",
                region->name, (void *) off, len);
        return NULL;
    }

    return ((uint8_t *) region->map) + off;
}

static optics_off_t region_alloc(struct region *region, size_t len)
{
    if (region->read_only) {
        optics_fail("unable to allocate space in read-only region '%s'", region->name);
        return NULL;
    }

    // align to cache lines to avoid false-sharing.
    len += cache_line_len - (len % cache_line_len);

    optics_off_t off = atomic_fetch_add_explicit(&region->alloc, 1, memory_order_relaxed);
    if (off + len > region_max_len) {
        optics_fail("out-of-space in region '%s' for alloc of size '%lu'", region->name, len);
        return NULL;
    }

    memset(region_ptr(region, off, len), 0, len);
    return off;
}

static void region_free(struct region *region, optics_off_t off, size_t size)
{
    (void) region, (void) off, (void) size;
}
