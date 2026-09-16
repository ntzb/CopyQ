// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QChar>
#include <QString>

/**
 * A 64 bit summary of which characters a text contains.
 *
 * An item can only contain a search string if it contains every character of
 * it, so a search string whose bits are not all present in an item's bits
 * cannot match. Testing that is a single AND, which is much cheaper than
 * searching the text itself, and most items fail it.
 *
 * Characters are folded to lower case, so the summary is a superset for a
 * case sensitive search - it can only ever allow too much, never too little.
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
    for (const QChar c : text)
        *signature |= (Q_UINT64_C(1) << searchSignatureBucket(c.toLower().unicode()));
}

inline quint64 searchSignatureOf(const QString &text)
{
    quint64 signature = 0;
    addToSearchSignature(&signature, text);
    return signature;
}
