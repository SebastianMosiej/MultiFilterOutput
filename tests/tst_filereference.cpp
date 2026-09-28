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
    void findsLocations_data();
    void findsLocations();
    void findsSeveralOnOneLine();
    void leavesLookalikesAlone_data();
    void leavesLookalikesAlone();
    void hrefRoundTrips();
};

void tst_FileReference::findsQrcQmlReference()
{
    const QString line
        = "[WARN]: qrc:/qt/qml/QuickView.qml:67:13: Unable to assign [undefined] to bool";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first().target, QStringLiteral("qrc:/qt/qml/QuickView.qml:67:13"));
    QCOMPARE(line.mid(refs.first().start, refs.first().length), refs.first().target);
    QCOMPARE(refs.first().path, QStringLiteral("qrc:/qt/qml/QuickView.qml"));
    QCOMPARE(refs.first().line, 67);
    QCOMPARE(refs.first().column, 13);
}

void tst_FileReference::findsFileUrlReference()
{
    const QString line = "Loaded file:///home/user/project/main.qml:10:5 successfully";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QVERIFY(refs.first().target.startsWith("file:///home/user/project/main.qml:10:5"));
    QCOMPARE(refs.first().path, QStringLiteral("file:///home/user/project/main.qml"));
}

void tst_FileReference::findsAssertFailure()
{
    const QString line = "ASSERT: \"index >= 0\" in file /src/model.cpp, line 42";
    const QList<FileReference> refs = findFileReferences(line);

    QCOMPARE(refs.size(), 1);
    QCOMPARE(refs.first().target, QStringLiteral("/src/model.cpp, line 42"));
    QCOMPARE(refs.first().path, QStringLiteral("/src/model.cpp"));
    QCOMPARE(refs.first().line, 42);
    QCOMPARE(refs.first().column, -1);
}

void tst_FileReference::noMatchOnPlainLine()
{
    QVERIFY(findFileReferences("12:00:00 INFO application started").isEmpty());
}

void tst_FileReference::findsLocations_data()
{
    QTest::addColumn<QString>("line");
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("path");
    QTest::addColumn<int>("lineNumber");
    QTest::addColumn<int>("column");

    QTest::newRow("gcc error") << "src/main.cpp:12:5: error: expected ';'"
                               << "src/main.cpp:12:5" << "src/main.cpp" << 12 << 5;
    QTest::newRow("gcc no column") << "/home/u/a.h:3: warning: unused"
                                   << "/home/u/a.h:3" << "/home/u/a.h" << 3 << -1;
    QTest::newRow("windows path") << "C:\\dev\\app\\main.cpp:12:5: error: oops"
                                  << "C:\\dev\\app\\main.cpp:12:5" << "C:\\dev\\app\\main.cpp"
                                  << 12 << 5;
    QTest::newRow("gtest linux") << "/home/u/foo_test.cc:20: Failure"
                                 << "/home/u/foo_test.cc:20" << "/home/u/foo_test.cc" << 20 << -1;
    QTest::newRow("sanitizer frame") << "    #0 0x4f5a in main /home/u/a.cpp:5:3"
                                     << "/home/u/a.cpp:5:3" << "/home/u/a.cpp" << 5 << 3;
    QTest::newRow("msvc") << "C:\\dev\\app\\main.cpp(12): error C2065: 'x': undeclared"
                          << "C:\\dev\\app\\main.cpp(12)" << "C:\\dev\\app\\main.cpp" << 12 << -1;
    QTest::newRow("msvc column") << "main.cpp(12,5): warning C4100: unused"
                                 << "main.cpp(12,5)" << "main.cpp" << 12 << 5;
    QTest::newRow("gtest windows") << "C:\\src\\foo_test.cc(20): error: Value of: x"
                                   << "C:\\src\\foo_test.cc(20)" << "C:\\src\\foo_test.cc" << 20
                                   << -1;
    QTest::newRow("qtest loc") << "   Loc: [C:\\Program Files\\x\\tst_a.cpp(34)]"
                               << "C:\\Program Files\\x\\tst_a.cpp(34)"
                               << "C:\\Program Files\\x\\tst_a.cpp" << 34 << -1;
    QTest::newRow("python") << "  File \"/home/u/my app.py\", line 7, in main"
                            << "File \"/home/u/my app.py\", line 7" << "/home/u/my app.py" << 7
                            << -1;
    QTest::newRow("object warning")
        << "QObject::connect: No such signal Foo::bar() in C:/dir/main.cpp:42"
        << "C:/dir/main.cpp:42" << "C:/dir/main.cpp" << 42 << -1;
}

void tst_FileReference::findsLocations()
{
    QFETCH(QString, line);
    QFETCH(QString, target);
    QFETCH(QString, path);
    QFETCH(int, lineNumber);
    QFETCH(int, column);

    const QList<FileReference> refs = findFileReferences(line);
    QCOMPARE(refs.size(), 1);
    const FileReference &ref = refs.first();
    QCOMPARE(ref.target, target);
    QCOMPARE(line.mid(ref.start, ref.length), target);
    QCOMPARE(ref.path, path);
    QCOMPARE(ref.line, lineNumber);
    QCOMPARE(ref.column, column);
}

void tst_FileReference::findsSeveralOnOneLine()
{
    const QList<FileReference> refs = findFileReferences(
        "b.h:2:1: note: in expansion of macro, see a.cpp:1");
    QCOMPARE(refs.size(), 2);
    QCOMPARE(refs.at(0).path, QStringLiteral("b.h"));
    QCOMPARE(refs.at(1).path, QStringLiteral("a.cpp"));
}

void tst_FileReference::leavesLookalikesAlone_data()
{
    QTest::addColumn<QString>("line");
    QTest::newRow("time") << "12:00:01 INFO started";
    QTest::newRow("url with port") << "listening on http://example.com:8080/api";
    QTest::newRow("ratio") << "aspect ratio 3.5:1";
    QTest::newRow("version call") << "loaded plugin v1.2(3)";
    QTest::newRow("no extension") << "localhost:8080 refused";
}

void tst_FileReference::leavesLookalikesAlone()
{
    QFETCH(QString, line);
    const QList<FileReference> refs = findFileReferences(line);
    QVERIFY2(refs.isEmpty(), qPrintable(refs.isEmpty() ? QString() : refs.first().target));
}

void tst_FileReference::hrefRoundTrips()
{
    FileReference ref;
    ref.path = "C:\\Program Files\\x\\a.cpp";
    ref.line = 12;
    ref.column = 5;
    const std::optional<FileReference> back = fileReferenceFromHref(fileLinkHref(ref));
    QVERIFY(back);
    QCOMPARE(back->path, ref.path);
    QCOMPARE(back->line, 12);
    QCOMPARE(back->column, 5);

    QVERIFY(!fileReferenceFromHref("not a link"));
}

QTEST_GUILESS_MAIN(tst_FileReference)

#include "tst_filereference.moc"
