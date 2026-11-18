#include "afg.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

QString tr(const char *source)
{
    return QCoreApplication::translate("Afg", source);
}
[[noreturn]] void fail(const QString &message)
{
    throw std::runtime_error(message.toUtf8().constData());
}
void require(bool valid, const char *message)
{
    if (!valid) fail(tr(message));
}
bool between(double value, double lower, double upper)
{
    return std::isfinite(value) && value >= lower && value <= upper;
}
QString number(double value)
{
    return QString::number(value, 'G', 7);
}
void checkStatus(unsigned char status)
{
    if (status & 0x80) {
        fail(QCoreApplication::translate("Afg", "Der AFG meldet Überspannung am Ausgang."));
    }
    if (status & 0x2f) {
        fail(QCoreApplication::translate("Afg", "Der AFG meldet einen Befehls-, Parameter- oder Gerätefehler (Status 0x%1).")
                 .arg(status, 2, 16, QLatin1Char('0')));
    }
}

void validate(const SignalSettings &signal, const TimingSettings &timing, const ModulationSettings &modulation)
{
    require(between(signal.level, 0, 10) && between(signal.offset, -5, 5),
            QT_TRANSLATE_NOOP("Afg", "Pegel muss zwischen 0 und 10 Vss und Offset zwischen −5 und +5 V liegen."));
    const bool standard = signal.waveform <= Waveform::Square;
    require(signal.symmetry >= 5 && signal.symmetry <= 95,
            QT_TRANSLATE_NOOP("Afg", "Symmetrie muss zwischen 5 und 95 % liegen."));
    if (standard) {
        const bool symmetric = signal.symmetry == 50;
        require(between(signal.frequency, symmetric ? 0.01 : 2.0, symmetric ? 20e6 : 2e6),
                QT_TRANSLATE_NOOP("Afg", "Frequenz muss bei 50 % Symmetrie zwischen 0,01 Hz und 20 MHz liegen, sonst zwischen 2 Hz und 2 MHz."));
    } else {
        require(timing.mode == RunMode::Continuous, QT_TRANSLATE_NOOP("Afg", "Trapez und Puls benötigen kontinuierlichen Betrieb."));
        require(modulation.mode == Modulation::Off && !modulation.pm,
                QT_TRANSLATE_NOOP("Afg", "Bei Trapez und Puls ist nur AM zulässig."));
        require(between(signal.period, signal.waveform == Waveform::Pulse ? 1e-6 : 2e-6, 1500),
                QT_TRANSLATE_NOOP("Afg", "Periodendauer liegt außerhalb des zulässigen Bereichs."));
        if (signal.waveform == Waveform::Trapezoid) {
            require(between(signal.width, 1e-6, 1499) && between(signal.rise, 25e-9, 50) && between(signal.fall, 25e-9, 50),
                    QT_TRANSLATE_NOOP("Afg", "Trapezbreite oder Flankenzeiten liegen außerhalb des zulässigen Bereichs."));
            const auto sum = signal.rise + signal.fall;
            require(signal.period > 1.0002 * signal.width && signal.width > sum
                    && signal.rise / signal.fall >= 1.0 / 19 && signal.rise / signal.fall <= 19
                    && ((sum > 500e-9 && sum < 0.5) || signal.rise == signal.fall)
                    && signal.period < 6500 * (signal.width - signal.fall),
                    QT_TRANSLATE_NOOP("Afg", "Trapezzeiten sind unverträglich. Bedingungen siehe Handbuch 2.3.5.2."));
        } else {
            require(between(signal.width, 25e-9, 50) && signal.pulses >= 1 && signal.pulses <= 4095,
                    QT_TRANSLATE_NOOP("Afg", "Pulsbreite oder Pulszahl liegen außerhalb des zulässigen Bereichs."));
            if (signal.pulses == 1) {
                require(signal.period > 2.01 * signal.width,
                        QT_TRANSLATE_NOOP("Afg", "Bei einem Puls muss die Periode größer als 2,01 × Pulsbreite sein."));
            } else {
                require(between(signal.delay, 50e-9, 100) && signal.period > signal.pulses * signal.delay
                        && signal.width / signal.delay >= 0.05 && signal.width / signal.delay <= 0.95
                        && ((signal.delay > 500e-9 && signal.delay < 0.5) || signal.width == signal.delay / 2),
                        QT_TRANSLATE_NOOP("Afg", "Pulsfolge ist unverträglich. Bedingungen siehe Handbuch 2.3.5.3."));
            }
        }
    }
    require(between(timing.phase, -90, 90) && timing.cycles >= 1 && timing.cycles <= 4095,
            QT_TRANSLATE_NOOP("Afg", "Phase muss zwischen −90 und +90° und Burstzahl zwischen 1 und 4095 liegen."));
    if (timing.mode != RunMode::Continuous && !timing.external) {
        require(between(timing.interval, timing.mode == RunMode::HalfCycle ? 500e-6 : 1e-6, 1500),
                QT_TRANSLATE_NOOP("Afg", "Triggerintervall liegt außerhalb des zulässigen Bereichs."));
        if (timing.mode == RunMode::Burst) {
            require(timing.interval > timing.cycles / signal.frequency,
                    QT_TRANSLATE_NOOP("Afg", "Burstintervall muss größer als Burstzahl / Frequenz sein."));
        } else if (timing.mode == RunMode::HalfCycle) {
            require(2 * timing.interval > 1 / signal.frequency,
                    QT_TRANSLATE_NOOP("Afg", "Zweifaches Intervall muss größer als die Periodendauer sein."));
        } else {
            require(between(timing.time, 1e-6, 1500) && between(timing.pause, 1e-6, 1500)
                    && timing.time / timing.pause > 1.0 / 6500 && timing.time / timing.pause < 6500,
                    QT_TRANSLATE_NOOP("Afg", "Torzeit und Pause sind unverträglich."));
        }
    }
    const bool keyed = modulation.mode == Modulation::FSK || modulation.pm;
    require(!keyed || timing.mode == RunMode::Continuous,
            QT_TRANSLATE_NOOP("Afg", "FSK und Pulsmodulation benötigen kontinuierlichen Betrieb."));
    require(!(modulation.mode == Modulation::FSK && modulation.pm),
            QT_TRANSLATE_NOOP("Afg", "FSK und Pulsmodulation können nicht kombiniert werden."));
    if (modulation.mode == Modulation::FSK) {
        const double lower = signal.symmetry == 50 ? 0.01 : 2;
        const double upper = signal.symmetry == 50 ? 20e6 : 2e6;
        require(between(modulation.frequency1, lower, upper) && between(modulation.frequency2, lower, upper)
                && modulation.frequency1 / modulation.frequency2 >= 0.1
                && modulation.frequency1 / modulation.frequency2 <= 10,
                QT_TRANSLATE_NOOP("Afg", "FSK-Frequenzen sind unverträglich. Ihr Verhältnis muss zwischen 0,1 und 10 liegen."));
    }
    if (keyed && !timing.external) {
        require(between(modulation.time1, 500e-6, 1500) && between(modulation.time2, 500e-6, 1500)
                && modulation.time1 / modulation.time2 > 1.0 / 6500
                && modulation.time1 / modulation.time2 < 6500,
                QT_TRANSLATE_NOOP("Afg", "Modulationszeiten sind unverträglich."));
    }
}
} // namespace

QStringList settingCommands(const SignalSettings &signal, const TimingSettings &timing, const ModulationSettings &modulation)
{
    validate(signal, timing, modulation);
    // Cancel mutually exclusive operations before choosing the new configuration.
    QStringList commands{"SWEEP(OFF)", "MODULATION(OFF)", "MODE(CONTINUOUS)"};
    const std::array<const char *, 5> names{{"SINE", "TRIANGLE", "SQUARE", "TRAPEZOID", "PULSE"}};
    QStringList parameters;
    if (signal.waveform <= Waveform::Square) {
        parameters << "FREQ " + number(signal.frequency) << "SYMM " + QString::number(signal.symmetry);
    } else {
        parameters << "PER " + number(signal.period) << "WID " + number(signal.width);
        if (signal.waveform == Waveform::Trapezoid) {
            parameters << "RISE " + number(signal.rise) << "FALL " + number(signal.fall);
        } else {
            parameters << "N " + QString::number(signal.pulses);
            if (signal.pulses > 1) parameters << "DEL " + number(signal.delay);
        }
    }
    // These dependent parameters must travel in the same command group.
    parameters << "LEV(PP) " + number(signal.level) << "OFFS " + number(signal.offset);
    commands << QString("WAVE(%1)[%2]").arg(QLatin1String(names.at(static_cast<int>(signal.waveform))), parameters.join(','));
    commands << (signal.inverted ? "OUTPUT(INVERTED)" : "OUTPUT(NONINVERTED)");
    commands << (timing.external
        ? QString("TRIGGER(EXTERNAL)[%1,LEVEL(%2V)]").arg(timing.falling ? "FALL" : "RISE", timing.threshold1V ? "1" : "0")
        : QString("TRIGGER(INTERNAL)"));
    switch (timing.mode) {
    case RunMode::Continuous:
        break;
    case RunMode::Burst:
        commands << QString("MODE(BURST)[PHASE %1,NUMBER %2%3]").arg(number(timing.phase)).arg(timing.cycles)
            .arg(timing.external ? QString() : ",INTERVAL " + number(timing.interval));
        break;
    case RunMode::Gate:
        commands << QString("MODE(GATE)[PHASE %1%2]").arg(number(timing.phase),
            timing.external ? QString() : ",TIME " + number(timing.time) + ",PAUSE " + number(timing.pause));
        break;
    case RunMode::HalfCycle:
        commands << QString("MODE(HALFCYCLE)[PHASE %1%2]").arg(number(timing.phase),
            timing.external ? QString() : ",INTERVAL " + number(timing.interval));
        break;
    }
    QStringList modes;
    if (modulation.am) modes << "AM";
    if (modulation.mode == Modulation::FM) modes << "FM";
    if (modulation.mode == Modulation::VCO) modes << "VCO";
    if (modulation.mode == Modulation::FSK) modes << "FSK";
    if (modulation.pm) modes << "PM";
    if (!modes.isEmpty()) {
        QString command = "MODULATION(" + modes.join(' ') + ')';
        if (modulation.mode == Modulation::FSK) {
            command += QString("[FREQUENCY(1) %1,FREQUENCY(2) %2").arg(number(modulation.frequency1), number(modulation.frequency2));
            if (!timing.external) command += QString(",TIME(1) %1,TIME(2) %2").arg(number(modulation.time1), number(modulation.time2));
            command += ']';
        } else if (modulation.pm && !timing.external) {
            command += QString("[TIME %1,PAUSE %2]").arg(number(modulation.time1), number(modulation.time2));
        }
        commands << command;
    }
    for (const auto &command : commands) {
        require(command.size() <= 130, QT_TRANSLATE_NOOP("Afg", "Die Parameter benötigen mehr als 132 Zeichen in einer Befehlszeile."));
    }
    return commands;
}

QStringList sweepCommands(const SignalSettings &signal, const TimingSettings &timing,
                          const ModulationSettings &modulation, const SweepSettings &sweep)
{
    require(signal.waveform <= Waveform::Square && timing.mode == RunMode::Continuous
            && modulation.mode == Modulation::Off && !modulation.am && !modulation.pm,
            QT_TRANSLATE_NOOP("Afg", "Sweep benötigt Sinus, Dreieck oder Rechteck, kontinuierlichen Betrieb und ausgeschaltete Modulation."));
    auto commands = settingCommands(signal, timing, modulation);
    const bool symmetric = signal.symmetry == 50;
    const double lower = symmetric ? 0.01 : 2;
    const double upper = symmetric ? 20e6 : 2e6;
    require(between(sweep.frequency1, lower, upper) && between(sweep.frequency2, lower, upper)
            && sweep.frequency1 != sweep.frequency2 && between(sweep.time, 0.012, 1500) && between(sweep.pause, 0, 1500),
            QT_TRANSLATE_NOOP("Afg", "Sweep-Frequenzen oder -Zeiten liegen außerhalb des zulässigen Bereichs."));
    const std::array<const char *, 3> directions{{"UP", "DOWN", "ALTERNATE"}};
    require(sweep.direction >= 0 && sweep.direction < 3 && sweep.trigger >= 0 && sweep.trigger < 3,
            QT_TRANSLATE_NOOP("Afg", "Ungültige Sweep-Auswahl."));
    commands << QString("SWEEP(FUNCTION)[%1,%2]").arg(sweep.logarithmic ? "LOGARITHM" : "LINEAR", directions.at(sweep.direction));
    commands << QString("SWEEP(PARAMETER)[FREQUENCY(1) %1,FREQUENCY(2) %2,TIME %3,PAUSE %4]")
        .arg(number(sweep.frequency1), number(sweep.frequency2), number(sweep.time), number(sweep.pause));
    for (int i = 0; i < 4; ++i) {
        if (sweep.markerEnabled[i]) {
            require(between(sweep.markerFrequency[i], std::min(sweep.frequency1, sweep.frequency2), std::max(sweep.frequency1, sweep.frequency2)),
                    QT_TRANSLATE_NOOP("Afg", "Frequenzmarken müssen innerhalb der Sweep-Grenzen liegen."));
            commands << QString("SWEEP(MARKER %1) %2").arg(i + 1).arg(number(sweep.markerFrequency[i]));
        } else {
            commands << QString("SWEEP(MARKER %1 OFF)").arg(i + 1);
        }
    }
    const std::array<const char *, 3> triggers{{"AUTO", "MANUAL", "EXTERNAL"}};
    commands << QString("SWEEP(%1)").arg(triggers.at(sweep.trigger));
    for (const auto &command : commands) {
        require(command.size() <= 130, QT_TRANSLATE_NOOP("Afg", "Die Parameter benötigen mehr als 132 Zeichen in einer Befehlszeile."));
    }
    return commands;
}

Afg::Afg(QObject *parent) : QObject(parent) {}

void Afg::sendAndWait(const QByteArray &command)
{
    checkStatus(device->status()); // Inspect and clear a previous request.
    device->write(command);
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 30000) {
        const auto status = device->status();
        checkStatus(status);
        if (status & 0x10) return;
        QThread::msleep(10);
    }
    fail(tr("Der AFG hat die Befehlsbearbeitung nicht innerhalb von 30 Sekunden bestätigt."));
}
void Afg::connectDevice(GpibTarget target)
{
    InstrumentResult result;
    try {
        if (device) release();
        device = openGpib(target);
        sendAndWait("TRANSFERMODE(2),SRQ(READY ERRORS OVERVOLTAGE)");
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
        try { release(); } catch (...) {}
    }
    result.connected = bool(device);
    emit finished(result);
}
void Afg::release()
{
    auto connection = std::move(device);
    if (connection) connection->local();
}
void Afg::disconnectDevice()
{
    InstrumentResult result;
    try {
        release();
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    emit finished(result);
}
void Afg::execute(const QStringList &commands)
{
    InstrumentResult result;
    try {
        if (!device) fail(tr("GPIB ist nicht verbunden."));
        for (const auto &command : commands) sendAndWait(command.toLatin1());
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    result.connected = bool(device);
    emit finished(result);
}
void Afg::readStatus()
{
    InstrumentResult result;
    try {
        if (!device) fail(tr("GPIB ist nicht verbunden."));
        checkStatus(device->status());
    } catch (const std::exception &error) {
        result.error = QString::fromUtf8(error.what());
    }
    result.connected = bool(device);
    emit finished(result);
}
