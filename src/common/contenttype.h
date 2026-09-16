// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once


#include <Qt>

/**
 * Enum values are used in ClipboardModel class to fetch or set data from ClipboardItem.
 * @see ClipboardModel:setData(), ClipboardModel::data()
 */
namespace contentType {

enum {
    /**
     * Set/get data as QVarianMap (key is MIME type and value is QByteArray).
     */
    data = Qt::UserRole,

    /**
     * Update existing data. Clears non-internal data if passed data map contains non-internal data.
     */
    updateData,

    /**
     * Remove formats (QStringList of MIME types).
     */
    removeFormats,

    text,
    html,
    notes,

    /// Item color (string expression as used in themes).
    color,

    /// If true, hide content of item (not notes, tags etc.).
    isHidden,

    /// Item text with diacritics removed; invalid if the text has none.
    textWithoutAccents,

    /// Bit per character class present in the item's searchable text,
    /// as a quint64. See ClipboardItem::searchSignature().
    /// New values go last so plugins built against an older header keep
    /// the meaning of the ones they know.
    searchSignature
};

}
