#include "qtccompat.h"

#include <texteditor/fontsettings.h>
#if !MULTIFILTEROUTPUT_QTC21
#include <coreplugin/icore.h>
#include <texteditor/texteditorsettings.h>
#endif

#include <QObject>

namespace MultiFilterOutput::Compat {

QFont textEditorFont()
{
#if MULTIFILTEROUTPUT_QTC21
    return TextEditor::globalFontSettings().data().font();
#else
    // TextEditorSettings::instance() creates the settings on first use, which takes a running
    // Qt Creator: always the case for the plugin, never for a unit test.
    if (!Core::ICore::instance())
        return QFont();
    return TextEditor::TextEditorSettings::fontSettings().font();
#endif
}

void onTextEditorFontChanged(QObject *context, const std::function<void()> &callback)
{
#if MULTIFILTEROUTPUT_QTC21
    QObject::connect(
        &TextEditor::globalFontSettings(), &TextEditor::FontSettings::changed, context, callback);
#else
    if (!Core::ICore::instance())
        return;
    QObject::connect(
        TextEditor::TextEditorSettings::instance(),
        &TextEditor::TextEditorSettings::fontSettingsChanged,
        context,
        callback);
#endif
}

} // namespace MultiFilterOutput::Compat
