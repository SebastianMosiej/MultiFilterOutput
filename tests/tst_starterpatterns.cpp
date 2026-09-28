#include "../src/linematcher.h"
#include "../src/linerouter.h"
#include "../src/patternlibrary.h"
#include "../src/starterpatterns.h"

#include <QSet>
#include <QtTest>

using namespace MultiFilterOutput;

class tst_StarterPatterns : public QObject
{
    Q_OBJECT

private slots:
    void areValidTaggedAndHaveFixedUniqueIds();
    void matchWhatTheyAreFor_data();
    void matchWhatTheyAreFor();
    void qtTestFailuresKeepTheirLocationLine();
    void addingThemSkipsWhatTheLibraryHas();
};

static Pattern starter(const QString &name)
{
    for (const Pattern &p : starterPatterns()) {
        if (p.name == name)
            return p;
    }
    qFatal("no starter pattern named %s", qPrintable(name));
    return {};
}

void tst_StarterPatterns::areValidTaggedAndHaveFixedUniqueIds()
{
    QList<Pattern> patterns = starterPatterns();
    QCOMPARE(patterns.size(), 5);
    QVERIFY(validatePatterns(patterns).isEmpty());
    QSet<QString> ids;
    for (const Pattern &p : patterns) {
        QVERIFY(p.id.startsWith("MultiFilterOutput.Starter."));
        ids.insert(p.id);
        QVERIFY(p.tags.contains("starter"));
        QVERIFY(!p.isDefault);
        QVERIFY(p.color.isValid());
    }
    QCOMPARE(ids.size(), patterns.size());
    QCOMPARE(starterPatterns().first().id, patterns.first().id); // the same every time
}

void tst_StarterPatterns::matchWhatTheyAreFor_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QString>("line");
    QTest::addColumn<bool>("matches");

    QTest::newRow("error") << "Errors" << "main.cpp:12: error: expected ';'" << true;
    QTest::newRow("ERROR") << "Errors" << "12:00:01 ERROR connection refused" << true;
    QTest::newRow("fatal") << "Errors" << "Fatal: out of memory" << true;
    QTest::newRow("errors count") << "Errors" << "0 errors, 2 warnings" << false;
    QTest::newRow("terror") << "Errors" << "a tale of terror" << false;

    QTest::newRow("warning") << "Warnings" << "main.cpp:3: warning: unused variable" << true;
    QTest::newRow("WARN") << "Warnings" << "12:00:01 WARN slow response" << true;
    QTest::newRow("warnings count") << "Warnings" << "0 errors, 2 warnings" << false;

    QTest::newRow("console.log") << "QML" << "qml: button clicked" << true;
    QTest::newRow("qml file")
        << "QML" << "qrc:/main.qml:12:5: ReferenceError: foo is not defined" << true;
    QTest::newRow("not qml") << "QML" << "loading data.qmlx" << false;

    QTest::newRow("Q_ASSERT")
        << "Assertions" << "ASSERT: \"ptr\" in file widget.cpp, line 42" << true;
    QTest::newRow("Q_ASSERT_X")
        << "Assertions" << "ASSERT failure in QList::at: \"index out of range\"" << true;
    QTest::newRow("msvc assert")
        << "Assertions" << "Assertion failed: x > 0, file main.c, line 5" << true;
    QTest::newRow("glibc assert")
        << "Assertions" << "app: main.c:5: main: Assertion `x > 0' failed." << true;
    QTest::newRow("no assert") << "Assertions" << "assertive design" << false;

    QTest::newRow("FAIL!") << "Qt Test failures" << "FAIL!  : tst_Foo::bar() Compared values" << true;
    QTest::newRow("XPASS") << "Qt Test failures" << "XPASS  : tst_Foo::baz() unexpected" << true;
    QTest::newRow("PASS") << "Qt Test failures" << "PASS   : tst_Foo::bar()" << false;
}

void tst_StarterPatterns::matchWhatTheyAreFor()
{
    QFETCH(QString, pattern);
    QFETCH(QString, line);
    QFETCH(bool, matches);
    QCOMPARE(lineMatches(starter(pattern), line), matches);
}

// QtTest prints where a failure happened on the next, indented line.
void tst_StarterPatterns::qtTestFailuresKeepTheirLocationLine()
{
    LineRouter router({starter("Qt Test failures")});
    QCOMPARE(router.route("FAIL!  : tst_Foo::bar() Compared values", Utils::StdOutFormat).routes.size(), 1);
    const LineRouter::Result location = router.route(
        "   Loc: [tst_foo.cpp(12)]", Utils::StdOutFormat);
    QCOMPARE(location.routes.size(), 1);
    QVERIFY(!location.routes.first().countsAsMatch);
}

void tst_StarterPatterns::addingThemSkipsWhatTheLibraryHas()
{
    Pattern mine;
    mine.id = "mine";
    mine.name = "Mine";
    Pattern editedErrors = starter("Errors");
    editedErrors.name = "My errors";

    const QList<Pattern> result = withStarterPatterns({mine, editedErrors});

    QCOMPARE(result.size(), 1 + starterPatterns().size());
    QCOMPARE(result.at(0).id, QStringLiteral("mine"));
    QCOMPARE(result.at(1).name, QStringLiteral("My errors")); // left as edited
    QCOMPARE(withStarterPatterns(result).size(), result.size()); // nothing twice
}

QTEST_GUILESS_MAIN(tst_StarterPatterns)

#include "tst_starterpatterns.moc"
