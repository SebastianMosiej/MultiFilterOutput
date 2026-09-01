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

QTEST_GUILESS_MAIN(tst_LineBuffer)

#include "tst_linebuffer.moc"
