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

#include "mms_engine.h"
#include "mms_dispatcher.h"
#include "mms_lib_util.h"
#include "mms_ofono_connman.h"
#include "mms_handler_dbus.h"
#include "mms_log.h"

/* Generated code */
#include "org.nemomobile.MmsEngine.h"

#ifdef DEBUG
#  define ENABLE_TEST
#endif

struct mms_engine {
    GObject parent;
    const MMSConfig* config;
    MMSDispatcher* dispatcher;
    MMSDispatcherDelegate dispatcher_delegate;
    MMSLogModule** log_modules;
    int log_count;
    GDBusConnection* engine_bus;
    OrgNemomobileMmsEngine* proxy;
    GMainLoop* loop;
    gboolean stopped;
    gboolean keep_running;
    guint start_timeout_id;
    gulong send_message_id;
    gulong push_signal_id;
    gulong push_notify_signal_id;
    gulong receive_signal_id;
    gulong read_report_signal_id;
    gulong cancel_signal_id;
    gulong set_log_level_signal_id;
    gulong set_log_type_signal_id;
#ifdef ENABLE_TEST
    gulong test_signal_id;
#endif
};

typedef GObjectClass MMSEngineClass;
G_DEFINE_TYPE(MMSEngine, mms_engine, G_TYPE_OBJECT);
#define MMS_ENGINE_TYPE (mms_engine_get_type())
#define MMS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
        MMS_ENGINE_TYPE, MMSEngine))

inline static MMSEngine*
mms_engine_from_dispatcher_delegate(MMSDispatcherDelegate* delegate)
    { return MMS_CAST(delegate,MMSEngine,dispatcher_delegate); }

static
gboolean
mms_engine_stop_callback(
    gpointer data)
{
    MMSEngine* engine = data;
    mms_engine_stop(engine);
    mms_engine_unref(engine);
    return FALSE;
}

static
void
mms_engine_stop_schedule(
    MMSEngine* engine)
{
    g_idle_add(mms_engine_stop_callback, mms_engine_ref(engine));
}

static
gboolean
mms_engine_start_timeout_callback(
    gpointer data)
{
    MMSEngine* engine = data;
    MMS_ASSERT(engine->start_timeout_id);
    MMS_INFO("Shutting down due to inactivity...");
    engine->start_timeout_id = 0;
    mms_engine_stop_schedule(engine);
    return FALSE;
}

static
void
mms_engine_start_timeout_cancel(
    MMSEngine* engine)
{
    if (engine->start_timeout_id) {
        g_source_remove(engine->start_timeout_id);
        engine->start_timeout_id = 0;
    }
}

static
void
mms_engine_start_timeout_schedule(
    MMSEngine* engine)
{
    mms_engine_start_timeout_cancel(engine);
    engine->start_timeout_id = g_timeout_add_seconds(engine->config->idle_secs,
        mms_engine_start_timeout_callback, engine);
}

#ifdef ENABLE_TEST
/* org.nemomobile.MmsEngine.test */
static
gboolean
mms_engine_handle_test(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    MMSEngine* engine)
{
    MMS_DEBUG("Test");
    org_nemomobile_mms_engine_complete_test(proxy, call);
    return TRUE;
}
#endif /* ENABLE_TEST */

/* org.nemomobile.MmsEngine.sendMessage */
static
gboolean
mms_engine_handle_send_message(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    int database_id,
    const char* imsi_to,
    const char* const* to,
    const char* const* cc,
    const char* const* bcc,
    const char* subject,
    guint flags,
    GVariant* attachments,
    MMSEngine* engine)
{
    if (to && *to) {
        unsigned int i;
        char* to_list = g_strjoinv(",", (char**)to);
        char* cc_list = NULL;
        char* bcc_list = NULL;
        char* id = NULL;
        char* imsi;
        MMSAttachmentInfo* parts;
        GArray* info = g_array_sized_new(FALSE, FALSE, sizeof(*parts), 0);
        GError* error = NULL;

        /* Extract attachment info */
        char* fn = NULL;
        char* ct = NULL;
        char* cid = NULL;
        GVariantIter* iter = NULL;
        g_variant_get(attachments, "a(sss)", &iter);
        while (g_variant_iter_loop(iter, "(sss)", &fn, &ct, &cid)) {
            MMSAttachmentInfo part;
            part.file_name = g_strdup(fn);
            part.content_type = g_strdup(ct);
            part.content_id = g_strdup(cid);
            g_array_append_vals(info, &part, 1);
        }

        /* Convert address lists into comma-separated strings
         * expected by mms_dispatcher_send_message and mms_codec */
        if (cc && *cc) cc_list = g_strjoinv(",", (char**)cc);
        if (bcc && *bcc) bcc_list = g_strjoinv(",", (char**)bcc);
        if (database_id > 0) id = g_strdup_printf("%u", database_id);

        /* Queue the message */
        parts = (void*)info->data;
        imsi = mms_dispatcher_send_message(engine->dispatcher, id,
            imsi_to, to_list, cc_list, bcc_list, subject, flags, parts,
            info->len, &error);
        if (imsi) {
            if (mms_dispatcher_start(engine->dispatcher)) {
                mms_engine_start_timeout_cancel(engine);
            }
            org_nemomobile_mms_engine_complete_send_message(proxy, call, imsi);
            g_free(imsi);
        } else {
            g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED, "%s", MMS_ERRMSG(error));
            g_error_free(error);
        }

        for (i=0; i<info->len; i++) {
            g_free((void*)parts[i].file_name);
            g_free((void*)parts[i].content_type);
            g_free((void*)parts[i].content_id);
        }

        g_free(to_list);
        g_free(cc_list);
        g_free(bcc_list);
        g_free(id);
        g_array_unref(info);
        g_variant_iter_free(iter);
    } else {
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "Missing recipient");
    }
    return TRUE;
}

/* org.nemomobile.MmsEngine.receiveMessage */
static
gboolean
mms_engine_handle_receive_message(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    int database_id,
    const char* imsi,
    gboolean automatic,
    GVariant* data,
    MMSEngine* engine)
{
    gsize len = 0;
    const guint8* bytes = g_variant_get_fixed_array(data, &len, 1);
    MMS_DEBUG("Processing push %u bytes from %s", (guint)len, imsi);
    if (imsi && bytes && len) {
        char* id = g_strdup_printf("%d", database_id);
        GBytes* push = g_bytes_new(bytes, len);
        GError* error = NULL;
        if (mms_dispatcher_receive_message(engine->dispatcher, id, imsi,
            automatic, push, &error)) {
            if (mms_dispatcher_start(engine->dispatcher)) {
                mms_engine_start_timeout_cancel(engine);
            }
            org_nemomobile_mms_engine_complete_receive_message(proxy, call);
        } else {
            g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED, "%s", MMS_ERRMSG(error));
            g_error_free(error);
        }
        g_bytes_unref(push);
        g_free(id);
    } else {
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "Invalid parameters");
    }
    return TRUE;
}

/* org.nemomobile.MmsEngine.sendReadReport */
static
gboolean
mms_engine_handle_send_read_report(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    const char* id,
    const char* imsi,
    const char* message_id,
    const char* to,
    int read_status, /*  0: Read  1: Deleted without reading */
    MMSEngine* engine)
{
    GError* error = NULL;
    MMS_DEBUG_("%s %s %s %s %d", id, imsi, message_id, to, read_status);
    if (mms_dispatcher_send_read_report(engine->dispatcher, id, imsi,
        message_id, to, (read_status == 1) ? MMS_READ_STATUS_DELETED :
        MMS_READ_STATUS_READ, &error)) {
        if (mms_dispatcher_start(engine->dispatcher)) {
            mms_engine_start_timeout_cancel(engine);
        }
        org_nemomobile_mms_engine_complete_send_read_report(proxy, call);
    } else {
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "%s", MMS_ERRMSG(error));
        g_error_free(error);
    }
    return TRUE;
}

/* org.nemomobile.MmsEngine.cancel */
static
gboolean
mms_engine_handle_cancel(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    int database_id,
    MMSEngine* engine)
{
    const char *id = NULL;
    if (database_id > 0) id = g_strdup_printf("%u", database_id);
    MMS_DEBUG_("%s", id);
    mms_dispatcher_cancel(engine->dispatcher, id);
    org_nemomobile_mms_engine_complete_cancel(proxy, call);
    g_free(id);
    return TRUE;
}

/* org.nemomobile.MmsEngine.pushNotify */
static
gboolean
mms_engine_handle_push_notify(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    const char* imsi,
    const char* type,
    GVariant* data,
    MMSEngine* engine)
{
    gsize len = 0;
    const guint8* bytes = g_variant_get_fixed_array(data, &len, 1);
    MMS_DEBUG("Received %u bytes from %s", (guint)len, imsi);
    if (!type || g_ascii_strcasecmp(type, MMS_CONTENT_TYPE)) {
        MMS_ERR("Unsupported content type %s", type);
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "Unsupported content type");
    } else if (!imsi || !imsi[0]) {
        MMS_ERR_("IMSI is missing");
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "IMSI is missing");
    } else if (!bytes || !len) {
        MMS_ERR_("No data provided");
        g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
            G_DBUS_ERROR_FAILED, "No data provided");
    } else {
        GError* err = NULL;
        GBytes* msg = g_bytes_new(bytes, len);
        if (mms_dispatcher_handle_push(engine->dispatcher, imsi, msg, &err)) {
            if (mms_dispatcher_start(engine->dispatcher)) {
                mms_engine_start_timeout_cancel(engine);
            }
            org_nemomobile_mms_engine_complete_push(proxy, call);
        } else {
            g_dbus_method_invocation_return_error(call, G_DBUS_ERROR,
                G_DBUS_ERROR_FAILED, "%s", MMS_ERRMSG(err));
            g_error_free(err);
        }
        g_bytes_unref(msg);
    }
    return TRUE;
}

/* org.nemomobile.MmsEngine.push */
static
gboolean
mms_engine_handle_push(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    const char* imsi,
    const char* from,
    guint32 remote_time,
    guint32 local_time,
    int dst_port,
    int src_port,
    const char* type,
    GVariant* data,
    MMSEngine* eng)
{
    return mms_engine_handle_push_notify(proxy, call, imsi, type, data, eng);
}

/* org.nemomobile.MmsEngine.setLogLevel */
static
gboolean
mms_engine_handle_set_log_level(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    const char* module,
    gint level,
    MMSEngine* engine)
{
    MMS_DEBUG_("%s:%d", module, level);
    if (module && module[0]) {
        int i;
        for (i=0; i<engine->log_count; i++) {
            MMSLogModule* log = engine->log_modules[i];
            if (log->name && log->name[0] && !strcmp(log->name, module)) {
                log->level = level;
                break;
            }
        }
    } else {
        mms_log_default.level = level;
    }
    org_nemomobile_mms_engine_complete_set_log_level(proxy, call);
    return TRUE;
}

/* org.nemomobile.MmsEngine.setLogType */
static
gboolean
mms_engine_handle_set_log_type(
    OrgNemomobileMmsEngine* proxy,
    GDBusMethodInvocation* call,
    const char* type,
    MMSEngine* engine)
{
    MMS_DEBUG_("%s", type);
    mms_log_set_type(type, MMS_APP_LOG_PREFIX);
    org_nemomobile_mms_engine_complete_set_log_type(proxy, call);
    return TRUE;
}

MMSEngine*
mms_engine_new(
    const MMSConfig* config,
    unsigned int flags,
    MMSLogModule* log_modules[],
    int log_count)
{
    MMSConnMan* cm = mms_connman_ofono_new();
    if (cm) {
        MMSEngine* mms = g_object_new(MMS_ENGINE_TYPE, NULL);
        MMSHandler* handler = mms_handler_dbus_new();
        mms->dispatcher = mms_dispatcher_new(config, cm, handler);
        mms_connman_unref(cm);
        mms_handler_unref(handler);
        mms_dispatcher_set_delegate(mms->dispatcher,
            &mms->dispatcher_delegate);

        if (flags & MMS_ENGINE_FLAG_KEEP_RUNNING) {
            mms->keep_running = TRUE;
        }

        mms->config = config;
        mms->log_modules = log_modules;
        mms->log_count = log_count;
        mms->proxy = org_nemomobile_mms_engine_skeleton_new();
        mms->send_message_id =
            g_signal_connect(mms->proxy, "handle-send-message",
            G_CALLBACK(mms_engine_handle_send_message), mms);
        mms->push_signal_id =
            g_signal_connect(mms->proxy, "handle-push",
            G_CALLBACK(mms_engine_handle_push), mms);
        mms->push_notify_signal_id =
            g_signal_connect(mms->proxy, "handle-push-notify",
            G_CALLBACK(mms_engine_handle_push_notify), mms);
        mms->cancel_signal_id =
            g_signal_connect(mms->proxy, "handle-cancel",
            G_CALLBACK(mms_engine_handle_cancel), mms);
        mms->receive_signal_id =
            g_signal_connect(mms->proxy, "handle-receive-message",
            G_CALLBACK(mms_engine_handle_receive_message), mms);
        mms->read_report_signal_id =
            g_signal_connect(mms->proxy, "handle-send-read-report",
            G_CALLBACK(mms_engine_handle_send_read_report), mms);
        mms->set_log_level_signal_id =
            g_signal_connect(mms->proxy, "handle-set-log-level",
            G_CALLBACK(mms_engine_handle_set_log_level), mms);
        mms->set_log_type_signal_id =
            g_signal_connect(mms->proxy, "handle-set-log-type",
            G_CALLBACK(mms_engine_handle_set_log_type), mms);

#ifdef ENABLE_TEST
        mms->test_signal_id =
            g_signal_connect(mms->proxy, "handle-test",
            G_CALLBACK(mms_engine_handle_test), mms);
#endif

        return mms;
    }

    return NULL;
}

MMSEngine*
mms_engine_ref(
    MMSEngine* engine)
{
    return g_object_ref(MMS_ENGINE(engine));
}

void
mms_engine_unref(
    MMSEngine* engine)
{
    if (engine) g_object_unref(MMS_ENGINE(engine));
}

void
mms_engine_run(
    MMSEngine* engine,
    GMainLoop* loop)
{
    MMS_ASSERT(!engine->loop);
    engine->loop = loop;
    engine->stopped = FALSE;
    if (!mms_dispatcher_start(engine->dispatcher) && !engine->keep_running) {
        mms_engine_start_timeout_schedule(engine);
    }
    g_main_loop_run(loop);
    mms_engine_start_timeout_cancel(engine);
    engine->loop = NULL;
}

void
mms_engine_stop(
    MMSEngine* engine)
{
    engine->stopped = TRUE;
    if (engine->loop) g_main_loop_quit(engine->loop);
}

void
mms_engine_unregister(
    MMSEngine* engine)
{
    if (engine->engine_bus) {
        g_dbus_interface_skeleton_unexport(
            G_DBUS_INTERFACE_SKELETON(engine->proxy));
        g_object_unref(engine->engine_bus);
        engine->engine_bus = NULL;
    }
}

gboolean
mms_engine_register(
    MMSEngine* engine,
    GDBusConnection* bus,
    GError** error)
{
    mms_engine_unregister(engine);
    if (g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(engine->proxy), bus,
        MMS_ENGINE_PATH, error)) {
        g_object_ref(engine->engine_bus = bus);
        return TRUE;
    } else {
        return FALSE;
    }
}

static
void
mms_engine_delegate_dispatcher_done(
    MMSDispatcherDelegate* delegate,
    MMSDispatcher* dispatcher)
{
    MMSEngine* engine = mms_engine_from_dispatcher_delegate(delegate);
    MMS_DEBUG("All done");
    if (!engine->keep_running) mms_engine_stop_schedule(engine);
}

/**
 * Per object initializer
 *
 * Only sets up internal state (all values set to zero)
 */
static
void
mms_engine_init(
    MMSEngine* engine)
{
    engine->dispatcher_delegate.fn_done = mms_engine_delegate_dispatcher_done;
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
mms_engine_dispose(
    GObject* object)
{
    MMSEngine* e = MMS_ENGINE(object);
    MMS_VERBOSE_("%p", e);
    MMS_ASSERT(!e->loop);
    mms_engine_unregister(e);
    mms_engine_start_timeout_cancel(e);
    if (e->proxy) {
#ifdef ENABLE_TEST
        g_signal_handler_disconnect(e->proxy, e->test_signal_id);
#endif
        g_signal_handler_disconnect(e->proxy, e->send_message_id);
        g_signal_handler_disconnect(e->proxy, e->push_signal_id);
        g_signal_handler_disconnect(e->proxy, e->push_notify_signal_id);
        g_signal_handler_disconnect(e->proxy, e->receive_signal_id);
        g_signal_handler_disconnect(e->proxy, e->read_report_signal_id);
        g_signal_handler_disconnect(e->proxy, e->cancel_signal_id);
        g_signal_handler_disconnect(e->proxy, e->set_log_level_signal_id);
        g_signal_handler_disconnect(e->proxy, e->set_log_type_signal_id);
        g_object_unref(e->proxy);
        e->proxy = NULL;
    }
    if (e->dispatcher) {
        mms_dispatcher_set_delegate(e->dispatcher, NULL);
        mms_dispatcher_unref(e->dispatcher);
        e->dispatcher = NULL;
    }
    G_OBJECT_CLASS(mms_engine_parent_class)->dispose(object);
}

/**
 * Per class initializer
 */
static
void
mms_engine_class_init(
    MMSEngineClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    MMS_ASSERT(object_class);
    object_class->dispose = mms_engine_dispose;
    MMS_VERBOSE_("done");
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
