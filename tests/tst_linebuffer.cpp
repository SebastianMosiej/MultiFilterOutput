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
    void splitterKeepsInterleavedFormatsApart();
    void splitterFlushReturnsEveryFormatsPendingLine();
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

QTEST_GUILESS_MAIN(tst_LineBuffer)

#include "tst_linebuffer.moc"
