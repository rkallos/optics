/* poller_lens_test.c
   Rémi Attab (remi.attab@gmail.com), 09 Mar 2016
   FreeBSD-style copyright and disclaimer apply
*/

#include "test.h"


// -----------------------------------------------------------------------------
// backend
// -----------------------------------------------------------------------------

void backend_cb(void *ctx, uint64_t ts, const char *key, double value)
{
    (void) ts;

    struct optics_set *keys = ctx;
    assert_true(optics_set_put(keys, key, value));

    char buffer[1024];
    optics_set_print(keys, buffer, sizeof(buffer));
}


// -----------------------------------------------------------------------------
// gauge
// -----------------------------------------------------------------------------

optics_test_head(poller_gauge_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *gauge = optics_gauge_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    time_t ts = 0;

    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0, make_kv("bleh.blah", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        optics_gauge_set(gauge, 1.0);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0, make_kv("bleh.blah", 1.0));

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0, make_kv("bleh.blah", 1.0));

        optics_gauge_set(gauge, 1.34e-5);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 1e-7, make_kv("bleh.blah", 1.34e-5));
    }

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// counter
// -----------------------------------------------------------------------------

optics_test_head(poller_counter_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *counter = optics_counter_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    time_t ts = 0;

    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0, make_kv("bleh.blah", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        optics_counter_inc(counter, 1);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0, make_kv("bleh.blah", 1.0));

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0, make_kv("bleh.blah", 0.0));

        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(counter, 1.0);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0.0, make_kv("bleh.blah", 10.0));

        for (size_t j = 0; j < 10; ++j)
            optics_counter_inc(counter, -1.0);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0.0, make_kv("bleh.blah", -10.0));
    }

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// dist
// -----------------------------------------------------------------------------

optics_test_head(poller_dist_test)
{
    struct optics *optics = optics_create(test_name);
    struct optics_lens *dist = optics_dist_alloc(optics, "blah");
    optics_set_prefix(optics, "bleh");

    struct optics_set result = {0};
    struct optics_poller *poller = optics_poller_alloc();
    optics_poller_backend(poller, &result, backend_cb, NULL);

    time_t ts = 0;

    optics_poller_poll_at(poller, ++ts);
    assert_set_equal(&result, 0,
            make_kv("bleh.blah.count", 0.0),
            make_kv("bleh.blah.p50", 0.0),
            make_kv("bleh.blah.p90", 0.0),
            make_kv("bleh.blah.p99", 0.0),
            make_kv("bleh.blah.max", 0.0));

    for (size_t i = 0; i < 3; ++i) {
        optics_dist_record(dist, 1.0);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0,
                make_kv("bleh.blah.count", 1.0),
                make_kv("bleh.blah.p50", 1.0),
                make_kv("bleh.blah.p90", 1.0),
                make_kv("bleh.blah.p99", 1.0),
                make_kv("bleh.blah.max", 1.0));

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0,
                make_kv("bleh.blah.count", 0.0),
                make_kv("bleh.blah.p50", 0.0),
                make_kv("bleh.blah.p90", 0.0),
                make_kv("bleh.blah.p99", 0.0),
                make_kv("bleh.blah.max", 0.0));

        for (size_t i = 0; i < 10; ++i)
            optics_dist_record(dist, i + 1);

        optics_set_reset(&result);
        optics_poller_poll_at(poller, ++ts);
        assert_set_equal(&result, 0,
                make_kv("bleh.blah.count", 10.0),
                make_kv("bleh.blah.p50", 6.0),
                make_kv("bleh.blah.p90", 10.0),
                make_kv("bleh.blah.p99", 10.0),
                make_kv("bleh.blah.max", 10.0));
    }

    optics_poller_free(poller);
}
optics_test_tail()


// -----------------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------------

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(poller_gauge_test),
        cmocka_unit_test(poller_counter_test),
        cmocka_unit_test(poller_dist_test),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}