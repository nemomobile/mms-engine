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

#include "mms_task.h"
#include "mms_attachment.h"
#include "mms_dispatcher.h"
#include "mms_handler.h"
#include "mms_file_util.h"
#include "mms_util.h"
#include "mms_codec.h"

#include <gio/gio.h>

/* Logging */
#define MMS_LOG_MODULE_NAME mms_task_encode_log
#include "mms_lib_log.h"
#include "mms_error.h"
MMS_LOG_MODULE_DEFINE("mms-task-encode");

/* Class definition */
typedef MMSTaskClass MMSTaskEncodeClass;
typedef struct mms_encode_job MMSEncodeJob;
typedef struct mms_task_encode {
    MMSTask task;
    char* to;
    char* cc;
    char* bcc;
    char* subject;
    int flags;
    MMSAttachment** parts;
    int nparts;
    MMSEncodeJob* active_job;
} MMSTaskEncode;

G_DEFINE_TYPE(MMSTaskEncode, mms_task_encode, MMS_TYPE_TASK);
#define MMS_TYPE_TASK_ENCODE (mms_task_encode_get_type())
#define MMS_TASK_ENCODE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),\
   MMS_TYPE_TASK_ENCODE, MMSTaskEncode))

/* Encoding job (runs on its own thread) */

typedef enum mms_encode_state {
    MMS_ENCODE_STATE_NONE,
    MMS_ENCODE_STATE_RUNNING,
    MMS_ENCODE_STATE_TOO_BIG,
    MMS_ENCODE_STATE_ERROR,
    MMS_ENCODE_STATE_DONE
} MMS_ENCODE_STATE;

typedef struct mms_encode_job {
    gint ref_count;                 /* Reference count */
    MMSTaskEncode* enc;             /* Associated task */
    GCancellable* cancellable;      /* Can be used to cancel the job */
    GMainContext* context;          /* Pointer to the main contex */
    char* path;                     /* Path to the encoded file */
    MMS_ENCODE_STATE state;         /* Job state */
} MMSEncodeJob;

static
void
mms_task_encode_job_done(
    MMSTaskEncode* enc,
    MMSEncodeJob* job);

static
gboolean
mms_encode_job_resize(
    MMSEncodeJob* job)
{
    /* Resize the largest resizable attachment */
    MMSTaskEncode* enc = job->enc;
    MMSAttachment* resize_me = NULL;
    unsigned int largest_size = 0;
    int i;
    MMS_DEBUG("Message is too big, need to resize");
    for (i=0; i<enc->nparts; i++) {
        MMSAttachment* part = enc->parts[i];
        if (part->flags & MMS_ATTACHMENT_RESIZABLE) {
            const char* fname = part->file_name;
            int fd = open(fname, O_RDONLY);
            if (fd >= 0) {
                struct stat st;
                int err = fstat(fd, &st);
                if (!err) {
                    if (largest_size < (unsigned int)st.st_size) {
                        largest_size = st.st_size;
                        resize_me = part;
                    }
                } else {
                    MMS_ERR("Can't stat %s: %s", fname, strerror(errno));
                }
                close(fd);
            } else {
                MMS_ERR("Can't open %s: %s", fname, strerror(errno));
            }
        }
    }
    if (resize_me) {
        MMS_DEBUG("Resizing %s", resize_me->original_file);
        return mms_attachment_resize(resize_me);
    } else {
        MMS_DEBUG("There is nothing to resize");
        return FALSE;
    }
}

static
gsize
mms_encode_job_encode(
    MMSEncodeJob* job)
{
    MMSTaskEncode* enc = job->enc;
    gsize pdu_size = 0;
    char* dir;
    int fd;

    g_free(job->path);
    job->path = NULL;
    dir = mms_task_dir(&enc->task);
    fd = mms_create_file(dir, MMS_SEND_REQ_FILE, &job->path, NULL);
    if (fd >= 0) {
        int i;
        gboolean ok;
        char* start;
        const int flags = enc->flags;
        MMSPdu* mms = g_new0(MMSPdu, 1);
        MMSAttachment* smil = enc->parts[0];

        const char* ct[6];
        ct[0] = "application/vnd.wap.multipart.related";
        ct[1] = "start";
        ct[2] = (start = g_strconcat("<", smil->content_id, ">", NULL));
        ct[3] = "type";
        ct[4] = SMIL_CONTENT_TYPE;
        ct[5] = NULL;
        MMS_ASSERT(smil->flags & MMS_ATTACHMENT_SMIL);

        mms->type = MMS_MESSAGE_TYPE_SEND_REQ;
        mms->version = MMS_VERSION;
        mms->transaction_id = g_strdup(enc->task.id);
        mms->sr.to = g_strdup(enc->to);
        mms->sr.cc = g_strdup(enc->cc);
        mms->sr.bcc = g_strdup(enc->bcc);
        mms->sr.subject = g_strdup(enc->subject);
        mms->sr.dr = ((flags & MMS_SEND_FLAG_REQUEST_DELIVERY_REPORT) != 0);
        mms->sr.rr = ((flags & MMS_SEND_FLAG_REQUEST_READ_REPORT) != 0);
        mms->sr.content_type = mms_unparse_http_content_type((char**)ct);
        for (i=0; i<enc->nparts; i++) {
            MMSAttachment* part = enc->parts[i];
            struct mms_attachment* at = g_new0(struct mms_attachment, 1);
            at->content_type = g_strdup(part->content_type);
            at->data = (void*)g_mapped_file_get_contents(part->map);
            at->length = g_mapped_file_get_length(part->map);
            at->content_id = g_strdup(part->content_id);
            at->content_location = g_strdup(part->content_location);
            mms->attachments = g_slist_append(mms->attachments, at);
        }

        ok = mms_message_encode(mms, fd);
        mms_message_free(mms);
        g_free(start);

        if (ok) {
            struct stat st;
            int err = fstat(fd, &st);
            if (!err) {
                pdu_size = st.st_size;
                MMS_DEBUG("Created %s (%d bytes)", job->path, (int)pdu_size);
            } else {
                MMS_ERR("Can't stat %s: %s", job->path, strerror(errno));
                ok = FALSE;
            }
        } else {
            MMS_ERR("Failed to encode message");
        }

        close(fd);
        if (!ok) {
            unlink(job->path);
            g_free(job->path);
            job->path = NULL;
        }
    }
    g_free(dir);
    return pdu_size;
}

static
void
mms_encode_job_run(
    MMSEncodeJob* job)
{
    int i;
    gsize size;
    const MMSConfig* config = job->enc->task.config;
    MMSTaskEncode* enc = job->enc;

    job->state = MMS_ENCODE_STATE_RUNNING;

    /* Reset the resizing state */
    for (i=0; i<enc->nparts; i++) {
        mms_attachment_reset(enc->parts[i]);
    }

    /* Keep resizing attachments until we squeeze them into the limit */
    size = mms_encode_job_encode(job);
    while (config->size_limit && size > config->size_limit &&
           !g_cancellable_is_cancelled(job->cancellable) &&
           mms_encode_job_resize(job)) {
        gsize last_size = size;
        size = mms_encode_job_encode(job);
        if (!size || size >= last_size) break;
    }

    if (size > 0 && (!config->size_limit || size <= config->size_limit)) {
        job->state = MMS_ENCODE_STATE_DONE;
    } else {
        unlink(job->path);
        g_free(job->path);
        job->path = NULL;
        job->state = (size > 0) ? MMS_ENCODE_STATE_TOO_BIG :
            MMS_ENCODE_STATE_ERROR;
    }
}

static
MMSEncodeJob*
mms_encode_job_ref(
    MMSEncodeJob* job)
{
    if (job) {
        MMS_ASSERT(job->ref_count > 0);
        g_atomic_int_inc(&job->ref_count);
    }
    return job;
}

static
void
mms_encode_job_unref(
    MMSEncodeJob* job)
{
    if (job) {
        MMS_ASSERT(job->ref_count > 0);
        if (g_atomic_int_dec_and_test(&job->ref_count)) {
            mms_task_unref(&job->enc->task);
            g_object_unref(job->cancellable);
            g_main_context_unref(job->context);
            g_free(job->path);
            g_free(job);
        }
    }
}

static
MMSEncodeJob*
mms_encode_job_new(
    MMSTaskEncode* enc)
{
    MMSEncodeJob* job = g_new0(MMSEncodeJob, 1);
    mms_task_ref(&enc->task);
    job->ref_count = 1;
    job->enc = enc;
    job->cancellable = g_cancellable_new();
    job->context = g_main_context_ref(g_main_context_default());
    return job;
}

static
gboolean
mms_encode_job_done(
    gpointer data)
{
    MMSEncodeJob* job = data;
    mms_task_encode_job_done(job->enc, job);
    mms_encode_job_unref(job);
    return FALSE;
}

static
gpointer
mms_encode_job_thread(
    gpointer data)
{
    MMSEncodeJob* job = data;
    mms_encode_job_run(job);
    g_main_context_invoke(job->context, mms_encode_job_done, job);
    /* Reference will be released by mms_encode_job_done */
    return NULL;
}

/* Encoding task */

static
void
mms_task_encode_job_done(
    MMSTaskEncode* enc,
    MMSEncodeJob* job)
{
    if (enc->active_job == job) {
        MMSTask* task = &enc->task;
        MMS_VERBOSE_("Encoding completion state %d", job->state);
        enc->active_job = NULL;
        if (job->state == MMS_ENCODE_STATE_DONE) {
            mms_task_queue_and_unref(task->delegate, mms_task_send_new(
                task->config, task->handler, task->id, task->imsi));
        } else {
            mms_handler_message_send_state_changed(task->handler, task->id,
                (job->state == MMS_ENCODE_STATE_TOO_BIG) ?
                MMS_SEND_STATE_TOO_BIG : MMS_SEND_STATE_SEND_ERROR);
        }
        mms_task_set_state(&enc->task, MMS_TASK_STATE_DONE);
        mms_encode_job_unref(job);
    } else {
        MMS_VERBOSE_("Ignoring stale job completion");
    }
}

static
void
mms_task_encode_run(
    MMSTask* task)
{
    MMSTaskEncode* enc = MMS_TASK_ENCODE(task);
    MMSEncodeJob* job = mms_encode_job_new(enc);
    GError* error = NULL;
    GThread* thread;
    /* Add one extra reference. mms_encode_job_done() will release it */
    mms_encode_job_ref(job);
    thread = g_thread_try_new(task->name, mms_encode_job_thread, job, &error);
    if (thread) {
        g_thread_unref(thread);
        mms_handler_message_send_state_changed(task->handler, task->id,
            MMS_SEND_STATE_ENCODING);
        mms_task_set_state(task, MMS_TASK_STATE_WORKING);
        MMS_ASSERT(!enc->active_job);
        enc->active_job = job;
    } else {
        MMS_ERR("%s", MMS_ERRMSG(error));
        g_error_free(error);
        mms_encode_job_unref(job);
        mms_encode_job_unref(job);
        mms_handler_message_send_state_changed(task->handler, task->id,
            mms_task_retry(task) ? MMS_SEND_STATE_DEFERRED :
            MMS_SEND_STATE_SEND_ERROR);
    }
}

static
void
mms_task_encode_cancel(
    MMSTask* task)
{
    MMSTaskEncode* enc = MMS_TASK_ENCODE(task);
    if (enc->active_job) g_cancellable_cancel(enc->active_job->cancellable);
    MMS_TASK_CLASS(mms_task_encode_parent_class)->fn_cancel(task);
}

static
void
mms_task_encode_finalize(
    GObject* object)
{
    MMSTaskEncode* enc = MMS_TASK_ENCODE(object);
    if (enc->parts) {
        int i;
        for (i=0; i<enc->nparts; i++) mms_attachment_unref(enc->parts[i]);
        g_free(enc->parts);
    }
    g_free(enc->to);
    g_free(enc->cc);
    g_free(enc->bcc);
    g_free(enc->subject);
    G_OBJECT_CLASS(mms_task_encode_parent_class)->finalize(object);
}

static
char*
mms_task_encode_generate_unique_path(
    const char* dir,
    const char* file)
{
    /* Most likely the very first check would succeed */
    char* path = g_strconcat(dir, "/", file, NULL);
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        int i;
        char* tmpfile = NULL;
        for (i=0; i<100; i++) {
            char* newfile = g_strconcat("_", file, NULL);
            g_free(tmpfile);
            g_free(path);
            path = g_strconcat(dir, "/", newfile, NULL);
            file = tmpfile = newfile;
            if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) break;
        }
        g_free(tmpfile);
    }
    return path;
}

static
GPtrArray*
mms_task_encode_prepare_attachments(
    const MMSConfig* config,
    const char* dir,
    const MMSAttachmentInfo* parts,
    int nparts,
    GError** error)
{
    int i;
    int smil_index = -1;
    GPtrArray* array = g_ptr_array_sized_new(nparts);
    for (i=0; i<nparts; i++) {
        MMSAttachment* attachment = NULL;
        MMSAttachmentInfo info = parts[i];
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        char* path = mms_task_encode_generate_unique_path(dir,
            g_basename(info.file_name));
        G_GNUC_END_IGNORE_DEPRECATIONS
        GFile* src = g_file_new_for_path(info.file_name);
        GFile* dest = g_file_new_for_path(path);
        if (g_file_copy(src, dest, 0, NULL, NULL, NULL, error)) {
            info.file_name = path;
            attachment = mms_attachment_new(config, &info, error);
            if (attachment) {
                if (smil_index < 0 &&
                   (attachment->flags & MMS_ATTACHMENT_SMIL)) {
                    smil_index = i;
                }
                g_ptr_array_add(array, attachment);
            }
        } else if (error && *error) {
            MMS_ERR("%s", MMS_ERRMSG(*error));
        }
        g_object_unref(src);
        g_object_unref(dest);
        g_free(path);
        if (!attachment) break;
    }

    if (i == nparts) {
        /* Generate SMIL if necessary */
        if (smil_index < 0) {
            char* path = mms_task_encode_generate_unique_path(dir, "smil");
            MMSAttachment* smil = mms_attachment_new_smil(config, path,
                (MMSAttachment**)array->pdata, array->len, error);
            g_free(path);
            if (smil) {
                smil_index = array->len;
                g_ptr_array_add(array, smil);
            }
        }
        /* Make sure that we SMIL is the first attachment */
        if (smil_index > 0) {
            MMSAttachment* smil = array->pdata[smil_index];
            memmove(array->pdata + 1, array->pdata, sizeof(void*)*smil_index);
            smil_index = 0;
            array->pdata[smil_index] = smil;
        }
    }

    if (smil_index >= 0) {
        return array;
    } else {
        for (i=0; i<array->len; i++) {
            mms_attachment_unref(array->pdata[i]);
        }
        g_ptr_array_free(array, TRUE);
        return NULL;
    }
}

static
char*
mms_task_encode_prepare_address(
    const char* s)
{
    if (s && s[0]) {
        char* str = g_strstrip(g_strdup(s));
        const char* type = g_strrstr(str, MMS_ADDRESS_TYPE_SUFFIX);
        if (type) {
            return str;
        } else {
            char* adr = g_strconcat(str, MMS_ADDRESS_TYPE_SUFFIX_PHONE, NULL);
            g_free(str);
            return adr;
        }
    }
    return NULL;
}

static
char*
mms_task_encode_prepare_address_list(
    const char* s)
{
    if (s && s[0]) {
        if (strchr(s, ',')) {
            int i;
            char** part = g_strsplit(s, ",", 0);
            GString* buf = g_string_sized_new(strlen(s));
            for (i=0; part[i]; i++) {
                char* addr = mms_task_encode_prepare_address(part[i]);
                if (addr) {
                    if (buf->len > 0) g_string_append_c(buf, ',');
                    g_string_append(buf, addr);
                    g_free(addr);
                }
            }
            g_strfreev(part);
            return g_string_free(buf, FALSE);
        } else {
            return mms_task_encode_prepare_address(s);
        }
    }
    return NULL;
}

static
void
mms_task_encode_class_init(
    MMSTaskEncodeClass* klass)
{
    klass->fn_run = mms_task_encode_run;
    klass->fn_cancel = mms_task_encode_cancel;
    G_OBJECT_CLASS(klass)->finalize = mms_task_encode_finalize;
}

static
void
mms_task_encode_init(
    MMSTaskEncode* enc)
{
}

/* Create MMS encode task */
MMSTask*
mms_task_encode_new(
    const MMSConfig* config,
    MMSHandler* handler,
    const char* id,
    const char* imsi,
    const char* to,
    const char* cc,
    const char* bcc,
    const char* subject,
    int flags,
    const MMSAttachmentInfo* parts,
    int nparts,
    GError** error)
{
    MMS_ASSERT(to && to[0]);
    if (to && to[0]) {
        int err;
        char* dir;
        MMSTaskEncode* enc = mms_task_alloc(MMS_TYPE_TASK_ENCODE,
            config, handler, "Encode", id, imsi);
        MMSTask* task = &enc->task;

        mms_task_make_id(task);
        dir = mms_task_file(task, MMS_ENCODE_DIR);
        err = g_mkdir_with_parents(dir, MMS_DIR_PERM);
        if (!err || errno == EEXIST) {
            GPtrArray* array = mms_task_encode_prepare_attachments(config,
                dir, parts, nparts, error);
            if (array) {
                enc->nparts = array->len;
                enc->parts = (MMSAttachment**)g_ptr_array_free(array, FALSE);
                enc->to = mms_task_encode_prepare_address_list(to);
                enc->cc = mms_task_encode_prepare_address_list(cc);
                enc->bcc = mms_task_encode_prepare_address_list(bcc);
                enc->subject = g_strdup(subject);
                enc->flags = flags;

                MMS_ASSERT(!error || !*error);
                g_free(dir);
                return &enc->task;
            }
        } else {
            MMS_ERROR(error, MMS_LIB_ERROR_IO,
                "Failed to create directory %s: %s", dir, strerror(errno));
        }
        g_free(dir);
        mms_task_unref(task);
    } else {
        MMS_ERROR(error, MMS_LIB_ERROR_ARGS, "Missing To: address");
    }
    MMS_ASSERT(!error || *error);
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
