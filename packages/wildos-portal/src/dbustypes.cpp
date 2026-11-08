#include "dbustypes.h"

#include <QDBusMetaType>

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::FilterRule &rule)
{
    arg.beginStructure();
    arg << rule.type << rule.pattern;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::FilterRule &rule)
{
    arg.beginStructure();
    arg >> rule.type >> rule.pattern;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Filter &filter)
{
    arg.beginStructure();
    arg << filter.name << filter.rules;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Filter &filter)
{
    arg.beginStructure();
    arg >> filter.name >> filter.rules;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::ChoiceOption &option)
{
    arg.beginStructure();
    arg << option.id << option.label;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::ChoiceOption &option)
{
    arg.beginStructure();
    arg >> option.id >> option.label;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Choice &choice)
{
    arg.beginStructure();
    arg << choice.id << choice.label << choice.options << choice.initial;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Choice &choice)
{
    arg.beginStructure();
    arg >> choice.id >> choice.label >> choice.options >> choice.initial;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const Portal::Chosen &chosen)
{
    arg.beginStructure();
    arg << chosen.id << chosen.value;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, Portal::Chosen &chosen)
{
    arg.beginStructure();
    arg >> chosen.id >> chosen.value;
    arg.endStructure();
    return arg;
}

namespace Portal
{
void registerTypes()
{
    qDBusRegisterMetaType<FilterRule>();
    qDBusRegisterMetaType<FilterRules>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<Filters>();
    qDBusRegisterMetaType<ChoiceOption>();
    qDBusRegisterMetaType<ChoiceOptions>();
    qDBusRegisterMetaType<Choice>();
    qDBusRegisterMetaType<Choices>();
    qDBusRegisterMetaType<Chosen>();
    qDBusRegisterMetaType<ChosenList>();
}
}
