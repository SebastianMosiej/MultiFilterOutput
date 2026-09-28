#pragma once

// What differs between the Qt Creator versions the plugin builds against: 21 (plugin API
// 20.0.82) and 20.x. CMakeLists.txt sets MULTIFILTEROUTPUT_QTC21 from the Qt Creator it finds.
//  - 21 folded the Aggregation library into Utils (Utils::Aggregation);
//  - 21 reaches the text editor font through TextEditor::globalFontSettings(), 20 through
//    TextEditor::TextEditorSettings;
//  - RunControl::isOutputVisible()/outputVisibilityChanged() are new in 21 -- see
//    RunTracker::canFollowApplicationOutput().

#include "multifilteroutput_global.h"

#include <QFont>

#include <functional>

#if MULTIFILTEROUTPUT_QTC21
#include <utils/aggregate.h>
#else
#include <aggregation/aggregate.h>
#endif

class QObject;

namespace MultiFilterOutput::Compat {

#if MULTIFILTEROUTPUT_QTC21
namespace Aggregation = ::Utils::Aggregation;
#else
namespace Aggregation = ::Aggregation;
#endif

// The font of Preferences > Text Editor > Font & Colors -- what Application Output uses.
MULTIFILTEROUTPUT_EXPORT QFont textEditorFont();
// Calls `callback` whenever that font changes, for as long as `context` lives.
MULTIFILTEROUTPUT_EXPORT void onTextEditorFontChanged(
    QObject *context, const std::function<void()> &callback);

} // namespace MultiFilterOutput::Compat
