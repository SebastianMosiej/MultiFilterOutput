#pragma once

#include "pattern.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLineEdit;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace MultiFilterOutput::Internal {

class PatternEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatternEditDialog(const Pattern &initial, QWidget *parent = nullptr);

    Pattern result() const;

private:
    void updateValidationState();
    void pickColor();

    Pattern m_pattern;
    QLineEdit *m_nameEdit;
    QLineEdit *m_patternEdit;
    QCheckBox *m_regexCheck;
    QCheckBox *m_caseSensitiveCheck;
    QCheckBox *m_invertCheck;
    QPushButton *m_colorButton;
    QLabel *m_errorLabel;
    QPushButton *m_okButton;
};

} // namespace MultiFilterOutput::Internal
