#include "../src/pattern.h"

#include <QtTest>

using namespace MultiFilterOutput;

class tst_Pattern : public QObject
{
    Q_OBJECT

private slots:
    void roundTripsThroughMap();
    void createNewGeneratesUniqueIds();
    void continuationFieldsDefaultAndRoundTrip();
    void isDefaultDefaultsFalseAndRoundTrips();
    void grayRoundTripsLikeAnyOtherColor();
    void channelsDefaultToAllAndRoundTrip();
    void channelOfSortsFormats();
    void looksAtOnlyItsChannels();
};

void tst_Pattern::roundTripsThroughMap()
{
    Pattern p;
    p.id = "abc-123";
    p.name = "Errors";
    p.patternText = "ERROR";
    p.isRegex = false;
    p.caseSensitive = true;
    p.invert = false;
    p.color = QColor(Qt::red);

    const Pattern restored = Pattern::fromMap(p.toMap());

    QCOMPARE(restored.id, p.id);
    QCOMPARE(restored.name, p.name);
    QCOMPARE(restored.patternText, p.patternText);
    QCOMPARE(restored.isRegex, p.isRegex);
    QCOMPARE(restored.caseSensitive, p.caseSensitive);
    QCOMPARE(restored.invert, p.invert);
    QCOMPARE(restored.color, p.color);
}

void tst_Pattern::continuationFieldsDefaultAndRoundTrip()
{
    // A freshly constructed Pattern (e.g. from createNew()) has continuation grouping off,
    // but a sensible default pattern already filled in for whenever it's turned on.
    QCOMPARE(Pattern().groupContinuationLines, false);
    QCOMPARE(Pattern().continuationPattern, QStringLiteral("^ {1,3}"));

    Pattern p;
    p.groupContinuationLines = true;
    p.continuationPattern = "^\\t";

    const Pattern restored = Pattern::fromMap(p.toMap());
    QCOMPARE(restored.groupContinuationLines, p.groupContinuationLines);
    QCOMPARE(restored.continuationPattern, p.continuationPattern);
}

void tst_Pattern::isDefaultDefaultsFalseAndRoundTrips()
{
    QCOMPARE(Pattern().isDefault, false);

    Pattern p;
    p.isDefault = true;
    const Pattern restored = Pattern::fromMap(p.toMap());
    QCOMPARE(restored.isDefault, true);
}

void tst_Pattern::createNewGeneratesUniqueIds()
{
    const Pattern a = Pattern::createNew();
    const Pattern b = Pattern::createNew();
    QVERIFY(!a.id.isEmpty());
    QVERIFY(a.id != b.id);
}

// The legacy "gray means no tint" migration belongs to loading old settings (see
// PatternLibrary::load()), not to fromMap(): a gray the user picks on purpose must survive.
void tst_Pattern::grayRoundTripsLikeAnyOtherColor()
{
    Pattern p;
    p.color = QColor(Qt::gray);
    QCOMPARE(Pattern::fromMap(p.toMap()).color, QColor(Qt::gray));
}

QTEST_GUILESS_MAIN(tst_Pattern)

void tst_Pattern::channelsDefaultToAllAndRoundTrip()
{
    QCOMPARE(Pattern().channels, Pattern::Channels(Pattern::AllChannels));
    // Settings from before channels existed.
    QCOMPARE(Pattern::fromMap({{"id", "x"}}).channels, Pattern::Channels(Pattern::AllChannels));

    Pattern p;
    p.channels = Pattern::StdErrChannel | Pattern::MessagesChannel;
    QCOMPARE(Pattern::fromMap(p.toMap()).channels, p.channels);

    // No channel at all would make a pattern that can never match: read as all.
    p.channels = {};
    QCOMPARE(Pattern::fromMap(p.toMap()).channels, Pattern::Channels(Pattern::AllChannels));
}

void tst_Pattern::channelOfSortsFormats()
{
    QCOMPARE(Pattern::channelOf(Utils::StdOutFormat), Pattern::StdOutChannel);
    QCOMPARE(Pattern::channelOf(Utils::StdErrFormat), Pattern::StdErrChannel);
    for (Utils::OutputFormat format :
         {Utils::NormalMessageFormat, Utils::ErrorMessageFormat, Utils::LogMessageFormat,
          Utils::DebugFormat, Utils::GeneralMessageFormat}) {
        QCOMPARE(Pattern::channelOf(format), Pattern::MessagesChannel);
    }
}

void tst_Pattern::looksAtOnlyItsChannels()
{
    Pattern p;
    QVERIFY(p.looksAt(Utils::StdErrFormat));
    p.channels = Pattern::StdOutChannel;
    QVERIFY(p.looksAt(Utils::StdOutFormat));
    QVERIFY(!p.looksAt(Utils::StdErrFormat));
    QVERIFY(!p.looksAt(Utils::NormalMessageFormat));
}

#include "tst_pattern.moc"
