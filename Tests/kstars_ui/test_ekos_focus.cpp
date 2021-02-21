/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_focus.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "Options.h"

class KFocusProcedureSteps: public QObject
{
public:
    QMetaObject::Connection starting;
    QMetaObject::Connection aborting;
    QMetaObject::Connection completing;
    QMetaObject::Connection notguiding;
    QMetaObject::Connection guiding;
    QMetaObject::Connection quantifying;

public:
    bool started { false };
    bool aborted { false };
    bool complete { false };
    bool unguided { false };
    double hfr { -2 };

public:
    KFocusProcedureSteps():
        starting (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() { started = true; }, Qt::UniqueConnection)),
        aborting (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() { started = false; aborted = true; }, Qt::UniqueConnection)),
        completing (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, this, [&]() { started = false; complete = true; }, Qt::UniqueConnection)),
        notguiding (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::suspendGuiding, this, [&]() { unguided = true; }, Qt::UniqueConnection)),
        guiding (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::resumeGuiding, this, [&]() { unguided = false; }, Qt::UniqueConnection)),
        quantifying (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newHFR, this, [&](double _hfr) { hfr = _hfr; }, Qt::UniqueConnection))
    {};
    virtual ~KFocusProcedureSteps() {
        disconnect(starting);
        disconnect(aborting);
        disconnect(completing);
        disconnect(notguiding);
        disconnect(guiding);
        disconnect(quantifying);
    };
};

class KFocusStateList: public QObject, public QList <Ekos::FocusState>
{
public:
    QMetaObject::Connection handler;

public:
    KFocusStateList():
        handler (connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::newStatus, this, [&](Ekos::FocusState s) { append(s); }, Qt::UniqueConnection))
    {};
    virtual ~KFocusStateList() {};
};

TestEkosFocus::TestEkosFocus(QObject *parent) : QObject(parent)
{
}

void TestEkosFocus::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosFocus::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFocus::init()
{

}

void TestEkosFocus::cleanup()
{

}

void TestEkosFocus::testCaptureStates()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Pre-configure steps
    KTRY_FOCUS_MOVETO(40000);

    // Prepare to detect state change
    KFocusStateList state_list;
    QVERIFY(state_list.handler);

    // Configure some fields, capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(2, 1, 99);
    QTRY_COMPARE_WITH_TIMEOUT(state_list.count(), 2, 5000);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_IDLE);
    state_list.clear();

    // Move step value, expect no capture
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_MOVETO(43210);
    QTest::qWait(1000);
    QCOMPARE(state_list.count(), 0);

    KTRY_FOCUS_GADGET(QPushButton, startLoopB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);

    // Loop captures, abort, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_CLICK(startLoopB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 1, 5000);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 4, 5000);
    QCOMPARE((int)state_list[0], (int)Ekos::FocusState::FOCUS_FRAMING);
    QCOMPARE((int)state_list[1], (int)Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE((int)state_list[2], (int)Ekos::FocusState::FOCUS_ABORTED);
    QCOMPARE((int)state_list[3], (int)Ekos::FocusState::FOCUS_FAILED);
    state_list.clear();

    KTRY_FOCUS_GADGET(QCheckBox, useAutoStar);
    KTRY_FOCUS_GADGET(QPushButton, resetFrameB);

    QWARN("This test does not wait for the hardcoded timeout to select a star.");

    // Use successful automatic star selection (not full-field), capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    useAutoStar->setCheckState(Qt::CheckState::Checked);
    KTRY_FOCUS_DETECT(2, 1, 99);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    KTRY_FOCUS_CLICK(resetFrameB);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_IDLE);
    useAutoStar->setCheckState(Qt::CheckState::Unchecked);
    state_list.clear();

    // Use unsuccessful automatic star selection, capture, check states
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    useAutoStar->setCheckState(Qt::CheckState::Checked);
    KTRY_FOCUS_DETECT(0.01, 1, 1);
    QTRY_VERIFY_WITH_TIMEOUT(state_list.count() >= 2, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    KTRY_FOCUS_CLICK(resetFrameB);
    QCOMPARE(state_list[0], Ekos::FocusState::FOCUS_PROGRESS);
    QCOMPARE(state_list[1], Ekos::FocusState::FOCUS_WAITING);
    useAutoStar->setCheckState(Qt::CheckState::Unchecked);
    state_list.clear();
}

void TestEkosFocus::testDuplicateFocusRequest()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);

    // If we click the autofocus button, we receive a signal that the procedure starts, the state changes and the button is disabled
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(Ekos::Manager::Instance()->focusModule()->status() != Ekos::FOCUS_IDLE);
    QVERIFY(!startFocusB->isEnabled());
    QVERIFY(stopFocusB->isEnabled());

    // If we issue an autofocus command at that point (bypassing d-bus), no procedure should start
    for (int i = 0; i < 5; i++)
    {
        autofocus.started = false;
        Ekos::Manager::Instance()->focusModule()->start();
        QTest::qWait(500);
        QVERIFY(!autofocus.started);
    }

    // Stop the running autofocus
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
}

void TestEkosFocus::testAutofocusSignalEmission()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);

    // Prepare to restart the autofocus procedure immediately when it finishes
    volatile bool ran_once = false;
    autofocus.completing = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusComplete, &autofocus, [&]() {
        autofocus.complete = true;
        autofocus.started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    }, Qt::UniqueConnection);
    QVERIFY(autofocus.completing);

    // Run the autofocus, wait for the completion signal and restart a second one immediately
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);

    // Wait for the second run to finish
    autofocus.complete = false;
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);

    // No other autofocus started after that
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusAbort()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus, pre-set to near-optimal 38500 steps
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.completing);

    // Prepare to restart the autofocus procedure immediately when it finishes
    volatile bool ran_once = false;
    autofocus.aborting = connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusAborted, this, [&]() {
        autofocus.aborted = true;
        autofocus.started = false;
        if (!ran_once)
        {
            Ekos::Manager::Instance()->focusModule()->start();
            ran_once = true;
        }
    }, Qt::UniqueConnection);
    QVERIFY(autofocus.aborting);

    // Run the autofocus, don't wait for the completion signal, abort it and restart a second one immediately
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 1000);

    // Wait for the second run to finish
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);

    // No other autofocus started after that
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testGuidingSuspend()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a fast autofocus
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3);
    KTRY_FOCUS_EXPOSURE(1, 99);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);
    QVERIFY(autofocus.notguiding);
    QVERIFY(autofocus.guiding);

    KTRY_FOCUS_GADGET(QCheckBox, suspendGuideCheck);

    // Abort the autofocus with guiding set to suspend, guiding will be required to suspend, then required to resume
    suspendGuideCheck->setCheckState(Qt::CheckState::Checked);
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    QVERIFY(!autofocus.unguided);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(autofocus.unguided);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
    QVERIFY(!autofocus.unguided);

    // Run the autofocus to completion with guiding set to suspend, guiding will be required to suspend, then required to resume
    autofocus.started = autofocus.aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(autofocus.unguided);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.unguided);

    // No other autofocus started after that
    QVERIFY(!autofocus.started);

    // Abort the autofocus with guiding set to continue, no guiding signal will be emitted
    suspendGuideCheck->setCheckState(Qt::CheckState::Unchecked);
    autofocus.started = autofocus.aborted = autofocus.complete = autofocus.unguided = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(!autofocus.unguided);
    Ekos::Manager::Instance()->focusModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 5000);
    QVERIFY(!autofocus.unguided);

    // Run the autofocus to completion with guiding set to continue, no guiding signal will be emitted
    autofocus.started = autofocus.aborted = false;
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QVERIFY(!autofocus.unguided);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 30000);
    QVERIFY(!autofocus.unguided);

    // No other autofocus started after that
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusWhenGuidingResumes()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a successful pre-focus capture
    KTRY_FOCUS_MOVETO(50000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 50);
    KTRY_FOCUS_EXPOSURE(2, 1);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);

    // Run a standard autofocus
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);

    // Wait a little, then run a capture check with an unachievable HFR
    QTest::qWait(3000);
    QWARN("Requesting a first in-sequence HFR check, letting it complete...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure succeeds
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 60000);

    // Run again a capture check
    autofocus.complete = false;
    QWARN("Requesting a second in-sequence HFR check, aborting it...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure starts properly
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);

    // Abort the procedure manually, and run again a capture check that will fail
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 10000);
    QWARN("Requesting a third in-sequence HFR check, making it fail during autofocus...");
    autofocus.aborted = autofocus.complete = false;
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure starts properly, after acknowledging there is an autofocus to run
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);

    // Change settings so that the procedure fails now
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 0.1, 0.1);
    KTRY_FOCUS_EXPOSURE(0.1, 1);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 60000);

    // Run again a capture check
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 20);
    KTRY_FOCUS_EXPOSURE(2, 1);
    autofocus.aborted = autofocus.complete = false;
    QWARN("Requesting a fourth in-sequence HFR check, making it succeed...");
    Ekos::Manager::Instance()->focusModule()->checkFocus(0.1);

    // Procedure succeeds
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.complete, 60000);
}

void TestEkosFocus::testFocusFailure()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a failing autofocus - small gain, small exposure, small frame filter
    KTRY_FOCUS_MOVETO(10000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 1.0, 0.1);
    KTRY_FOCUS_EXPOSURE(0.01, 1);

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    // Prepare to detect the beginning of the autofocus_procedure
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.starting);
    QVERIFY(autofocus.aborting);
    QVERIFY(autofocus.completing);

    // Run the autofocus, wait for the completion signal
    QVERIFY(!autofocus.started);
    QVERIFY(!autofocus.aborted);
    QVERIFY(!autofocus.complete);
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.started, 500);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus.aborted, 30000);

    // No other autofocus started after that, we are not running a sequence focus
    QVERIFY(!autofocus.started);
}

void TestEkosFocus::testFocusOptions()
{
    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    // Sync high on meridian to avoid issues with CCD Simulator
    KTRY_FOCUS_SYNC(60.0);

    // Configure a proper autofocus
    KTRY_FOCUS_MOVETO(40000);
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3);
    KTRY_FOCUS_EXPOSURE(1, 99);

    // Prepare to detect a new HFR
    KFocusProcedureSteps autofocus;
    QVERIFY(autofocus.quantifying);

    KTRY_FOCUS_GADGET(QPushButton, captureB);

    // Filter to apply to frame after capture
    // This option is tricky: it follows the FITSViewer::filterTypes filter list, but
    // is used as a list of filter that may be applied in the FITS view.
    // In the Focus view, it also requires the "--" item as no-op filter, present in the
    // combobox stating which filter to apply to frames before analysing them.
    {
        // Validate the default Ekos option is recognised as a filter
        int const fe = Options::focusEffect();
        QVERIFY(0 <= fe && fe < FITSViewer::filterTypes.count() + 1);
        QWARN(qPrintable(QString("Default filtering option is %1/%2").arg(fe).arg(FITSViewer::filterTypes.value(fe - 1, "--"))));

        // Validate the UI changes the Ekos option
        for (int i = 0; i < FITSViewer::filterTypes.count(); i++)
        {
            QTRY_VERIFY_WITH_TIMEOUT(captureB->isEnabled(), 5000);

            QString const & filterType = FITSViewer::filterTypes.value(i, "Unknown image filter");
            QWARN(qPrintable(QString("Testing filtering option %1/%2").arg(i + 1).arg(filterType)));

            // Set filter to apply in the UI, verify impact on Ekos option
            Options::setFocusEffect(fe);
            KTRY_FOCUS_COMBO_SET(filterCombo, filterType);
            QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) i + 1, 1000);

            // Set filter to apply with the d-bus entry point, verify impact on Ekos option
            Options::setFocusEffect(fe);
            Ekos::Manager::Instance()->focusModule()->setImageFilter(filterType);
            QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) i + 1, 1000);

            // Run a capture with detection for coverage
            autofocus.hfr = -2;
            KTRY_FOCUS_CLICK(captureB);
            QTRY_VERIFY_WITH_TIMEOUT(-1 <= autofocus.hfr, 5000);
        }

        // Set no-op filter to apply in the UI, verify impact on Ekos option
        Options::setFocusEffect(0);
        KTRY_FOCUS_COMBO_SET(filterCombo, "--");
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) 0, 1000);

        // Set no-op filter to apply with the d-bus entry point, verify impact on Ekos option
        Options::setFocusEffect(0);
        Ekos::Manager::Instance()->focusModule()->setImageFilter("--");
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) 0, 1000);

        // Restore the original Ekos option
        KTRY_FOCUS_COMBO_SET(filterCombo, FITSViewer::filterTypes.value(fe - 1, "--"));
        QTRY_COMPARE_WITH_TIMEOUT(Options::focusEffect(), (uint) fe, 1000);
    }
}

void TestEkosFocus::testStarDetection_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    std::list<char const *> Objects = { "Polaris", "Mizar", "M 51", "M 13", "M 47", "Vega", "NGC 2238", "M 81" };
    size_t count = 0;

    foreach (char const *name, Objects)
    {
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (10.0 < o.alt().Degrees())
            {
                QTest::addRow("%s", name)
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                count++;
            }
            else QWARN(QString("Fixture '%1' altitude is '%2' degrees, discarding.").arg(name).arg(so->alt().Degrees()).toStdString().c_str());
        }
    }

    if (!count)
        QSKIP("No usable fixture objects, bypassing test.");
#endif
}

void TestEkosFocus::testStarDetection()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);
    qDebug("Test focusing on '%s' RA '%s' DEC '%s'",
           NAME.toStdString().c_str(),
           RA.toStdString().c_str(),
           DEC.toStdString().c_str());

    // Just sync to RA/DEC to make the mount teleport to the object
    QWARN("During this test, the mount is not tracking - we leave it as is for the feature in the CCD simulator to trigger a failure.");
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    QVERIFY(ekos->mountModule()->sync(RA, DEC));

    // Wait for Focus to come up, switch to Focus tab
    KTRY_FOCUS_SHOW();

    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);

    KTRY_FOCUS_GADGET(QLineEdit, starsOut);

    // Locate somewhere we do see stars with the CCD Simulator
    KTRY_FOCUS_MOVETO(35000);

    // Run the focus procedure for SEP
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Centroid
    KTRY_FOCUS_CONFIGURE("Centroid", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Threshold - disable full-field
    KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Gradient - disable full-field
    KTRY_FOCUS_CONFIGURE("Gradient", "Iterative", 0.0, 0.0, 3.0);
    KTRY_FOCUS_DETECT(1, 3, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Longer exposure
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0, 3.0);
    KTRY_FOCUS_DETECT(8, 1, 99);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure again to cover more code
    // Filtering annulus is independent of the detection method
    // Run the HFR average over three frames with SEP to avoid
    for (double inner = 0.0; inner < 100.0; inner += 43.0)
    {
        for (double outer = 100.0; inner < outer; outer -= 42.0)
        {
            KTRY_FOCUS_CONFIGURE("SEP", "Iterative", inner, outer, 3.0);
            KTRY_FOCUS_DETECT(0.1, 2, 99);
        }
    }

    // Test threshold - disable full-field
    for (double threshold = 80.0; threshold < 99.0; threshold += 13.3)
    {
        KTRY_FOCUS_GADGET(QDoubleSpinBox, thresholdSpin);
        thresholdSpin->setValue(threshold);
        KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0, 0.0, 3.0);
        KTRY_FOCUS_DETECT(0.1, 1, 99);
    }
#endif
}

QTEST_KSTARS_MAIN(TestEkosFocus)

#endif // HAVE_INDI
