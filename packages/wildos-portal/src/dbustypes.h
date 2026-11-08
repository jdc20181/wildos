#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>

// The structured values of the FileChooser portal
// (org.freedesktop.impl.portal.FileChooser): filters a(sa(us)), one
// filter (sa(us)), choices a(ssa(ss)s) and chosen values a(ss).
namespace Portal
{
struct FilterRule {
    uint type = 0; // 0 a glob such as *.pdf, 1 a MIME type
    QString pattern;
};
using FilterRules = QList<FilterRule>;

struct Filter {
    QString name;
    FilterRules rules;
};
using Filters = QList<Filter>;

struct ChoiceOption {
    QString id;
    QString label;
};
using ChoiceOptions = QList<ChoiceOption>;

struct Choice {
    QString id;
    QString label;
    ChoiceOptions options; // empty for a check box
    QString initial;
};
using Choices = QList<Choice>;

struct Chosen {
    QString id;
    QString value;
};
using ChosenList = QList<Chosen>;

void registerTypes();
}

Q_DECLARE_METATYPE(Portal::FilterRule)
Q_DECLARE_METATYPE(Portal::Filter)
Q_DECLARE_METATYPE(Portal::ChoiceOption)
Q_DECLARE_METATYPE(Portal::Choice)
Q_DECLARE_METATYPE(Portal::Chosen)

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::FilterRule &rule);
const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::FilterRule &rule);
QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Filter &filter);
const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Filter &filter);
QDBusArgument &operator<<(QDBusArgument &arg, const Portal::ChoiceOption &option);
const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::ChoiceOption &option);
QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Choice &choice);
const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Choice &choice);
QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Chosen &chosen);
const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Chosen &chosen);
