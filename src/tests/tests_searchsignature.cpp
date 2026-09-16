// SPDX-License-Identifier: GPL-3.0-or-later

#include "tests.h"

#include "common/searchsignature.h"

#include <QStringMatcher>
#include <QtTest>

namespace {

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
    // signature must not reject the item, or the item silently disappears.
    const QStringList texts = {
        QStringLiteral("10 μF capacitor"),  // GREEK SMALL LETTER MU
        QStringLiteral("10 µF capacitor"),  // MICRO SIGN
        QStringLiteral("Οδός Ερμού"),  // final sigma
        QStringLiteral("ſuperfluous"),      // LATIN SMALL LETTER LONG S
        QStringLiteral("ẛ"),
        QStringLiteral("İstanbul"),         // dotted capital I
        QStringLiteral("straẞe"),           // capital sharp s
        QStringLiteral("\U00010400"),            // DESERET CAPITAL LONG I
        QStringLiteral("Ꭰ"),                // CHEROKEE LETTER A
        QStringLiteral("Plain ASCII text 123"),
    };
    const QStringList needles = {
        QStringLiteral("μ"), QStringLiteral("µ"),
        QStringLiteral("σ"), QStringLiteral("ς"),
        QStringLiteral("οδος"),
        QStringLiteral("s"), QStringLiteral("S"), QStringLiteral("ſ"),
        QStringLiteral("ṡ"), QStringLiteral("i"), QStringLiteral("I"),
        QStringLiteral("ß"), QStringLiteral("\U00010428"),
        QStringLiteral("ꭰ"), QStringLiteral("ascii"), QStringLiteral("123"),
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
