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

extern "C" {
#define MMS_LOG_MODULE_NAME mms_attachment_log
#include "mms_lib_log.h"
#include "mms_attachment_image.h"
}

#ifdef MMS_RESIZE_QT

#include <QtGui/QtGui>

gboolean
mms_attachment_image_resize_qt(
    MMSAttachmentImage* image)
{
    gboolean ok = FALSE;
    QImage qimage;
    if (qimage.load(image->attachment.original_file)) {
        const int w = qimage.width();
        const int h = qimage.height();
        const int step = mms_attachment_image_next_resize_step(image, w, h);
        const char* fname = mms_attachment_image_prepare_filename(image);
        const int w1 = w/(step+1);
        QImage scaled = qimage.scaledToWidth(w1, Qt::SmoothTransformation);
        if (scaled.save(fname)) {
            MMS_DEBUG("Scaling %s (%dx%d -> %dx%d) with Qt", fname, w, h,
                scaled.width(), scaled.height());
            image->resize_step = step;
            ok = TRUE;
        }
    }
    return ok;
}

#endif /* MMS_RESIZE_QT */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
