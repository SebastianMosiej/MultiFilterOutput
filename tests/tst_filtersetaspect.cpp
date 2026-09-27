#include "../src/filtersetaspect.h"
#include "../src/pattern.h"
#include "../src/patternlibrary.h"

#include <utils/store.h>

#include <QtTest>

using namespace MultiFilterOutput;

class tst_FilterSetAspect : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToEmpty();
    void setterGetterRoundTrip();
    void mapRoundTrip();
    void seedsFromDefaultPatterns();
    void forgetsDeletedPatterns();
    void keepsIdsOfPatternsNotDeleted();
};

void tst_FilterSetAspect::defaultsToEmpty()
{
    FilterSetAspect aspect(nullptr);
    QVERIFY(aspect.enabledPatternIds().isEmpty());
}

void tst_FilterSetAspect::setterGetterRoundTrip()
{
    FilterSetAspect aspect(nullptr);
    aspect.setEnabledPatternIds({"id-1", "id-2"});
    QCOMPARE(aspect.enabledPatternIds(), (QStringList{"id-1", "id-2"}));
}

void tst_FilterSetAspect::mapRoundTrip()
{
    FilterSetAspect aspect(nullptr);
    aspect.setEnabledPatternIds({"id-1", "id-2"});

    Utils::Store map;
    aspect.toMap(map);

    FilterSetAspect restored(nullptr);
    restored.fromMap(map);

    QCOMPARE(restored.enabledPatternIds(), (QStringList{"id-1", "id-2"}));
}

void tst_FilterSetAspect::seedsFromDefaultPatterns()
{
    Pattern defaultOn;
    defaultOn.id = "default-on";
    defaultOn.isDefault = true;
    Pattern defaultOff;
    defaultOff.id = "default-off";
    defaultOff.isDefault = false;
    patternLibrary().setPatterns({defaultOn, defaultOff});

    // A newly constructed aspect (mirroring a brand-new run configuration, which -- unlike a
    // restored one -- never calls fromMap()) should pick up only the default-flagged pattern.
    FilterSetAspect aspect(nullptr);
    QCOMPARE(aspect.enabledPatternIds(), (QStringList{"default-on"}));

    patternLibrary().setPatterns({});
}

// A deleted pattern's id used to stay in every run configuration's selection -- and on disk
// -- forever, skipped only when a run started.
void tst_FilterSetAspect::forgetsDeletedPatterns()
{
    Pattern a;
    a.id = "id-a";
    Pattern b;
    b.id = "id-b";
    patternLibrary().setPatterns({a, b});
    FilterSetAspect aspect(nullptr);
    aspect.setEnabledPatternIds({"id-a", "id-b"});

    patternLibrary().remove("id-a");
    QCOMPARE(aspect.enabledPatternIds(), QStringList{"id-b"});

    patternLibrary().setPatterns({});
    QVERIFY(aspect.enabledPatternIds().isEmpty());

    Utils::Store map;
    aspect.toMap(map);
    QVERIFY(map.value("MultiFilterOutput.FilterSetAspect").toStringList().isEmpty());
}

// Only actual deletions prune: an id the library simply doesn't know (e.g. one restored from
// disk before the library had it) is left alone -- the pruning must never be able to wipe a
// selection just because the library came up empty.
void tst_FilterSetAspect::keepsIdsOfPatternsNotDeleted()
{
    Pattern a;
    a.id = "id-a";
    patternLibrary().setPatterns({a});
    FilterSetAspect aspect(nullptr);
    aspect.setEnabledPatternIds({"id-a", "unknown-id"});

    Pattern c;
    c.id = "id-c";
    patternLibrary().addOrUpdate(c);

    QCOMPARE(aspect.enabledPatternIds(), (QStringList{"id-a", "unknown-id"}));
    patternLibrary().setPatterns({});
}

QTEST_GUILESS_MAIN(tst_FilterSetAspect)

#include "tst_filtersetaspect.moc"
