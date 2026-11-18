#pragma once

#include "afg.h"
#include <QMainWindow>
#include <QSettings>
#include <QThread>
#include <QTranslator>
#include <functional>
#include <optional>
#include <vector>

class QAction;
class QBoxLayout;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QHBoxLayout;
class ScientificSpinBox;
class QLabel;
class QMenu;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    template<typename Widget, typename Setter>
    Widget *localized(Widget *widget, const char *source, Setter setter)
    {
        auto update = [widget, source, setter] { (widget->*setter)(tr(source)); };
        textUpdates.push_back(update);
        update();
        return widget;
    }
    QLabel *caption(const char *source);
    QPushButton *button(const char *source, const char *name);
    QCheckBox *checkBox(const char *source, const char *name);
    QComboBox *comboBox(const char *name, std::initializer_list<const char *> sources);
    ScientificSpinBox *numberInput(const char *name, double lower, double upper, double initial, const char *unit, int decimals = 12);
    void row(QFormLayout *form, const char *source, QWidget *input);
    enum class Unit { Frequency, Voltage };
    QHBoxLayout *unitField(ScientificSpinBox *input, Unit type, std::function<QString()> fieldName);
    void unitRow(QFormLayout *form, const char *source, ScientificSpinBox *input, Unit type);
    QFormLayout *panel(QBoxLayout *page, const char *source);
    QWidget *page(const char *source);
    void createMenus();
    void createSignalPage();
    void createTimingPage();
    void createSweepPage();
    void createModulationPage();
    void createDevicePage();
    void editConnection();
    void setDarkTheme(bool dark);
    void installLanguage(const QString &code);
    void changeLanguage(const QString &code);
    void showAbout();
    void checkForUpdates();
    void checkPrivateRelease();
    void finishUpdateCheck(const QByteArray &data, const QString &error = {});
    void loadSettings();
    void saveSettings();
    void updateControls();
    void setStatus(std::function<QString()> text = {}, bool error = false);
    void start(const QStringList &commands, const char *success, std::optional<bool> nextOutput = {});
    void applySettings(bool sweep = false);
    void toggleConnection();
    void finish(const InstrumentResult &result);
    SignalSettings signalSettings() const;
    TimingSettings timingSettings() const;
    ModulationSettings modulationSettings() const;
    SweepSettings sweepSettings() const;
    void interpretInputs();

    QSettings preferences;
    QTranslator appTranslator;
    QTranslator qtTranslator;
    QString language;
    bool darkTheme = true;
    GpibTarget target;
    bool connected = false;
    bool busy = false;
    bool closing = false;
    std::optional<bool> outputState;
    std::optional<bool> pendingOutput;
    const char *successMessage = nullptr;
    std::function<QString()> statusText;
    std::vector<std::function<void()>> textUpdates;
    QList<QPushButton *> deviceButtons;
    QThread workerThread;
    Afg *instrument;

    QTabWidget *tabs;
    QAction *connectionAction;
    QAction *updateAction;
    QMenu *languageMenu = nullptr;
    QLabel *connectionLabel;
    QLabel *outputLabel;
    QLabel *statusLabel;
    QPushButton *connectButton;
    QPushButton *applyButton;
    QPushButton *triggerButton;
    QPushButton *sweepStartButton;

    QComboBox *waveform;
    ScientificSpinBox *frequency;
    ScientificSpinBox *level;
    ScientificSpinBox *offset;
    QDoubleSpinBox *symmetry;
    QCheckBox *inverted;
    QWidget *pulsePanel;
    QDoubleSpinBox *period;
    QDoubleSpinBox *width;
    QDoubleSpinBox *rise;
    QDoubleSpinBox *fall;
    QDoubleSpinBox *delay;
    QDoubleSpinBox *pulseCount;

    QComboBox *runMode;
    QComboBox *triggerSource;
    QComboBox *triggerSlope;
    QComboBox *triggerThreshold;
    QDoubleSpinBox *phase;
    QDoubleSpinBox *cycles;
    QDoubleSpinBox *interval;
    QDoubleSpinBox *gateTime;
    QDoubleSpinBox *gatePause;

    QComboBox *sweepScale;
    QComboBox *sweepDirection;
    QComboBox *sweepTrigger;
    ScientificSpinBox *sweepFrequency1;
    ScientificSpinBox *sweepFrequency2;
    QDoubleSpinBox *sweepTime;
    QDoubleSpinBox *sweepPause;
    std::array<QCheckBox *, 4> markerEnabled;
    std::array<ScientificSpinBox *, 4> markerFrequency;

    QComboBox *modulation;
    QCheckBox *am;
    QCheckBox *pm;
    QLabel *modulationHelp;
    ScientificSpinBox *fskFrequency1;
    ScientificSpinBox *fskFrequency2;
    QDoubleSpinBox *modulationTime1;
    QDoubleSpinBox *modulationTime2;
    QDoubleSpinBox *memorySlot;
    QComboBox *offResistance;
    QComboBox *errorFormat;
    QComboBox *maintenance;
};
