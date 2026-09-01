#include "../src/filereference.h"

#include <QtTest>

using namespace MultiFilterOutput;

class tst_FileReference : public QObject
{
    Q_OBJECT

private slots:
    void findsQrcQmlReference();
    void findsFileUrlReference();
    void findsAssertFailure();
    void noMatchOnPlainLine();
};

void tst_FileReference::findsQrcQmlReference()
{
    const QString line
        = "[WARN]: qrc:/qt/qml/QuickView.qml:67:13: Unable to assign [undefined] to bool";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first().target, QStringLiteral("qrc:/qt/qml/QuickView.qml:67:13"));
    QCOMPARE(line.mid(refs.first().start, refs.first().length), refs.first().target);
}

void tst_FileReference::findsFileUrlReference()
{
    const QString line = "Loaded file:///home/user/project/main.qml:10:5 successfully";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QVERIFY(refs.first().target.startsWith("file:///home/user/project/main.qml:10:5"));
}

void tst_FileReference::findsAssertFailure()
{
    const QString line = "ASSERT: \"index >= 0\" in file /src/model.cpp, line 42";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first().target, QStringLiteral("/src/model.cpp, line 42"));
}

void tst_FileReference::noMatchOnPlainLine()
{
    QVERIFY(findFileReferences("12:00:00 INFO application started").isEmpty());
}

QTEST_GUILESS_MAIN(tst_FileReference)

#include "tst_filereference.moc"
