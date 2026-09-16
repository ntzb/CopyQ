#pragma once

#include <memory>

class QModelIndex;
class QString;
class QTextCharFormat;
class QTextEdit;

class ItemFilter
{
public:
    virtual ~ItemFilter() = default;
    virtual bool matchesAll() const = 0;
    virtual bool matchesNone() const = 0;
    virtual bool matches(const QString &text) const = 0;
    virtual bool matchesIndex(const QModelIndex &index) const = 0;
    virtual void highlight(QTextEdit *edit, const QTextCharFormat &format) const = 0;
    virtual void search(QTextEdit *edit, bool backwards) const = 0;
    virtual QString searchString() const = 0;

    /**
     * Return true if this filter can only match a subset of what the filter
     * for @a previousSearchString matched.
     *
     * Allows re-filtering only the items still visible instead of the whole
     * tab. Regular expressions cannot narrow ("a" to "a|b" widens), so the
     * default is false.
     */
    virtual bool narrows(const QString &previousSearchString) const;
};

inline bool ItemFilter::narrows(const QString &) const
{
    return false;
}

using ItemFilterPtr = std::shared_ptr<ItemFilter>;
