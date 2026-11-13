#include "ZzExampleLanguageController.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QLocale>
#include <QtCore/QMetaProperty>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QThread>
#include <QtCore/QXmlStreamReader>
#include <QtGui/QAction>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTabBar>
#include <ZzFluentUI/ZzMultiSelectComboBox.h>
#include <ZzFluentUI/ZzRollerPicker.h>
#include <ZzFluentUI/ZzBreadcrumbBar.h>
#include <ZzCore/ZzError.h>
#include <ZzCore/ZzErrorCode.h>
#include <ZzCore/ZzSettingsStore.h>

namespace ZzExample {
namespace {
QPointer<ZzExampleLanguageController> activeController;

QString normalizedLanguage(const QString &value)
{
    return value == QStringLiteral("en") || value == QStringLiteral("zh_CN")
        ? value : QStringLiteral("system");
}

QStringList formatArguments(const QString &pattern, const QString &value)
{
    QString expression;
    qsizetype offset = 0;
    auto placeholders = QRegularExpression(QStringLiteral("%[1-9][0-9]*")).globalMatch(pattern);
    while (placeholders.hasNext()) {
        const auto placeholder = placeholders.next();
        expression += QRegularExpression::escape(pattern.mid(offset, placeholder.capturedStart() - offset));
        expression += QStringLiteral("(.*?)");
        offset = placeholder.capturedEnd();
    }
    expression += QRegularExpression::escape(pattern.mid(offset));
    const auto match = QRegularExpression(QStringLiteral("\\A") + expression
        + QStringLiteral("\\z"), QRegularExpression::DotMatchesEverythingOption).match(value);
    return match.hasMatch() ? match.capturedTexts().mid(1) : QStringList{};
}
} // namespace

ZzExampleLanguageController::ZzExampleLanguageController(ZzCore::ZzSettingsStore &settings)
    : settings_(settings)
{
    loaded_ = english_.load(QStringLiteral(":/translations/ZzPureToolsExample_en.qm"));
    QFile catalog(QStringLiteral(":/translations/ZzPureToolsExample_en.ts"));
    if (catalog.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xml(&catalog);
        QByteArray context;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == u"name") {
                context = xml.readElementText().toUtf8();
            } else if (xml.isStartElement() && xml.name() == u"source") {
                const auto source = xml.readElementText();
                if (!sources_.contains(source)) sources_.insert(source, context);
            }
        }
    }
    activeController = this;
    QCoreApplication::installTranslator(this);
}

ZzExampleLanguageController::~ZzExampleLanguageController()
{
    QCoreApplication::removeTranslator(this);
}

ZzExampleLanguageController *ZzExampleLanguageController::active()
{
    return activeController;
}

void zzExampleBindText(QObject *target, std::function<QString()> text, const char *property)
{
    const QString name = QStringLiteral("zzDynamicTranslation_") + QString::fromLatin1(property);
    delete target->findChild<QObject *>(name, Qt::FindDirectChildrenOnly);
    auto *binding = new QObject(target);
    binding->setObjectName(name);
    const auto refresh = [target, property = QByteArray(property), text = std::move(text)] {
        target->setProperty(property.constData(), text());
    };
    if (auto *language = ZzExampleLanguageController::active()) {
        QObject::connect(language, &ZzExampleLanguageController::languageChanged, binding, refresh);
    }
    refresh();
}

ZzCore::ZzResult<void> ZzExampleLanguageController::restore(bool useSavedPreference)
{
    QString preference = QStringLiteral("system");
    if (useSavedPreference) {
        const auto saved = settings_.read(u"ui/language", preference);
        if (!saved) return ZzCore::ZzResult<void>::failure(saved.error());
        preference = normalizedLanguage(saved.value().toString());
    }
    return apply(preference, false);
}

ZzCore::ZzResult<void> ZzExampleLanguageController::setLanguage(const QString &language)
{
    return apply(language, true);
}

ZzCore::ZzResult<void> ZzExampleLanguageController::apply(const QString &language, bool persist)
{
    if (language != normalizedLanguage(language)) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidArgument, QStringLiteral("unsupported example language")));
    }
    if (!loaded_ || sources_.isEmpty()) {
        return ZzCore::ZzResult<void>::failure(ZzCore::ZzError(
            ZzCore::ZzErrorCode::InvalidState, QStringLiteral("example translation resources are missing")));
    }
    if (persist) {
        auto result = settings_.write(u"ui/language", language);
        if (!result) return result;
        result = settings_.sync();
        if (!result) return result;
    }
    language_ = language;
    englishActive_ = language == QStringLiteral("en")
        || (language == QStringLiteral("system") && QLocale::system().language() == QLocale::English);
    Q_EMIT languageChanged();
    QEvent event(QEvent::LanguageChange);
    QCoreApplication::sendEvent(QCoreApplication::instance(), &event);
    return ZzCore::ZzResult<void>::success();
}

QString ZzExampleLanguageController::translate(const char *context, const char *source,
    const char *disambiguation, int n) const
{
    if (QThread::currentThread() == thread() && !captures_.isEmpty()) {
        for (auto *capture : captures_) capture->insert(QString::fromUtf8(source), QByteArray(context));
        return QString::fromUtf8(source);
    }
    return englishActive_ ? english_.translate(context, source, disambiguation, n) : QString{};
}

ZzExampleLanguageController::ZzText ZzExampleLanguageController::identify(
    const QString &value, const QHash<QString, QByteArray> &contexts) const
{
    if (sources_.contains(value)) {
        return {contexts.value(value, sources_.value(value)), value, {}};
    }
    auto patterns = contexts.keys();
    std::sort(patterns.begin(), patterns.end(), [](const QString &left, const QString &right) {
        return left.size() > right.size();
    });
    for (const auto &pattern : patterns) {
        if (!pattern.contains(u'%') || !sources_.contains(pattern)) continue;
        const auto arguments = formatArguments(pattern, value);
        if (!arguments.isEmpty()) return {contexts.value(pattern), pattern, arguments};
    }
    return {};
}

QString ZzExampleLanguageController::render(const ZzText &text) const
{
    QString result = englishActive_ ? english_.translate(text.context.constData(),
        text.source.toUtf8().constData()) : QString{};
    if (result.isEmpty()) result = text.source;
    for (const auto &argument : text.arguments) {
        const auto nested = sources_.contains(argument) ? identify(argument, {}) : ZzText{};
        result = result.arg(nested.source.isEmpty() ? argument : render(nested));
    }
    return result;
}

void ZzExampleLanguageController::bind(QObject *owner, const QString &key,
    std::function<QString()> read, std::function<void(const QString &)> write,
    const QHash<QString, QByteArray> &contexts)
{
    const QByteArray marker = (QStringLiteral("_zzTranslation_") + key).toUtf8();
    if (owner->property(marker.constData()).toBool()) return;
    const QString initial = read();
    ZzText text = identify(initial, contexts);
    if (text.source.isEmpty()) return;
    const auto explicitContext = owner->property("zzTranslationContext").toByteArray();
    if (!explicitContext.isEmpty()) text.context = explicitContext;
    owner->setProperty(marker.constData(), true);
    QObject::connect(this, &ZzExampleLanguageController::languageChanged, owner,
        [this, read = std::move(read), write = std::move(write), text, last = initial]() mutable {
            // Preserve externally changed values; only update this bound display text.
            if (read() != last) return;
            last = render(text);
            write(last);
        });
}

void ZzExampleLanguageController::bindObjectTree(QObject *root,
    const QHash<QString, QByteArray> &contexts)
{
    QList<QObject *> objects{root};
    objects.append(root->findChildren<QObject *>());
    for (QObject *object : objects) {
        if (object->property("zzLanguageUserContent").toBool()
            || QByteArray(object->metaObject()->className()).contains("ZzTimelineEvent")) continue;
        const bool displayText = qobject_cast<QLabel *>(object)
            || qobject_cast<QAbstractButton *>(object) || qobject_cast<QAction *>(object)
            || QByteArray(object->metaObject()->className()).contains("MessageBar");
        for (const char *property : {"text", "title", "headerText", "description", "message",
                 "placeholderText", "toolTip", "statusTip", "accessibleName", "accessibleDescription",
                 "windowTitle", "actionButtonText"}) {
            if (QByteArray(property) == "text" && (!displayText
                    || qobject_cast<ZzFluentUI::ZzRollerPicker *>(object))) continue;
            const int index = object->metaObject()->indexOfProperty(property);
            if (index < 0 || !object->metaObject()->property(index).isWritable()) continue;
            bind(object, QString::fromLatin1(property),
                [object, property] { return object->property(property).toString(); },
                [object, property](const QString &value) {
                    const QSignalBlocker blocker(object);
                    object->setProperty(property, value);
                }, contexts);
        }
        if (auto *combo = qobject_cast<QComboBox *>(object); combo && !combo->isEditable()) {
            for (int index = 0; index < combo->count(); ++index) {
                bind(combo, QStringLiteral("item%1").arg(index),
                    [combo, index] { return combo->itemText(index); },
                    [combo, index](const QString &value) {
                        const QSignalBlocker blocker(combo);
                        combo->setItemText(index, value);
                    }, contexts);
            }
        }
        if (auto *multi = qobject_cast<ZzFluentUI::ZzMultiSelectComboBox *>(object)) {
            for (const auto &option : multi->options()) {
                const QString key = option.key;
                bind(multi, QStringLiteral("option_") + key,
                    [multi, key] {
                        for (const auto &entry : multi->options()) if (entry.key == key) return entry.text;
                        return QString{};
                    }, [multi, key](const QString &value) {
                        auto options = multi->options();
                        for (auto &entry : options) if (entry.key == key) entry.text = value;
                        const QSignalBlocker blocker(multi);
                        multi->setOptions(options);
                    }, contexts);
            }
        }
        if (auto *roller = qobject_cast<ZzFluentUI::ZzRollerPicker *>(object)) {
            const auto columns = roller->columns();
            for (int column = 0; column < columns.size(); ++column) {
                for (int row = 0; row < columns[column].items.size(); ++row) {
                    bind(roller, QStringLiteral("column_%1_%2").arg(column).arg(row),
                        [roller, column, row] { return roller->columns().value(column).items.value(row); },
                        [roller, column, row](const QString &value) {
                            auto updated = roller->columns();
                            if (column >= updated.size() || row >= updated[column].items.size()) return;
                            updated[column].items[row] = value;
                            const QSignalBlocker blocker(roller);
                            roller->setColumns(updated);
                        }, contexts);
                }
            }
        }
        if (auto *breadcrumb = qobject_cast<ZzFluentUI::ZzBreadcrumbBar *>(object)) {
            for (int index = 0; index < breadcrumb->items().size(); ++index) {
                bind(breadcrumb, QStringLiteral("crumb%1").arg(index),
                    [breadcrumb, index] { return breadcrumb->items().value(index); },
                    [breadcrumb, index](const QString &value) {
                        auto items = breadcrumb->items();
                        if (index >= items.size()) return;
                        items[index] = value;
                        const QSignalBlocker blocker(breadcrumb);
                        breadcrumb->setItems(items);
                    }, contexts);
            }
        }
        if (auto *tabs = qobject_cast<QTabBar *>(object)) {
            for (int index = 0; index < tabs->count(); ++index) {
                bind(tabs, QStringLiteral("tab%1").arg(index),
                    [tabs, index] { return tabs->tabText(index); },
                    [tabs, index](const QString &value) { tabs->setTabText(index, value); }, contexts);
            }
        }
        // Only explicitly registered example display models participate. Editable
        // documents, timeline events, logs and user models retain their values.
        auto *model = qobject_cast<QStandardItemModel *>(object);
        if (!model || !model->property("zzLanguageDisplayModel").toBool()) continue;
        auto *bindings = model->findChild<QObject *>(QStringLiteral("zzLanguageModelBindings"), Qt::FindDirectChildrenOnly);
        if (!bindings) {
            bindings = new QObject(model);
            bindings->setObjectName(QStringLiteral("zzLanguageModelBindings"));
            QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, bindings,
                [bindings] { delete bindings; });
        }
        for (int column = 0; column < model->columnCount(); ++column) {
            bind(bindings, QStringLiteral("header%1").arg(column),
                [model, column] { return model->headerData(column, Qt::Horizontal).toString(); },
                [model, column](const QString &value) { model->setHeaderData(column, Qt::Horizontal, value); },
                contexts);
        }
        const auto bindRows = [this, model, bindings, &contexts](auto &&visit, const QModelIndex &parent) -> void {
            for (int row = 0; row < model->rowCount(parent); ++row) {
                for (int column = 0; column < model->columnCount(parent); ++column) {
                    const QPersistentModelIndex index(model->index(row, column, parent));
                    const auto roles = model->itemData(index);
                    for (auto role = roles.cbegin(); role != roles.cend(); ++role) {
                        if (role.key() == Qt::EditRole || role.value().metaType() != QMetaType::fromType<QString>()) continue;
                        auto *item = model->itemFromIndex(index);
                        const QString marker = QStringLiteral("model_%1_%2")
                            .arg(reinterpret_cast<quintptr>(item)).arg(role.key());
                        bind(bindings, marker,
                            [index, key = role.key()] { return index.data(key).toString(); },
                            [model, index, key = role.key()](const QString &value) {
                                if (index.isValid()) model->setData(index, value, key);
                            }, contexts);
                    }
                    visit(visit, index);
                }
            }
        };
        bindRows(bindRows, {});
    }
}

ZzExampleTranslationScope::ZzExampleTranslationScope(QObject *root)
    : controller_(ZzExampleLanguageController::active()), root_(root)
{
    if (controller_) controller_->captures_.append(&contexts_);
}

ZzExampleTranslationScope::~ZzExampleTranslationScope()
{
    if (!controller_) return;
    controller_->captures_.removeLast();
    if (root_) controller_->bindObjectTree(root_, contexts_);
    if (controller_->captures_.isEmpty()) Q_EMIT controller_->languageChanged();
}
} // namespace ZzExample
