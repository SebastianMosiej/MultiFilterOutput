#include "patternlibrarysettingspage.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "pattern.h"
#include "patterneditdialog.h"
#include "patternlibrary.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace MultiFilterOutput::Internal {

namespace {

class PatternLibraryWidget final : public Core::IOptionsPageWidget
{
public:
    PatternLibraryWidget()
        : m_workingPatterns(patternLibrary().patterns())
    {
        m_list = new QListWidget(this);
        refreshList();

        auto *addButton = new QPushButton(Tr::tr("Add..."), this);
        auto *editButton = new QPushButton(Tr::tr("Edit..."), this);
        auto *removeButton = new QPushButton(Tr::tr("Remove"), this);

        connect(addButton, &QPushButton::clicked, this, [this] {
            PatternEditDialog dialog(Pattern::createNew(), this);
            if (dialog.exec() == QDialog::Accepted) {
                upsert(dialog.result());
                refreshList();
            }
        });
        connect(editButton, &QPushButton::clicked, this, [this] {
            QListWidgetItem *item = m_list->currentItem();
            if (!item)
                return;
            const auto existing = patternAt(item->data(Qt::UserRole).toString());
            if (!existing)
                return;
            PatternEditDialog dialog(*existing, this);
            if (dialog.exec() == QDialog::Accepted) {
                upsert(dialog.result());
                refreshList();
            }
        });
        connect(removeButton, &QPushButton::clicked, this, [this] {
            QListWidgetItem *item = m_list->currentItem();
            if (!item)
                return;
            const QString id = item->data(Qt::UserRole).toString();
            m_workingPatterns.removeIf([&id](const Pattern &p) { return p.id == id; });
            refreshList();
        });

        auto *buttonLayout = new QVBoxLayout;
        buttonLayout->addWidget(addButton);
        buttonLayout->addWidget(editButton);
        buttonLayout->addWidget(removeButton);
        buttonLayout->addStretch();

        auto *layout = new QHBoxLayout(this);
        layout->addWidget(m_list);
        layout->addLayout(buttonLayout);
    }

    // Add/Edit/Remove only ever touch the working copy, so dismissing the Options dialog
    // with Cancel (which simply never calls apply()) leaves the live library -- and, since
    // the plugin saves it again on shutdown, the on-disk settings -- untouched.
    void apply() final
    {
        patternLibrary().setPatterns(m_workingPatterns);
        patternLibrary().save();
    }

private:
    void refreshList()
    {
        m_list->clear();
        for (const Pattern &p : std::as_const(m_workingPatterns)) {
            auto *item = new QListWidgetItem(p.name, m_list);
            item->setData(Qt::UserRole, p.id);
        }
    }

    std::optional<Pattern> patternAt(const QString &id) const
    {
        for (const Pattern &p : m_workingPatterns) {
            if (p.id == id)
                return p;
        }
        return std::nullopt;
    }

    void upsert(const Pattern &pattern)
    {
        for (Pattern &existing : m_workingPatterns) {
            if (existing.id == pattern.id) {
                existing = pattern;
                return;
            }
        }
        m_workingPatterns.append(pattern);
    }

    QListWidget *m_list;
    QList<Pattern> m_workingPatterns;
};

} // namespace

PatternLibrarySettingsPage::PatternLibrarySettingsPage()
{
    setId(Constants::OPTIONS_PAGE_ID);
    setDisplayName(Tr::tr("Pattern Library"));
    setCategory(Constants::OPTIONS_CATEGORY_ID);
    setWidgetCreator([] { return new PatternLibraryWidget; });
}

} // namespace MultiFilterOutput::Internal
