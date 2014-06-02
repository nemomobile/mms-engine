/*
 * Copyright (C) 2014 Jolla Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "mms_log.h"

#define RET_OK  (0)
#define RET_ERR (1)

typedef struct test_desc {
    const char* name;
    MMSLogModule** mods;
    int nmods;
    const int* results; /* nmods elements */
    int default_level;
    char** opts;
    int nopts;
    gboolean test_success;
} TestDesc;

const MMS_LOG_MODULE_DEFINE_(log_defaults,NULL);

/* Test1: default level + 2 modules */
MMS_LOG_MODULE_DEFINE_(mod1a, "test-a");
MMS_LOG_MODULE_DEFINE_(mod1b, "test-b");
static MMSLogModule* mods1[] = { &mod1a, &mod1b };
static const int results1[] = { MMS_LOGLEVEL_DEBUG, MMS_LOGLEVEL_INFO };
static const char* opts1[] = { "verbose", "test-a:debug", "test-b:info" };
G_STATIC_ASSERT(G_N_ELEMENTS(mods1) == G_N_ELEMENTS(results1));

/* Test2: wrong module name */
MMS_LOG_MODULE_DEFINE_(mod2, "test-a");
static MMSLogModule* mods2[] = { &mod2 };
static const int results2[] = { MMS_LOGLEVEL_GLOBAL };
static const char* opts2[] = { "test-b:debug" };

/* Test3: one option overriding another */
MMS_LOG_MODULE_DEFINE_(mod3, "test");
static MMSLogModule* mods3[] = { &mod3 };
static const int results3[] = { MMS_LOGLEVEL_DEBUG };
static const char* opts3[] = { "test:verbose", "test:debug" };
G_STATIC_ASSERT(G_N_ELEMENTS(mods3) == G_N_ELEMENTS(results3));

/* Test4: log levels as numbers */
MMS_LOG_MODULE_DEFINE_(mod4a, "a");
MMS_LOG_MODULE_DEFINE_(mod4b, "b");
MMS_LOG_MODULE_DEFINE_(mod4c, "c");
MMS_LOG_MODULE_DEFINE_(mod4d, "d");
MMS_LOG_MODULE_DEFINE_(mod4e, "e");
MMS_LOG_MODULE_DEFINE_(mod4f, "f");
static MMSLogModule* mods4[] = { &mod4a,&mod4b,&mod4c,&mod4d,&mod4e,&mod4f };
static const int results4[] = { MMS_LOGLEVEL_NONE, MMS_LOGLEVEL_ERR,
  MMS_LOGLEVEL_WARN, MMS_LOGLEVEL_INFO, MMS_LOGLEVEL_DEBUG,
  MMS_LOGLEVEL_VERBOSE };
static const char* opts4[] = { "a:0", "b:1", "c:2", "d:3", "e:4", "f:5" };
G_STATIC_ASSERT(G_N_ELEMENTS(mods4) == G_N_ELEMENTS(results4));

/* Test5: invalid number */
MMS_LOG_MODULE_DEFINE_(mod5, "test");
static MMSLogModule* mods5[] = { &mod5 };
static const int results5[] = { MMS_LOGLEVEL_GLOBAL };
static const char* opts5[] = { "test-b:66" };

#define ARRAY(a) (a), G_N_ELEMENTS(a)

static const TestDesc log_tests[] = {
    { "Test1",  ARRAY(mods1), results1, MMS_LOGLEVEL_VERBOSE,
      (char**)ARRAY(opts1), TRUE },
    { "Test2",  ARRAY(mods2), results2, MMS_LOGLEVEL_DEFAULT,
      (char**)ARRAY(opts2), FALSE },
    { "Test3",  ARRAY(mods3), results3, MMS_LOGLEVEL_DEFAULT,
      (char**)ARRAY(opts3), TRUE },
    { "Test4",  ARRAY(mods4), results4, MMS_LOGLEVEL_DEFAULT,
      (char**)ARRAY(opts4), TRUE },
    { "Test5",  ARRAY(mods5), results5, MMS_LOGLEVEL_DEFAULT,
      (char**)ARRAY(opts5), FALSE },
};

static
gboolean
run_test(
    const TestDesc* test)
{
    int i;
    gboolean ok = FALSE;
    const int prev_default_level = mms_log_default.level;
    int test_default_level;
    mms_log_default.level = MMS_LOGLEVEL_DEFAULT;
    for (i=0; i<test->nmods; i++) {
        test->mods[i]->level = MMS_LOGLEVEL_GLOBAL;
    }
    for (i=0; i<test->nopts; i++) {
        const char* opt = test->opts[i];
        GError* error = NULL;
        if (!mms_log_parse_option(opt, test->mods, test->nmods, &error)) {
            g_error_free(error);
            break;
        }
    }
    test_default_level = mms_log_default.level;
    mms_log_default.level = prev_default_level;
    if (test_default_level == test->default_level && i == test->nopts) {
        char* desc = mms_log_description(test->mods, test->nmods);
        MMS_DEBUG("%s", desc);
        g_free(desc);
        ok = TRUE;
        for (i=0; i<test->nmods; i++) {
            const int expect = test->results[i];
            const MMSLogModule* mod = test->mods[i];
            if (mod->level != expect) {
                ok = FALSE;
                if (test->test_success) {
                    MMS_ERR("%s mismatch: %d vs %d", mod->name,
                        mod->level, expect);
                }
            }
        }
        if (ok && !test->test_success) {
            MMS_ERR("%s is expected to fail", test->name);
            ok = FALSE;
        }
    } else if (!test->test_success) {
        /* Test is expected to fail */
        ok = TRUE;
    } else if (test_default_level != test->default_level) {
        MMS_ERR("%s default log level mismatch: %d vs %d", test->name,
            test_default_level, test->default_level);
    } else {
        MMS_ERR("%s error parsing %s", test->name, test->opts[i]);
    }
    MMS_INFO("%s: %s", ok ? "OK" : "FAILED", test->name);
    return ok;
}

static
gboolean
run_named_test(
    const char* name,
    const TestDesc* tests,
    int count)
{
    int i;
    for (i=0; i<count; i++) {
        if (!strcmp(tests[i].name, name)) {
            return run_test(tests + i);
        }
    }
    MMS_ERR("Unknown test: %s", name);
    return FALSE;
}

static
int
run_tests(
    const TestDesc* tests,
    int count)
{
    int i, ret = RET_OK;
    for (i=0; i<count; i++) {
        if (!run_test(tests + i)) {
            ret = RET_ERR;
        }
    }
    return ret;
}

int main(int argc, char* argv[])
{
    int ret = RET_OK;
    mms_log_set_type(MMS_LOG_TYPE_STDOUT, "test_mms_log");
    mms_log_stdout_timestamp = FALSE;
    mms_log_default.level = MMS_LOGLEVEL_INFO;
    if (argc == 1) {
        MMS_DEBUG("log type %s", mms_log_get_type());
        ret = run_tests(log_tests, G_N_ELEMENTS(log_tests));
    } else {
        int i;
        for (i=1; i<argc; i++) {
            if (!run_named_test(argv[i], log_tests, G_N_ELEMENTS(log_tests))) {
                ret = RET_ERR;
            }
        }
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
