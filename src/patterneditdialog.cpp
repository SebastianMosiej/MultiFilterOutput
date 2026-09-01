#include "patterneditdialog.h"

#include "multifilteroutputtr.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace MultiFilterOutput::Internal {

PatternEditDialog::PatternEditDialog(const Pattern &initial, QWidget *parent)
    : QDialog(parent)
    , m_pattern(initial)
{
    setWindowTitle(Tr::tr("Edit Pattern"));

    m_nameEdit = new QLineEdit(m_pattern.name, this);
    m_patternEdit = new QLineEdit(m_pattern.patternText, this);
    m_regexCheck = new QCheckBox(Tr::tr("Regular expression"), this);
    m_regexCheck->setChecked(m_pattern.isRegex);
    m_caseSensitiveCheck = new QCheckBox(Tr::tr("Case sensitive"), this);
    m_caseSensitiveCheck->setChecked(m_pattern.caseSensitive);
    m_invertCheck = new QCheckBox(Tr::tr("Exclude matching lines (invert)"), this);
    m_invertCheck->setChecked(m_pattern.invert);
    m_colorButton = new QPushButton(Tr::tr("Choose..."), this);
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(m_pattern.color.name()));
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color: red;");

    auto *form = new QFormLayout;
    form->addRow(Tr::tr("Name:"), m_nameEdit);
    form->addRow(Tr::tr("Pattern:"), m_patternEdit);
    form->addRow(m_regexCheck);
    form->addRow(m_caseSensitiveCheck);
    form->addRow(m_invertCheck);
    form->addRow(Tr::tr("Tab color:"), m_colorButton);
    form->addRow(m_errorLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);

    connect(m_patternEdit, &QLineEdit::textChanged, this, &PatternEditDialog::updateValidationState);
    connect(m_regexCheck, &QCheckBox::toggled, this, &PatternEditDialog::updateValidationState);
    connect(m_colorButton, &QPushButton::clicked, this, &PatternEditDialog::pickColor);
    updateValidationState();
}

void PatternEditDialog::updateValidationState()
{
    bool valid = !m_patternEdit->text().isEmpty();
    if (valid && m_regexCheck->isChecked())
        valid = QRegularExpression(m_patternEdit->text()).isValid();

    m_errorLabel->setText(
        valid ? QString() : Tr::tr("Pattern is empty or not a valid regular expression."));
    m_okButton->setEnabled(valid);
}

void PatternEditDialog::pickColor()
{
    const QColor chosen = QColorDialog::getColor(m_pattern.color, this, Tr::tr("Choose Tab Color"));
    if (chosen.isValid()) {
        m_pattern.color = chosen;
        m_colorButton->setStyleSheet(QStringLiteral("background-color: %1;").arg(chosen.name()));
    }
}

Pattern PatternEditDialog::result() const
{
    Pattern p = m_pattern;
    p.name = m_nameEdit->text();
    p.patternText = m_patternEdit->text();
    p.isRegex = m_regexCheck->isChecked();
    p.caseSensitive = m_caseSensitiveCheck->isChecked();
    p.invert = m_invertCheck->isChecked();
    return p;
}

} // namespace MultiFilterOutput::Internal
