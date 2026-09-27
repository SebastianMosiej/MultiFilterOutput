#include "../src/linebuffer.h"

#include <QtTest>

using namespace MultiFilterOutput;

class tst_LineBuffer : public QObject
{
    Q_OBJECT

private slots:
    void singleCompleteLine();
    void splitAcrossChunks();
    void multipleLinesInOneChunk();
    void carriageReturnNewlineHandled();
    void flushReturnsPendingPartialLineOnce();
    void flushDropsTrailingCarriageReturn();
    void carriageReturnKeepsOnlyTextAfterIt();
    void carriageReturnOverwritesAcrossChunks();
    void trailingCarriageReturnsOnCompleteLineAreDropped();
    void progressOutputDoesNotAccumulate();
    void unterminatedOutputIsEmittedInCappedPieces();
    void capAppliesAcrossChunks();
    void longCompleteLineIsSplitToo();
    void capNeverSplitsSurrogatePair();
    void capKeepsTrailingCarriageReturnsPending();
    void capAtExactLengthThenSplitCrLf();
    void splitterKeepsInterleavedFormatsApart();
    void splitterFlushReturnsEveryFormatsPendingLine();
    void creatorMessageEndsPendingAppLines();
};

void tst_LineBuffer::singleCompleteLine()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("hello\n"), QStringList{"hello"});
}

void tst_LineBuffer::splitAcrossChunks()
{
    LineBuffer buf;
    QVERIFY(buf.feed("hel").isEmpty());
    QCOMPARE(buf.feed("lo\n"), QStringList{"hello"});
}

void tst_LineBuffer::multipleLinesInOneChunk()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("one\ntwo\nthree\n"), (QStringList{"one", "two", "three"}));
}

void tst_LineBuffer::carriageReturnNewlineHandled()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("one\r\ntwo\r\n"), (QStringList{"one", "two"}));
}

// A run can end on a line with no trailing newline (a crash message, a final printf); flush()
// is how RunTracker gets it out instead of dropping it.
void tst_LineBuffer::flushReturnsPendingPartialLineOnce()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("one\nlast"), QStringList{"one"});
    QCOMPARE(buf.flush(), QStringLiteral("last"));
    QVERIFY(buf.flush().isEmpty());
    QCOMPARE(buf.feed("next\n"), QStringList{"next"});
}

// The "\r" of a "\r\n" pair can be the last thing a chunk holds; if the run then ends, that
// half of the line ending must not leak into the flushed line.
void tst_LineBuffer::flushDropsTrailingCarriageReturn()
{
    LineBuffer buf;
    QVERIFY(buf.feed("last\r").isEmpty());
    QCOMPARE(buf.flush(), QStringLiteral("last"));
}

// A lone "\r" returns to the start of the line and what follows replaces it, as in a
// terminal and in Application Output (Utils::OutputFormatter::append()); patterns must match
// the text the user actually sees, not text that was overwritten.
void tst_LineBuffer::carriageReturnKeepsOnlyTextAfterIt()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("10%\r20%\r100%\n"), QStringList{"100%"});
}

void tst_LineBuffer::carriageReturnOverwritesAcrossChunks()
{
    LineBuffer buf;
    QVERIFY(buf.feed("abc\r").isEmpty());
    QCOMPARE(buf.feed("de\n"), QStringList{"de"});
}

// Utils::normalizeNewlines() collapses "\r\r\n" to "\n": the "\r"s end the line, they don't
// wipe it.
void tst_LineBuffer::trailingCarriageReturnsOnCompleteLineAreDropped()
{
    LineBuffer buf;
    QCOMPARE(buf.feed("abc\r\r\n"), QStringList{"abc"});
    QVERIFY(buf.feed("xyz\r\r").isEmpty());
    QCOMPARE(buf.feed("\n"), QStringList{"xyz"});
}

// A progress bar redraws its line with "\r" and may not print a newline until it is done;
// only the current state is kept, so nothing piles up or gets force-emitted by the cap.
void tst_LineBuffer::progressOutputDoesNotAccumulate()
{
    LineBuffer buf(16);
    for (int percent = 0; percent <= 100; ++percent)
        QVERIFY(buf.feed(QStringLiteral("progress %1%\r").arg(percent)).isEmpty());
    QCOMPARE(buf.flush(), QStringLiteral("progress 100%"));
}

// Output that never contains a newline (progress bars, binary dumps) must not pile up in the
// pending partial line forever; whole capped pieces come out as soon as they are available.
void tst_LineBuffer::unterminatedOutputIsEmittedInCappedPieces()
{
    LineBuffer buf(10);
    QCOMPARE(buf.feed("abcdefghijklmnopqrstuvwxy"), (QStringList{"abcdefghij", "klmnopqrst"}));
    QCOMPARE(buf.flush(), QStringLiteral("uvwxy"));
}

void tst_LineBuffer::capAppliesAcrossChunks()
{
    LineBuffer buf(10);
    QVERIFY(buf.feed("abcdef").isEmpty());
    QCOMPARE(buf.feed("ghijkl"), QStringList{"abcdefghij"});
    QCOMPARE(buf.feed("\n"), QStringList{"kl"});
}

// Same pieces whether the long line arrived in one chunk or many, so the result doesn't
// depend on how the OS happened to chunk the output.
void tst_LineBuffer::longCompleteLineIsSplitToo()
{
    LineBuffer buf(10);
    QCOMPARE(
        buf.feed("abcdefghijklmnopqrstuvwxy\nshort\n"),
        (QStringList{"abcdefghij", "klmnopqrst", "uvwxy", "short"}));
}

void tst_LineBuffer::capNeverSplitsSurrogatePair()
{
    LineBuffer buf(4);
    // "abc" + U+1F600 (a surrogate pair) + "de": a cut after 4 UTF-16 units would split it.
    const QString text = QStringLiteral("abc") + QString::fromUcs4(U"\U0001F600") + "de";
    QCOMPARE(
        buf.feed(text + "\n"),
        (QStringList{"abc", QString::fromUcs4(U"\U0001F600") + "de"}));
}

// The cap must not force out a pending line just because its trailing "\r"s -- possibly half
// of a "\r\n" -- push it over the limit; they aren't content.
void tst_LineBuffer::capKeepsTrailingCarriageReturnsPending()
{
    LineBuffer buf(10);
    QVERIFY(buf.feed("abcdefghij\r\r").isEmpty());
    QCOMPARE(buf.feed("\n"), QStringList{"abcdefghij"});
}

// Exactly cap-length content whose "\r\n" is split across chunks is one line, not the line
// plus a spurious empty one.
void tst_LineBuffer::capAtExactLengthThenSplitCrLf()
{
    LineBuffer buf(10);
    QVERIFY(buf.feed("abcdefghij\r").isEmpty());
    QCOMPARE(buf.feed("\n"), QStringList{"abcdefghij"});
}

// A stdout fragment still waiting for its newline must not be glued onto a stderr line that
// arrives in between; each stream's line comes out whole, with its own format.
void tst_LineBuffer::splitterKeepsInterleavedFormatsApart()
{
    OutputLineSplitter splitter;
    QVERIFY(splitter.feed("stdout he", Utils::StdOutFormat).isEmpty());

    const QList<OutputLineSplitter::Line> err = splitter.feed("stderr line\n", Utils::StdErrFormat);
    QCOMPARE(err.size(), 1);
    QCOMPARE(err.at(0).text, QStringLiteral("stderr line"));
    QCOMPARE(err.at(0).format, Utils::StdErrFormat);

    const QList<OutputLineSplitter::Line> out = splitter.feed("ad\n", Utils::StdOutFormat);
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.at(0).text, QStringLiteral("stdout head"));
    QCOMPARE(out.at(0).format, Utils::StdOutFormat);
}

void tst_LineBuffer::splitterFlushReturnsEveryFormatsPendingLine()
{
    OutputLineSplitter splitter;
    splitter.feed("err tail", Utils::StdErrFormat);
    splitter.feed("out tail", Utils::StdOutFormat);

    const QList<OutputLineSplitter::Line> flushed = splitter.flush();
    QCOMPARE(flushed.size(), 2);
    QCOMPARE(flushed.at(0).text, QStringLiteral("out tail"));
    QCOMPARE(flushed.at(0).format, Utils::StdOutFormat);
    QCOMPARE(flushed.at(1).text, QStringLiteral("err tail"));
    QCOMPARE(flushed.at(1).format, Utils::StdErrFormat);

    QVERIFY(splitter.flush().isEmpty());
}

// A final "Done." without a newline must come before Qt Creator's own "... exited with code
// 0" in All, as in Application Output -- not after it, when the run is finally untracked.
void tst_LineBuffer::creatorMessageEndsPendingAppLines()
{
    OutputLineSplitter splitter;
    QVERIFY(splitter.feed("Done.", Utils::StdOutFormat).isEmpty());
    QVERIFY(splitter.feed("oops", Utils::StdErrFormat).isEmpty());

    const QList<OutputLineSplitter::Line> lines
        = splitter.feed("MyApp exited with code 0\n", Utils::NormalMessageFormat);

    QCOMPARE(lines.size(), 3);
    QCOMPARE(lines.at(0).text, QStringLiteral("Done."));
    QCOMPARE(lines.at(0).format, Utils::StdOutFormat);
    QCOMPARE(lines.at(1).text, QStringLiteral("oops"));
    QCOMPARE(lines.at(1).format, Utils::StdErrFormat);
    QCOMPARE(lines.at(2).text, QStringLiteral("MyApp exited with code 0"));
    QVERIFY(splitter.flush().isEmpty());
}

QTEST_GUILESS_MAIN(tst_LineBuffer)

#include "tst_linebuffer.moc"
