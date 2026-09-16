// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QChar>
#include <QString>
#include <QtGlobal>

/**
 * A 64 bit summary of which characters a text contains.
 *
 * An item can only contain a search string if it contains every character of
 * it, so a search string whose bits are not all present in an item's bits
 * cannot match. Testing that is a single AND, which is much cheaper than
 * searching the text itself.
 *
 * Characters are folded the same way the matching does it - QStringMatcher
 * and QString::contains() both use case folding, not lower casing, and the
 * two differ for characters like the Greek final sigma and the micro sign.
 * Folding with anything else here would reject items that do match.
 *
 * Characters outside the basic plane are ignored: they are stored as two
 * UTF-16 units that cannot be folded on their own. Ignoring them in the
 * search string means they demand nothing, which stays on the safe side.
 */
inline int searchSignatureBucket(char16_t c)
{
    if (c >= u'a' && c <= u'z')
        return c - u'a';
    if (c >= u'0' && c <= u'9')
        return 26 + (c - u'0');
    return 36 + (c % 28);
}

inline void addToSearchSignature(quint64 *signature, const QString &text)
{
    for (const QChar c : text) {
        if ( c.isSurrogate() )
            continue;
        *signature |= (Q_UINT64_C(1) << searchSignatureBucket(c.toCaseFolded().unicode()));
    }
}
