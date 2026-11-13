#pragma once

#include <functional>
#include <atomic>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QTranslator>
#include <QtCore/QVector>
#include <ZzCore/ZzResult.h>

namespace ZzCore { class ZzSettingsStore; }

namespace ZzExample {

/** @brief Updates a dynamic display property and retains its translation recipe. */
void zzExampleBindText(QObject *target, std::function<QString()> text, const char *property = "text");

class ZzExampleTranslationScope;

/** @brief Owns the example's translator and persistent, application-wide language choice. */
class ZzExampleLanguageController final : public QTranslator
{
    Q_OBJECT
public:
    explicit ZzExampleLanguageController(ZzCore::ZzSettingsStore &settings);
    ~ZzExampleLanguageController() override;
    [[nodiscard]] ZzCore::ZzResult<void> restore(bool useSavedPreference = true);
    [[nodiscard]] ZzCore::ZzResult<void> setLanguage(const QString &language);
    [[nodiscard]] QString language() const { return language_; }
    [[nodiscard]] bool isEmpty() const override { return false; }
    [[nodiscard]] QString translate(const char *context, const char *source,
        const char *disambiguation = nullptr, int n = -1) const override;
    /** @brief Registers source-key bindings without rebuilding widgets or touching input values. */
    void bindObjectTree(QObject *root, const QHash<QString, QByteArray> &contexts);
    static ZzExampleLanguageController *active();

Q_SIGNALS:
    void languageChanged();

private:
    friend class ZzExampleTranslationScope;
    struct ZzText {
        QByteArray context;
        QString source;
        QStringList arguments;
    };
    [[nodiscard]] ZzText identify(const QString &value,
        const QHash<QString, QByteArray> &contexts) const;
    [[nodiscard]] QString render(const ZzText &text) const;
    void bind(QObject *owner, const QString &key,
        std::function<QString()> read, std::function<void(const QString &)> write,
        const QHash<QString, QByteArray> &contexts);
    [[nodiscard]] ZzCore::ZzResult<void> apply(const QString &language, bool persist);

    ZzCore::ZzSettingsStore &settings_;
    QTranslator english_;
    QHash<QString, QByteArray> sources_;
    QString language_ = QStringLiteral("system");
    std::atomic<bool> englishActive_{false};
    bool loaded_ = false;
    mutable QVector<QHash<QString, QByteArray> *> captures_;
};

/**
 * @brief Constructs a view in source text and binds its display properties before
 * applying the active language. This preserves unambiguous source keys even for
 * pages first opened in English (e.g. multiple source strings translate to "Value").
 */
class ZzExampleTranslationScope final
{
public:
    explicit ZzExampleTranslationScope(QObject *root);
    ~ZzExampleTranslationScope();
    ZzExampleTranslationScope(const ZzExampleTranslationScope &) = delete;
    ZzExampleTranslationScope &operator=(const ZzExampleTranslationScope &) = delete;
private:
    QPointer<ZzExampleLanguageController> controller_;
    QPointer<QObject> root_;
    QHash<QString, QByteArray> contexts_;
};

} // namespace ZzExample
