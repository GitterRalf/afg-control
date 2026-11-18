#pragma once
#include "gpib.h"
#include <QObject>
#include <QStringList>
#include <array>

enum class Waveform { Sine, Triangle, Square, Trapezoid, Pulse };
enum class RunMode { Continuous, Burst, Gate, HalfCycle };
enum class Modulation { Off, FM, VCO, FSK };

struct SignalSettings
{
    Waveform waveform = Waveform::Sine;
    double frequency = 1000.0;
    double level = 0.01;
    double offset = 0.0;
    int symmetry = 50;
    double period = 0.01;
    double width = 0.0005;
    double rise = 0.0001;
    double fall = 0.0001;
    double delay = 0.001;
    int pulses = 2;
    bool inverted = false;
};
struct TimingSettings
{
    RunMode mode = RunMode::Continuous;
    bool external = false;
    bool falling = false;
    bool threshold1V = false;
    double phase = 0.0;
    int cycles = 1;
    double interval = 0.01;
    double time = 0.005;
    double pause = 0.005;
};
struct ModulationSettings
{
    Modulation mode = Modulation::Off;
    bool am = false;
    bool pm = false;
    double frequency1 = 1000.0;
    double frequency2 = 2000.0;
    double time1 = 0.005;
    double time2 = 0.005;
};
struct SweepSettings
{
    bool logarithmic = false;
    int direction = 0; // Up, down, alternate.
    int trigger = 0; // Auto, manual, external.
    double frequency1 = 1000.0;
    double frequency2 = 2000.0;
    double time = 0.1;
    double pause = 0.0;
    std::array<bool, 4> markerEnabled{};
    std::array<double, 4> markerFrequency{{1500.0, 1500.0, 1500.0, 1500.0}};
};
QStringList settingCommands(const SignalSettings &, const TimingSettings &, const ModulationSettings &);
QStringList sweepCommands(const SignalSettings &, const TimingSettings &, const ModulationSettings &, const SweepSettings &);

struct InstrumentResult
{
    bool connected = false;
    QString error;
};
Q_DECLARE_METATYPE(InstrumentResult)

class Afg final : public QObject
{
    Q_OBJECT
public:
    explicit Afg(QObject *parent = nullptr);
    void connectDevice(GpibTarget target);
    void disconnectDevice();
    void execute(const QStringList &commands);
    void readStatus();

signals:
    void finished(const InstrumentResult &result);

private:
    void sendAndWait(const QByteArray &command);
    void release();
    std::unique_ptr<GpibSession> device;
};
