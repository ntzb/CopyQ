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
     * Return true if this filter can only match a subset of what
     * @a previousFilter matched.
     *
     * Allows re-filtering only the items still visible instead of the whole
     * tab. The whole filter is passed, not just its search string, because
     * the search options can change without the text changing.
     *
     * Regular expressions can widen ("a" to "a|b"), so the default is false.
     */
    virtual bool narrows(const ItemFilter &previousFilter) const;
};

inline bool ItemFilter::narrows(const ItemFilter &) const
{
    return false;
}

using ItemFilterPtr = std::shared_ptr<ItemFilter>;
