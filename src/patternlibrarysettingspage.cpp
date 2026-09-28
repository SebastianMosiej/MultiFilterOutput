#include "patternlibrarysettingspage.h"

#include "multifilteroutputconstants.h"
#include "multifilteroutputtr.h"
#include "outputpanesettings.h"
#include "pattern.h"
#include "patternlibrary.h"

#include <coreplugin/icore.h>

#include <utils/guiutils.h>
#include <utils/utilsicons.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QColorDialog>
#include <QDropEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

namespace MultiFilterOutput::Internal {

namespace {

enum Column {
    NameColumn,
    PatternColumn,
    RegexColumn,
    CaseColumn,
    InvertColumn,
    ColorColumn,
    DefaultColumn
};

// QTreeWidget's internal-move drag-and-drop reorders items without a documented, reliable
// signal to hook (rowsMoved isn't guaranteed for internal moves). Overriding dropEvent lets
// us react right after the base class has already applied the reorder to the item tree.
class PatternTreeWidget final : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

    std::function<void()> onRowsReordered;

protected:
    void dropEvent(QDropEvent *event) override
    {
        QTreeWidget::dropEvent(event);
        if (onRowsReordered)
            onRowsReordered();
    }
};

class PatternLibraryWidget final : public Core::IOptionsPageWidget
{
public:
    PatternLibraryWidget()
        : m_workingPatterns(patternLibrary().patterns())
    {
        auto *placementGroup = new QGroupBox(Tr::tr("Output Pane Position"), this);
        auto *nextToAppOutputRadio = new QRadioButton(Tr::tr("Next to Application Output"), this);
        auto *atEndRadio = new QRadioButton(Tr::tr("At the end (rightmost)"), this);
        if (outputPaneSettings().placement() == OutputPanePlacement::AtEnd)
            atEndRadio->setChecked(true);
        else
            nextToAppOutputRadio->setChecked(true);
        // Unlike the pattern list below, this persists immediately and is not gated by the
        // Options dialog's Apply/Cancel -- see OutputPaneSettings. The new position itself,
        // though, only takes effect on the next restart: Qt Creator only re-sorts the
        // status-bar buttons when building them at startup, and forcing that to happen again
        // mid-session (by recreating the pane) was tried and found to desync every OTHER
        // output pane's button from its content -- OutputPaneManager tracks a pane's stacked
        // content widget by the sorted position it had the first time it was ever shown, and
        // never updates that mapping, so an unrelated pane's button can end up displaying a
        // different pane's content until restart.
        connect(nextToAppOutputRadio, &QRadioButton::toggled, this, [this](bool checked) {
            if (!checked)
                return;
            const bool changed = outputPaneSettings().placement() != OutputPanePlacement::NextToApplicationOutput;
            outputPaneSettings().setPlacement(OutputPanePlacement::NextToApplicationOutput);
            if (changed)
                promptRestart();
        });
        connect(atEndRadio, &QRadioButton::toggled, this, [this](bool checked) {
            if (!checked)
                return;
            const bool changed = outputPaneSettings().placement() != OutputPanePlacement::AtEnd;
            outputPaneSettings().setPlacement(OutputPanePlacement::AtEnd);
            if (changed)
                promptRestart();
        });
        auto *placementLayout = new QHBoxLayout(placementGroup);
        placementLayout->addWidget(nextToAppOutputRadio);
        placementLayout->addWidget(atEndRadio);
        placementLayout->addStretch();

        auto *jumpToAllCheck = new QCheckBox(
            Tr::tr("Clicking a line in a pattern tab jumps to it in the All tab"), this);
        jumpToAllCheck->setChecked(outputPaneSettings().jumpToAllOnClick());
        // Also persists immediately, like the placement radio buttons above -- but takes
        // effect on the very next click, no restart needed: FilteredOutputPane::
        // onJumpRequested() just reads the current value each time a jump is requested.
        connect(jumpToAllCheck, &QCheckBox::toggled, this, [](bool checked) {
            outputPaneSettings().setJumpToAllOnClick(checked);
        });

        auto *focusOnRunStartCheck = new QCheckBox(
            Tr::tr("Show MultiFilterOutput instead of Application Output when a run starts"),
            this);
        focusOnRunStartCheck->setChecked(outputPaneSettings().focusPaneOnRunStart());
        // Also persists immediately and takes effect on the next run -- see
        // FilteredOutputPane::onLineReceived(), which reads this fresh for each new run.
        connect(focusOnRunStartCheck, &QCheckBox::toggled, this, [](bool checked) {
            outputPaneSettings().setFocusPaneOnRunStart(checked);
        });

        auto *unmatchedCheck = new QCheckBox(
            Tr::tr("Show an \"Unmatched\" tab with the lines no pattern took"), this);
        unmatchedCheck->setToolTip(
            Tr::tr("Only when a run has at least one pattern that isn't inverted -- otherwise "
                   "it would just repeat All. Inverted patterns don't count as taking a line."));
        unmatchedCheck->setChecked(outputPaneSettings().showUnmatchedTab());
        // Persists immediately, and the pane adds or removes the tab right away.
        connect(unmatchedCheck, &QCheckBox::toggled, this, [](bool checked) {
            outputPaneSettings().setShowUnmatchedTab(checked);
        });

        auto *maxLineLengthSpin = new QSpinBox(this);
        maxLineLengthSpin->setRange(0, 10'000'000);
        maxLineLengthSpin->setSingleStep(1000);
        maxLineLengthSpin->setSpecialValueText(Tr::tr("No limit"));
        maxLineLengthSpin->setSuffix(Tr::tr(" characters"));
        maxLineLengthSpin->setValue(outputPaneSettings().maxDisplayedLineLength());
        maxLineLengthSpin->setToolTip(
            Tr::tr("Lines longer than this are shown cut short in every MultiFilterOutput tab, "
                   "followed by how many characters were left out. Very long lines slow the "
                   "output view down. Patterns still match against the whole line. "
                   "Applies to output arriving after the change."));
        // Only commit on Enter, focus-out or the arrows -- not on every keystroke, each of
        // which would otherwise persist and push a half-typed limit to every open tab.
        maxLineLengthSpin->setKeyboardTracking(false);
        // Persists immediately like the settings above; FilteredOutputPane applies it to its
        // open tabs right away, via OutputPaneSettings::maxDisplayedLineLengthChanged().
        connect(maxLineLengthSpin, &QSpinBox::valueChanged, this, [](int value) {
            outputPaneSettings().setMaxDisplayedLineLength(value);
        });
        auto *highlightInAllCheck = new QCheckBox(
            Tr::tr("Highlight matched text in the All tab too"), this);
        highlightInAllCheck->setToolTip(
            Tr::tr("Pattern tabs always highlight what their pattern matched. This also does "
                   "it in All, in each pattern's color, for output arriving from now on."));
        highlightInAllCheck->setChecked(outputPaneSettings().highlightMatchesInAll());
        connect(highlightInAllCheck, &QCheckBox::toggled, this, [](bool checked) {
            outputPaneSettings().setHighlightMatchesInAll(checked);
        });

        auto *maxCharactersSpin = new QSpinBox(this);
        maxCharactersSpin->setRange(0, std::numeric_limits<int>::max());
        maxCharactersSpin->setSingleStep(1'000'000);
        maxCharactersSpin->setGroupSeparatorShown(true);
        maxCharactersSpin->setSpecialValueText(Tr::tr("Same as Application Output"));
        maxCharactersSpin->setSuffix(Tr::tr(" characters"));
        maxCharactersSpin->setValue(int(outputPaneSettings().maxCharactersPerTab()));
        maxCharactersSpin->setToolTip(
            Tr::tr("How much output each MultiFilterOutput tab keeps; the oldest lines are "
                   "dropped first. \"Same as Application Output\" follows its \"Limit output "
                   "to ... characters\" setting (Preferences > Build & Run > Application "
                   "Output)."));
        maxCharactersSpin->setKeyboardTracking(false);
        // Persists immediately; FilteredOutputPane applies it to its open tabs right away.
        connect(maxCharactersSpin, &QSpinBox::valueChanged, this, [](int value) {
            outputPaneSettings().setMaxCharactersPerTab(value);
        });

        auto *finishedRunsSpin = new QSpinBox(this);
        finishedRunsSpin->setRange(0, 20);
        finishedRunsSpin->setSpecialValueText(Tr::tr("None"));
        finishedRunsSpin->setValue(outputPaneSettings().finishedRunsKept());
        finishedRunsSpin->setToolTip(
            Tr::tr("How many stopped runs' output to keep, so the pane's run selector can "
                   "switch back to them (while Application Output still has their tabs). "
                   "Each keeps up to the per-tab limit."));
        finishedRunsSpin->setKeyboardTracking(false);
        connect(finishedRunsSpin, &QSpinBox::valueChanged, this, [](int value) {
            outputPaneSettings().setFinishedRunsKept(value);
        });
        auto *finishedRunsRow = new QHBoxLayout;
        finishedRunsRow->addWidget(new QLabel(Tr::tr("Keep output of finished runs:"), this));
        finishedRunsRow->addWidget(finishedRunsSpin);
        finishedRunsRow->addStretch();

        auto *maxLineLengthRow = new QHBoxLayout;
        maxLineLengthRow->addWidget(new QLabel(Tr::tr("Maximum displayed line length:"), this));
        maxLineLengthRow->addWidget(maxLineLengthSpin);
        maxLineLengthRow->addStretch();
        auto *maxCharactersRow = new QHBoxLayout;
        maxCharactersRow->addWidget(new QLabel(Tr::tr("Keep per tab at most:"), this));
        maxCharactersRow->addWidget(maxCharactersSpin);
        maxCharactersRow->addStretch();

        m_tree = new PatternTreeWidget(this);
        m_tree->setColumnCount(7);
        m_tree->setHeaderLabels(
            {Tr::tr("Name"), Tr::tr("Pattern"), Tr::tr("Regex"), Tr::tr("Case"),
             Tr::tr("Invert"), Tr::tr("Color"), Tr::tr("Default")});
        m_tree->setRootIsDecorated(false);
        m_tree->setUniformRowHeights(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->setDragDropMode(QAbstractItemView::InternalMove);
        m_tree->setDropIndicatorShown(true);
        m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(PatternColumn, QHeaderView::Stretch);
        refreshTree();

        auto *addButton = new QPushButton(Tr::tr("Add"), this);
        m_removeButton = new QPushButton(Tr::tr("Remove"), this);
        m_removeButton->setEnabled(false);
        m_moveUpButton = new QPushButton(Tr::tr("Move Up"), this);
        m_moveUpButton->setEnabled(false);
        m_moveDownButton = new QPushButton(Tr::tr("Move Down"), this);
        m_moveDownButton->setEnabled(false);

        connect(addButton, &QPushButton::clicked, this, &PatternLibraryWidget::addPattern);
        connect(m_removeButton, &QPushButton::clicked, this, &PatternLibraryWidget::removeSelected);
        connect(m_moveUpButton, &QPushButton::clicked, this, [this] { moveSelected(-1); });
        connect(m_moveDownButton, &QPushButton::clicked, this, [this] { moveSelected(1); });
        connect(m_tree, &QTreeWidget::currentItemChanged, this, &PatternLibraryWidget::selectItem);
        m_tree->onRowsReordered = [this] { syncOrderFromTree(); };

        auto *buttonColumn = new QVBoxLayout;
        buttonColumn->addWidget(addButton);
        buttonColumn->addWidget(m_removeButton);
        buttonColumn->addWidget(m_moveUpButton);
        buttonColumn->addWidget(m_moveDownButton);
        buttonColumn->addStretch();

        auto *topRow = new QHBoxLayout;
        topRow->addWidget(m_tree);
        topRow->addLayout(buttonColumn);

        // The detail panel below is always visible (Kits-style) rather than a modal dialog:
        // selecting a row loads it here, and edits apply live -- see fieldEdited().
        m_nameEdit = new QLineEdit(this);
        m_patternEdit = new QLineEdit(this);
        m_regexCheck = new QCheckBox(Tr::tr("Regular expression"), this);
        m_caseSensitiveCheck = new QCheckBox(Tr::tr("Case sensitive"), this);
        m_invertCheck = new QCheckBox(Tr::tr("Exclude matching lines (invert)"), this);
        m_defaultCheck = new QCheckBox(
            Tr::tr("Active by default for new run configurations"), this);
        m_groupContinuationCheck = new QCheckBox(Tr::tr("Group continuation lines"), this);
        m_stdOutCheck = new QCheckBox(Tr::tr("Standard output"), this);
        m_stdErrCheck = new QCheckBox(Tr::tr("Standard error"), this);
        m_messagesCheck = new QCheckBox(Tr::tr("Qt Creator messages"), this);
        m_messagesCheck->setToolTip(
            Tr::tr("Lines Qt Creator adds itself, such as \"Starting ...\" and \"... exited "
                   "with code 0\"."));
        m_continuationPatternEdit = new QLineEdit(Pattern().continuationPattern, this);
        m_colorButton = new QPushButton(Tr::tr("Choose..."), this);
        m_errorLabel = new QLabel(this);
        m_errorLabel->setStyleSheet("color: red;");

        auto *continuationInfoLabel = new QLabel(this);
        continuationInfoLabel->setPixmap(Utils::Icons::INFO.pixmap());
        continuationInfoLabel->setToolTip(
            Tr::tr("<p>Sweeps any line matching the pattern below into this tab, even though "
                   "it doesn't match the pattern above.</p>"
                   "<p>Useful for multi-line entries -- e.g. an error followed by an indented "
                   "stack trace or file excerpt.</p>"
                   "<p>Stops as soon as a line no longer matches the continuation pattern; "
                   "that line is then checked normally against every pattern.</p>"
                   "<p>Swept-in lines don't count toward this tab's match count -- only the "
                   "line that actually matched the pattern above does.</p>"
                   "<p>Leave unchecked for patterns whose matches are always single lines.</p>"));

        connect(m_nameEdit, &QLineEdit::textChanged, this, &PatternLibraryWidget::fieldEdited);
        connect(m_patternEdit, &QLineEdit::textChanged, this, &PatternLibraryWidget::fieldEdited);
        connect(m_regexCheck, &QCheckBox::toggled, this, &PatternLibraryWidget::fieldEdited);
        connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &PatternLibraryWidget::fieldEdited);
        connect(m_invertCheck, &QCheckBox::toggled, this, &PatternLibraryWidget::fieldEdited);
        connect(m_defaultCheck, &QCheckBox::toggled, this, &PatternLibraryWidget::fieldEdited);
        for (QCheckBox *check : {m_stdOutCheck, m_stdErrCheck, m_messagesCheck})
            connect(check, &QCheckBox::toggled, this, &PatternLibraryWidget::fieldEdited);
        connect(m_groupContinuationCheck, &QCheckBox::toggled, this, [this](bool) {
            updateContinuationFieldEnabled();
            fieldEdited();
        });
        connect(
            m_continuationPatternEdit, &QLineEdit::textChanged, this,
            &PatternLibraryWidget::fieldEdited);
        connect(m_colorButton, &QPushButton::clicked, this, &PatternLibraryWidget::pickColor);

        auto *continuationCheckRow = new QHBoxLayout;
        continuationCheckRow->addWidget(m_groupContinuationCheck);
        continuationCheckRow->addWidget(continuationInfoLabel);
        continuationCheckRow->addStretch();

        auto *channelsRow = new QHBoxLayout;
        channelsRow->addWidget(m_stdOutCheck);
        channelsRow->addWidget(m_stdErrCheck);
        channelsRow->addWidget(m_messagesCheck);
        channelsRow->addStretch();

        auto *form = new QFormLayout;
        form->addRow(Tr::tr("Name:"), m_nameEdit);
        form->addRow(Tr::tr("Pattern:"), m_patternEdit);
        form->addRow(m_regexCheck);
        form->addRow(m_caseSensitiveCheck);
        form->addRow(m_invertCheck);
        form->addRow(Tr::tr("Look at:"), channelsRow);
        form->addRow(m_defaultCheck);
        form->addRow(continuationCheckRow);
        form->addRow(Tr::tr("Continuation line starts with:"), m_continuationPatternEdit);
        form->addRow(Tr::tr("Tab color:"), m_colorButton);
        form->addRow(m_errorLabel);

        auto *layout = new QVBoxLayout(this);
        layout->addWidget(placementGroup);
        layout->addWidget(jumpToAllCheck);
        layout->addWidget(focusOnRunStartCheck);
        layout->addWidget(unmatchedCheck);
        layout->addWidget(highlightInAllCheck);
        layout->addLayout(maxLineLengthRow);
        layout->addLayout(maxCharactersRow);
        layout->addLayout(finishedRunsRow);
        layout->addLayout(topRow);
        layout->addLayout(form);

        setFieldsEnabled(false);
    }

    // Add/Edit/Remove only ever touch the working copy, so dismissing the Options dialog
    // with Cancel (which simply never calls apply()) leaves the live library -- and so the
    // on-disk settings, which only change when the library does -- untouched.
    void apply() final
    {
        // setPatterns() persists by itself -- see PatternLibrary::load().
        patternLibrary().setPatterns(m_workingPatterns);
    }

private:
    void promptRestart()
    {
        Core::ICore::askForRestart(
            Tr::tr("The MultiFilterOutput pane's new position takes effect after "
                   "%1 restarts.")
                .arg(QGuiApplication::applicationDisplayName()));
    }

    void refreshTree()
    {
        m_tree->clear();
        for (const Pattern &p : std::as_const(m_workingPatterns))
            m_tree->addTopLevelItem(makeItem(p));
    }

    QTreeWidgetItem *makeItem(const Pattern &p) const
    {
        auto *item = new QTreeWidgetItem;
        // Rows must not be individual drop targets: with the default ItemIsDropEnabled, a
        // drop anywhere but a razor-thin margin at a row's very top/bottom edge nests the
        // dragged item as that row's child instead of reordering it as a sibling, and since
        // this tree is flat, the nested item silently disappears from topLevelItem() (and
        // from m_workingPatterns via syncOrderFromTree()). Clearing the flag makes Qt treat
        // any drop over the row as Above/Below instead -- see
        // QAbstractItemViewPrivate::position() -- giving a full-row reorder target.
        item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
        item->setData(NameColumn, Qt::UserRole, p.id);
        fillItem(item, p);
        return item;
    }

    void fillItem(QTreeWidgetItem *item, const Pattern &p) const
    {
        static const QString checkMark = QString::fromUtf8("\xE2\x9C\x93");

        item->setText(NameColumn, p.name);
        item->setText(PatternColumn, p.patternText);
        item->setText(RegexColumn, p.isRegex ? checkMark : QString());
        item->setText(CaseColumn, p.caseSensitive ? checkMark : QString());
        item->setText(InvertColumn, p.invert ? checkMark : QString());
        item->setText(DefaultColumn, p.isDefault ? checkMark : QString());
        for (int column : {RegexColumn, CaseColumn, InvertColumn, ColorColumn, DefaultColumn})
            item->setTextAlignment(column, Qt::AlignCenter);
        // An invalid color (no tint chosen) should leave the swatch column showing the tree's
        // normal row background, not an arbitrary brush built from an invalid QColor.
        if (p.color.isValid())
            item->setBackground(ColorColumn, p.color);
        else
            item->setData(ColorColumn, Qt::BackgroundRole, QVariant());
    }

    QTreeWidgetItem *itemForId(const QString &id) const
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_tree->topLevelItem(i);
            if (item->data(NameColumn, Qt::UserRole).toString() == id)
                return item;
        }
        return nullptr;
    }

    std::optional<Pattern> patternAt(const QString &id) const
    {
        for (const Pattern &p : m_workingPatterns) {
            if (p.id == id)
                return p;
        }
        return std::nullopt;
    }

    void setFieldsEnabled(bool enabled)
    {
        m_nameEdit->setEnabled(enabled);
        m_patternEdit->setEnabled(enabled);
        m_regexCheck->setEnabled(enabled);
        m_caseSensitiveCheck->setEnabled(enabled);
        m_invertCheck->setEnabled(enabled);
        m_defaultCheck->setEnabled(enabled);
        for (QCheckBox *check : {m_stdOutCheck, m_stdErrCheck, m_messagesCheck})
            check->setEnabled(enabled);
        m_groupContinuationCheck->setEnabled(enabled);
        m_colorButton->setEnabled(enabled);
        m_removeButton->setEnabled(enabled);
        updateContinuationFieldEnabled();
    }

    // The continuation-pattern field is only meaningfully editable when a pattern is selected
    // AND that pattern's "Group continuation lines" checkbox is on.
    void updateContinuationFieldEnabled()
    {
        m_continuationPatternEdit->setEnabled(
            m_groupContinuationCheck->isEnabled() && m_groupContinuationCheck->isChecked());
    }

    void updateMoveButtons()
    {
        const int row = m_tree->indexOfTopLevelItem(m_tree->currentItem());
        m_moveUpButton->setEnabled(row > 0);
        m_moveDownButton->setEnabled(row >= 0 && row < m_tree->topLevelItemCount() - 1);
    }

    void addPattern()
    {
        const Pattern fresh = Pattern::createNew();
        m_workingPatterns.append(fresh);
        m_tree->addTopLevelItem(makeItem(fresh));
        m_tree->setCurrentItem(m_tree->topLevelItem(m_tree->topLevelItemCount() - 1));
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
        Utils::markSettingsDirty();
    }

    void removeSelected()
    {
        QTreeWidgetItem *item = m_tree->currentItem();
        if (!item)
            return;
        const QString id = item->data(NameColumn, Qt::UserRole).toString();
        m_workingPatterns.removeIf([&id](const Pattern &p) { return p.id == id; });
        delete item;
        updateMoveButtons();
        Utils::markSettingsDirty();
    }

    // Swaps the selected pattern with its neighbor in the given direction (-1 up, +1 down),
    // moving the corresponding tree item to match without rebuilding the whole tree.
    void moveSelected(int direction)
    {
        QTreeWidgetItem *item = m_tree->currentItem();
        if (!item)
            return;

        const int row = m_tree->indexOfTopLevelItem(item);
        const int targetRow = row + direction;
        if (targetRow < 0 || targetRow >= m_workingPatterns.size())
            return;

        m_workingPatterns.swapItemsAt(row, targetRow);

        m_tree->takeTopLevelItem(row);
        m_tree->insertTopLevelItem(targetRow, item);
        m_tree->setCurrentItem(item);

        updateMoveButtons();
        Utils::markSettingsDirty();
    }

    // Called after a drag-and-drop reorder: the tree view has already moved the item(s), so
    // rebuild m_workingPatterns from the tree's current top-level order.
    void syncOrderFromTree()
    {
        QList<Pattern> reordered;
        reordered.reserve(m_tree->topLevelItemCount());
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            const QString id = m_tree->topLevelItem(i)->data(NameColumn, Qt::UserRole).toString();
            if (const auto pattern = patternAt(id))
                reordered.append(*pattern);
        }
        m_workingPatterns = reordered;
        updateMoveButtons();
        Utils::markSettingsDirty();
    }

    void selectItem(QTreeWidgetItem *current)
    {
        if (!current) {
            m_currentId.clear();
            m_updatingFields = true;
            m_nameEdit->clear();
            m_patternEdit->clear();
            m_regexCheck->setChecked(false);
            m_caseSensitiveCheck->setChecked(false);
            m_invertCheck->setChecked(false);
            m_defaultCheck->setChecked(false);
            for (QCheckBox *check : {m_stdOutCheck, m_stdErrCheck, m_messagesCheck})
                check->setChecked(true);
            m_groupContinuationCheck->setChecked(false);
            m_continuationPatternEdit->setText(Pattern().continuationPattern);
            m_currentColor = QColor();
            updateColorButton();
            m_errorLabel->clear();
            m_updatingFields = false;
            setFieldsEnabled(false);
            updateMoveButtons();
            return;
        }

        const QString id = current->data(NameColumn, Qt::UserRole).toString();
        const auto pattern = patternAt(id);
        if (!pattern)
            return;

        m_currentId = id;
        m_currentColor = pattern->color;
        m_updatingFields = true;
        m_nameEdit->setText(pattern->name);
        m_patternEdit->setText(pattern->patternText);
        m_regexCheck->setChecked(pattern->isRegex);
        m_caseSensitiveCheck->setChecked(pattern->caseSensitive);
        m_invertCheck->setChecked(pattern->invert);
        m_defaultCheck->setChecked(pattern->isDefault);
        m_stdOutCheck->setChecked(pattern->channels.testFlag(Pattern::StdOutChannel));
        m_stdErrCheck->setChecked(pattern->channels.testFlag(Pattern::StdErrChannel));
        m_messagesCheck->setChecked(pattern->channels.testFlag(Pattern::MessagesChannel));
        m_groupContinuationCheck->setChecked(pattern->groupContinuationLines);
        m_continuationPatternEdit->setText(pattern->continuationPattern);
        updateColorButton();
        m_errorLabel->clear();
        m_updatingFields = false;
        setFieldsEnabled(true);
        updateMoveButtons();
    }

    void updateColorButton()
    {
        // Invalid means "no tint chosen yet"; leave the button's normal appearance rather
        // than painting it as if gray had been explicitly picked.
        m_colorButton->setStyleSheet(
            m_currentColor.isValid()
                ? QStringLiteral("background-color: %1;").arg(m_currentColor.name())
                : QString());
    }

    void pickColor()
    {
        const QColor chosen = QColorDialog::getColor(m_currentColor, this, Tr::tr("Choose Tab Color"));
        if (!chosen.isValid())
            return;
        m_currentColor = chosen;
        updateColorButton();
        fieldEdited();
    }

    // Validates the in-progress edit and, only if valid, writes it back into the working
    // copy and refreshes the tree row -- an invalid regex (or empty pattern text) stays a
    // draft in the form fields, matching the old dialog's "OK disabled until valid"
    // behavior without a modal to gate.
    void fieldEdited()
    {
        if (m_updatingFields || m_currentId.isEmpty())
            return;

        const QString text = m_patternEdit->text();
        bool valid = !text.isEmpty();
        if (valid && m_regexCheck->isChecked())
            valid = QRegularExpression(text).isValid();

        QString error;
        if (!valid) {
            error = Tr::tr("Pattern is empty or not a valid regular expression.");
        } else if (m_groupContinuationCheck->isChecked()) {
            const QString continuationText = m_continuationPatternEdit->text();
            if (continuationText.isEmpty() || !QRegularExpression(continuationText).isValid()) {
                valid = false;
                error = Tr::tr("Continuation pattern is empty or not a valid regular expression.");
            }
        }

        Pattern::Channels channels;
        channels.setFlag(Pattern::StdOutChannel, m_stdOutCheck->isChecked());
        channels.setFlag(Pattern::StdErrChannel, m_stdErrCheck->isChecked());
        channels.setFlag(Pattern::MessagesChannel, m_messagesCheck->isChecked());
        if (valid && !channels) {
            valid = false;
            error = Tr::tr("Choose at least one kind of output to look at.");
        }

        m_errorLabel->setText(error);
        if (!valid)
            return;

        Pattern updated;
        updated.id = m_currentId;
        updated.name = m_nameEdit->text();
        updated.patternText = text;
        updated.isRegex = m_regexCheck->isChecked();
        updated.caseSensitive = m_caseSensitiveCheck->isChecked();
        updated.invert = m_invertCheck->isChecked();
        updated.isDefault = m_defaultCheck->isChecked();
        updated.channels = channels;
        updated.groupContinuationLines = m_groupContinuationCheck->isChecked();
        updated.continuationPattern = m_continuationPatternEdit->text();
        updated.color = m_currentColor;

        for (Pattern &existing : m_workingPatterns) {
            if (existing.id == updated.id) {
                existing = updated;
                break;
            }
        }
        if (QTreeWidgetItem *item = itemForId(m_currentId))
            fillItem(item, updated);

        Utils::markSettingsDirty();
    }

    PatternTreeWidget *m_tree;
    QLineEdit *m_nameEdit;
    QLineEdit *m_patternEdit;
    QCheckBox *m_regexCheck;
    QCheckBox *m_caseSensitiveCheck;
    QCheckBox *m_invertCheck;
    QCheckBox *m_defaultCheck;
    QCheckBox *m_groupContinuationCheck;
    QCheckBox *m_stdOutCheck;
    QCheckBox *m_stdErrCheck;
    QCheckBox *m_messagesCheck;
    QLineEdit *m_continuationPatternEdit;
    QPushButton *m_colorButton;
    QLabel *m_errorLabel;
    QPushButton *m_removeButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QList<Pattern> m_workingPatterns;
    QString m_currentId;
    QColor m_currentColor;
    bool m_updatingFields = false;
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
