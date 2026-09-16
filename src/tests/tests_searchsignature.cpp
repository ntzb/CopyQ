// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests.h"

#include "common/searchsignature.h"

#include <QStringMatcher>
#include <QtTest>

namespace {

// Built from code points rather than literals: the source encoding the
// compiler assumes must not decide what this test covers.
QString chars(std::initializer_list<char32_t> codePoints)
{
    QString s;
    for (const char32_t c : codePoints)
        s += QString::fromUcs4(&c, 1);
    return s;
}

bool signatureAdmits(const QString &text, const QString &needle)
{
    quint64 present = 0;
    addToSearchSignature(&present, text);
    quint64 required = 0;
    addToSearchSignature(&required, needle);
    return (present & required) == required;
}

} // namespace

void CoreTests::searchSignatureIsSuperset()
{
    // Characters whose case folding differs from lower casing, or which are
    // stored as surrogate pairs. If the matcher finds the needle, the
    // signature must admit the item, or the item silently disappears.
    const QStringList texts = {
        chars({'1', '0', ' ', 0x03bc, 'F'}),   // GREEK SMALL LETTER MU
        chars({'1', '0', ' ', 0x00b5, 'F'}),   // MICRO SIGN
        chars({0x039f, 0x03b4, 0x03cc, 0x03c2}),  // ends in final sigma
        chars({0x017f, 'u', 'p'}),             // LATIN SMALL LETTER LONG S
        chars({0x1e9b}),
        chars({0x0130, 's'}),                  // LATIN CAPITAL I WITH DOT
        chars({0x1e9e, 'e'}),                  // LATIN CAPITAL SHARP S
        chars({0x10400}),                      // DESERET, outside the BMP
        chars({0x13a0}),                       // CHEROKEE LETTER A
        chars({0x05e9, 0x05dc, 0x05d5, 0x05dd}),  // Hebrew
        QStringLiteral("Plain ASCII text 123"),
    };
    const QStringList needles = {
        chars({0x03bc}), chars({0x00b5}),
        chars({0x03c3}), chars({0x03c2}),
        chars({0x03bf, 0x03b4, 0x03bf, 0x03c2}),
        QStringLiteral("s"), QStringLiteral("S"), chars({0x017f}),
        chars({0x1e61}), QStringLiteral("i"), QStringLiteral("I"),
        chars({0x00df}), chars({0x10428}), chars({0xab70}),
        chars({0x05dc, 0x05d5}),
        QStringLiteral("ascii"), QStringLiteral("123"),
    };

    for (const auto sensitivity : {Qt::CaseInsensitive, Qt::CaseSensitive}) {
        for (const QString &text : texts) {
            for (const QString &needle : needles) {
                const QStringMatcher matcher(needle, sensitivity);
                if ( matcher.indexIn(text) == -1 )
                    continue;
                QVERIFY2(
                    signatureAdmits(text, needle),
                    qPrintable(QStringLiteral("Signature rejects a match: text %1, needle %2")
                               .arg(QString::fromLatin1(text.toUtf8().toHex()),
                                    QString::fromLatin1(needle.toUtf8().toHex()))) );
            }
        }
    }
}
