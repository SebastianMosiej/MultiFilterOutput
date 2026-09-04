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

QTEST_GUILESS_MAIN(tst_FilterSetAspect)

#include "tst_filtersetaspect.moc"
