/*
 * Copyright (C) 2013-2014 Jolla Ltd.
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
#include "mms_codec.h"

typedef struct test_desc {
    const char* name;
    const char* input;
    const char* output;
    char** parsed;
} TestDesc;

static const char* parsed_basic[] =
    { "text/html", "charset", "ISO-8859-4", NULL };
static const char* parsed_quotes[] =
    { "application/octet-stream", "foo", " quoted \"text\" ", NULL};
static const char* parsed_parameters[] =
    { "type/subtype", "p1", "v1", "p2", "v2", NULL };

static const TestDesc media_type_tests[] = {
    {
        "Basic",
        "text/html; charset=ISO-8859-4",
        "text/html; charset=ISO-8859-4",
        (char**)parsed_basic
    },{
        "Spaces",
        " text/html ;\tcharset = ISO-8859-4\n",
        "text/html; charset=ISO-8859-4",
        (char**)parsed_basic
    },{
        "Quotes",
        "application/octet-stream; foo = \"\\ quoted \\\"text\\\" \"",
        "application/octet-stream; foo=\" quoted \\\"text\\\" \"",
        (char**)parsed_quotes
    },{
        "Parameters",
        "type/subtype; p1=v1 ; p2=\"v2\"",
        "type/subtype; p1=v1; p2=v2",
        (char**)parsed_parameters
    },{
        "MissingSubtype",
        "type",
        NULL,
        NULL
    },{
        "MissingParameter",
        "type/subtype; ",
        NULL,
        NULL
    }
};

static
gboolean
run_test(
    const TestDesc* test)
{
    gboolean ok = FALSE;
    char** parsed = mms_parse_http_content_type(test->input);
    if (parsed) {
        if (test->output) {
            char* unparsed = mms_unparse_http_content_type(parsed);
            if (!strcmp(unparsed, test->output)) {
                char** p1 = parsed;
                char** p2 = test->parsed;
                ok = TRUE;
                while (*p1 && ok) {
                    if (*p2) {
                        ok = !strcmp(*p1++, *p2++);
                    } else {
                        ok = FALSE;
                        break;
                    }
                }
                if (*p2) ok = FALSE;
            }
        }
    } else if (!test->output) {
        /* Test is expected to fail */
        ok = TRUE;
    }
    g_strfreev(parsed);
    MMS_INFO("%s: %s", ok ? "OK" : "FAILED", test->name);
    return ok;
}

static
gboolean
run_tests(
    const TestDesc* tests,
    int count)
{
    int i;
    gboolean ok = TRUE;
    for (i=0; i<count; i++) {
        if (!run_test(tests + i)) {
            ok = FALSE;
        }
    }
    return ok;
}

int main(int argc, char* argv[])
{
    mms_log_stdout_timestamp = FALSE;
    mms_log_default.level = MMS_LOGLEVEL_INFO;
    return !run_tests(media_type_tests, G_N_ELEMENTS(media_type_tests));
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
