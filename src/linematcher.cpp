#include "linematcher.h"

#include "pattern.h"

#include <QRegularExpression>

namespace MultiFilterOutput {

bool lineMatches(const Pattern &pattern, const QString &line)
{
    bool matched = false;
    if (pattern.isRegex) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!pattern.caseSensitive)
            options |= QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression re(pattern.patternText, options);
        matched = re.match(line).hasMatch();
    } else {
        const Qt::CaseSensitivity cs = pattern.caseSensitive ? Qt::CaseSensitive
                                                             : Qt::CaseInsensitive;
        matched = line.contains(pattern.patternText, cs);
    }
    return matched != pattern.invert;
}

} // namespace MultiFilterOutput
