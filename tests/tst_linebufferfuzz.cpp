#include "../src/linebuffer.h"

#include <QRandomGenerator>
#include <QtTest>

using namespace MultiFilterOutput;

// Property tests for LineBuffer and OutputLineSplitter: random output, split into random
// chunks, must produce the same lines as the same output fed in one piece. How the OS and
// Qt Creator happen to chunk a process's output is arbitrary, so any dependence on it shows
// up as lines that differ from run to run.
//
// Seeds are fixed, so a failure is reproducible; its message names the seed and the input.
class tst_LineBufferFuzz : public QObject
{
    Q_OBJECT

private slots:
    void linesWithinCapIgnoreChunking();
    void cappedLinesWithoutLoneCarriageReturnIgnoreChunking();
    void splitterKeepsEachStreamIndependentOfChunking();
};

namespace {

constexpr int Iterations = 2000;

enum class LoneCarriageReturns { Allowed, Excluded };

// Output made of the pieces that matter to LineBuffer: plain text, "\n", "\r\n", a surrogate
// pair that must never be split, and -- unless excluded -- lone "\r" (terminal-style
// overwrite) and runs of them.
QString randomOutput(
    QRandomGenerator &rng, int maxPieces, LoneCarriageReturns cr = LoneCarriageReturns::Allowed)
{
    static const QString pieces[] = {
        "a", "bc", " ", "xyz", "\n", "\r\n", QString::fromUcs4(U"\U0001F600"),
        // Lone carriage returns last, so they can be left out.
        "\r", "\r\r",
    };
    const int pieceCount = int(std::size(pieces)) - (cr == LoneCarriageReturns::Excluded ? 2 : 0);
    QString text;
    const int count = rng.bounded(maxPieces + 1);
    for (int i = 0; i < count; ++i)
        text += pieces[rng.bounded(pieceCount)];
    return text;
}

// `text` cut at random points -- never inside a surrogate pair, which a decoded QString
// chunk never is either.
QStringList randomChunks(QRandomGenerator &rng, const QString &text)
{
    QStringList chunks;
    qsizetype start = 0;
    while (start < text.size()) {
        qsizetype end = start + 1 + rng.bounded(int(qMin<qsizetype>(text.size() - start, 12)));
        if (end < text.size() && text.at(end).isLowSurrogate())
            ++end;
        chunks << text.mid(start, end - start);
        start = end;
    }
    return chunks;
}

QStringList feedAll(LineBuffer &buffer, const QStringList &chunks)
{
    QStringList lines;
    for (const QString &chunk : chunks)
        lines += buffer.feed(chunk);
    const QString last = buffer.flush();
    if (!last.isEmpty())
        lines << last;
    return lines;
}

// A line that begins or ends in the middle of a surrogate pair holds half a character.
bool splitsSurrogatePair(const QString &line)
{
    return !line.isEmpty() && (line.front().isLowSurrogate() || line.back().isHighSurrogate());
}

QString describe(quint32 seed, const QString &text, const QStringList &chunks)
{
    const auto visible = [](QString s) { return s.replace("\r", "\\r").replace("\n", "\\n"); };
    QStringList visibleChunks;
    for (const QString &c : chunks)
        visibleChunks << visible(c);
    return QStringLiteral("seed %1, text \"%2\", chunks [%3]")
        .arg(QString::number(seed), visible(text), visibleChunks.join("|"));
}

} // namespace

// With every line within the cap -- the everyday case -- chunking makes no difference at
// all, whatever mix of line endings and carriage returns the output has.
void tst_LineBufferFuzz::linesWithinCapIgnoreChunking()
{
    for (quint32 seed = 1; seed <= Iterations; ++seed) {
        QRandomGenerator rng(seed);
        const QString text = randomOutput(rng, 40);
        const QStringList chunks = randomChunks(rng, text);

        LineBuffer whole;
        LineBuffer chunked;
        const QStringList expected = feedAll(whole, {text});
        const QStringList actual = feedAll(chunked, chunks);

        QVERIFY2(actual == expected, qPrintable(describe(seed, text, chunks)));
        for (const QString &line : actual)
            QVERIFY2(!splitsSurrogatePair(line), qPrintable(describe(seed, text, chunks)));
    }
}

// Over-long lines are emitted in capped pieces as they arrive (to keep memory bounded), so
// the same text must be cut at the same places however it was chunked, and no piece may
// exceed the cap or split a surrogate pair. "\r\n" line endings are included -- one split
// across chunks right at the cap is exactly where chunking once did matter -- but not lone
// "\r": one arriving after a capped piece was already emitted can't overwrite it any more,
// so there chunking unavoidably matters (the price of keeping memory bounded).
void tst_LineBufferFuzz::cappedLinesWithoutLoneCarriageReturnIgnoreChunking()
{
    constexpr int cap = 7;
    for (quint32 seed = 1; seed <= Iterations; ++seed) {
        QRandomGenerator rng(seed);
        const QString text = randomOutput(rng, 40, LoneCarriageReturns::Excluded);
        const QStringList chunks = randomChunks(rng, text);

        LineBuffer whole(cap);
        LineBuffer chunked(cap);
        const QStringList expected = feedAll(whole, {text});
        const QStringList actual = feedAll(chunked, chunks);

        QVERIFY2(actual == expected, qPrintable(describe(seed, text, chunks)));
        for (const QString &line : actual) {
            QVERIFY2(line.size() <= cap, qPrintable(describe(seed, text, chunks)));
            QVERIFY2(!splitsSurrogatePair(line), qPrintable(describe(seed, text, chunks)));
        }
    }
}

// Two streams (stdout and stderr) interleaved chunk by chunk, in random order and with random
// chunking each: every stream's lines come out exactly as if it had been fed on its own.
void tst_LineBufferFuzz::splitterKeepsEachStreamIndependentOfChunking()
{
    for (quint32 seed = 1; seed <= Iterations; ++seed) {
        QRandomGenerator rng(seed);
        const QString out = randomOutput(rng, 30);
        const QString err = randomOutput(rng, 30);
        QStringList outChunks = randomChunks(rng, out);
        QStringList errChunks = randomChunks(rng, err);

        LineBuffer outAlone;
        LineBuffer errAlone;
        const QStringList expectedOut = feedAll(outAlone, {out});
        const QStringList expectedErr = feedAll(errAlone, {err});

        OutputLineSplitter splitter;
        QList<OutputLineSplitter::Line> lines;
        while (!outChunks.isEmpty() || !errChunks.isEmpty()) {
            const bool takeOut = errChunks.isEmpty() || (!outChunks.isEmpty() && rng.bounded(2) == 0);
            if (takeOut)
                lines += splitter.feed(outChunks.takeFirst(), Utils::StdOutFormat);
            else
                lines += splitter.feed(errChunks.takeFirst(), Utils::StdErrFormat);
        }
        lines += splitter.flush();

        QStringList actualOut;
        QStringList actualErr;
        for (const OutputLineSplitter::Line &line : std::as_const(lines))
            (line.format == Utils::StdOutFormat ? actualOut : actualErr) << line.text;

        QVERIFY2(actualOut == expectedOut, qPrintable("stdout: " + describe(seed, out, {})));
        QVERIFY2(actualErr == expectedErr, qPrintable("stderr: " + describe(seed, err, {})));
    }
}

QTEST_GUILESS_MAIN(tst_LineBufferFuzz)

#include "tst_linebufferfuzz.moc"
