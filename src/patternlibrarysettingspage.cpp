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
    {
        m_list = new QListWidget(this);
        refreshList();

        auto *addButton = new QPushButton(Tr::tr("Add..."), this);
        auto *editButton = new QPushButton(Tr::tr("Edit..."), this);
        auto *removeButton = new QPushButton(Tr::tr("Remove"), this);

        connect(addButton, &QPushButton::clicked, this, [this] {
            PatternEditDialog dialog(Pattern::createNew(), this);
            if (dialog.exec() == QDialog::Accepted) {
                patternLibrary().addOrUpdate(dialog.result());
                refreshList();
            }
        });
        connect(editButton, &QPushButton::clicked, this, [this] {
            QListWidgetItem *item = m_list->currentItem();
            if (!item)
                return;
            const auto existing = patternLibrary().patternById(item->data(Qt::UserRole).toString());
            if (!existing)
                return;
            PatternEditDialog dialog(*existing, this);
            if (dialog.exec() == QDialog::Accepted) {
                patternLibrary().addOrUpdate(dialog.result());
                refreshList();
            }
        });
        connect(removeButton, &QPushButton::clicked, this, [this] {
            QListWidgetItem *item = m_list->currentItem();
            if (!item)
                return;
            patternLibrary().remove(item->data(Qt::UserRole).toString());
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

    void apply() final { patternLibrary().save(); }

private:
    void refreshList()
    {
        m_list->clear();
        for (const Pattern &p : patternLibrary().patterns()) {
            auto *item = new QListWidgetItem(p.name, m_list);
            item->setData(Qt::UserRole, p.id);
        }
    }

    QListWidget *m_list;
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
