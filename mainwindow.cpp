#include "mainwindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFile>
#include <cmath>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMovie>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>

class ScientificSpinBox final : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;

    void setDisplayScale(double scale)
    {
        if (lineEdit()->isModified()) {
            interpretText();
        }
        displayScale = scale;
        const QSignalBlocker blockEditor(lineEdit());
        lineEdit()->setText(textFromValue(value()));
    }

protected:
    QString textFromValue(double value) const override
    {
        return locale().toString(value * displayScale, 'g', 10);
    }
    double valueFromText(const QString &text) const override
    {
        QString number = text;
        if (!suffix().isEmpty() && number.endsWith(suffix())) {
            number.chop(suffix().size());
        }
        return locale().toDouble(number.trimmed()) / displayScale;
    }
    QValidator::State validate(QString &text, int &position) const override
    {
        QString number = text;
        if (!suffix().isEmpty() && number.endsWith(suffix())) {
            number.chop(suffix().size());
        }
        QDoubleValidator validator(minimum() * displayScale, maximum() * displayScale, 12);
        validator.setLocale(locale());
        validator.setNotation(QDoubleValidator::ScientificNotation);
        int cursor = std::min(position, int(number.size()));
        return validator.validate(number, cursor);
    }

private:
    // Spin-box values stay in Hz or V for validation, GPIB and saved settings.
    double displayScale = 1.0;
};

namespace {

QIcon globeIcon(bool dark)
{
    QPixmap icon(24, 24);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(dark ? "#d4e3f5" : "#27425a"), 1.35));
    painter.drawEllipse(QRectF(3, 3, 18, 18));
    painter.drawEllipse(QRectF(8, 3, 8, 18));
    painter.drawLine(QPointF(3, 12), QPointF(21, 12));
    painter.drawLine(QPointF(5, 7.5), QPointF(19, 7.5));
    painter.drawLine(QPointF(5, 16.5), QPointF(19, 16.5));
    return QIcon(icon);
}


void refreshStyle(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), instrument(new Afg)
{
    setWindowTitle(QStringLiteral("AFG Control"));
    resize(1040, 780);
    target.board = std::clamp(preferences.value("gpib/board", 0).toInt(), 0, 99);
    target.address = std::clamp(preferences.value("gpib/address", 12).toInt(), 0, 30);
    installLanguage(preferences.value("language", "de").toString());
    setDarkTheme(preferences.value("appearance/dark", true).toBool());
    createMenus();

    auto *body = new QWidget;
    setCentralWidget(body);
    auto *outer = new QVBoxLayout(body);
    outer->setContentsMargins(24, 20, 24, 16);
    outer->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    auto *title = new QLabel(QStringLiteral("AFG Control"));
    title->setObjectName("title");
    titles->addWidget(title);
    auto *model = new QLabel(QStringLiteral("Rohde & Schwarz AFG · 377.2100.02"));
    model->setObjectName("muted");
    titles->addWidget(model);
    header->addLayout(titles);
    header->addStretch();
    connectionLabel = new QLabel;
    connectionLabel->setObjectName("connectionState");
    header->addWidget(connectionLabel, 0, Qt::AlignVCenter);
    connectButton = button(QT_TR_NOOP("Verbinden"), "connectDevice");
    connectButton->setProperty("primary", true);
    connect(connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    header->addWidget(connectButton, 0, Qt::AlignVCenter);
    outer->addLayout(header);

    tabs = new QTabWidget;
    tabs->setObjectName("deviceTabs");
    createSignalPage();
    createTimingPage();
    createSweepPage();
    createModulationPage();
    createDevicePage();
    outer->addWidget(tabs, 1);

    auto *actions = new QHBoxLayout;
    outputLabel = new QLabel;
    outputLabel->setObjectName("outputState");
    actions->addWidget(outputLabel, 1);
    auto *off = button(QT_TR_NOOP("Ausgang aus"), "outputOff");
    off->setProperty("stopping", true);
    deviceButtons.append(off);
    connect(off, &QPushButton::clicked, this, [this] {
        start({"OUTPUT(OFF)"}, QT_TR_NOOP("Ausgang abgeschaltet."), false);
    });
    actions->addWidget(off);
    auto *on = button(QT_TR_NOOP("Ausgang ein"), "outputOn");
    deviceButtons.append(on);
    connect(on, &QPushButton::clicked, this, [this] {
        start({"OUTPUT(ON)"}, QT_TR_NOOP("Ausgang eingeschaltet."), true);
    });
    actions->addWidget(on);
    applyButton = button(QT_TR_NOOP("Einstellungen übertragen"), "applySettings");
    applyButton->setProperty("primary", true);
    localized(applyButton, QT_TR_NOOP("Überträgt Signal, Betrieb, Trigger und Modulation und beendet einen laufenden Sweep."), &QPushButton::setToolTip);
    deviceButtons.append(applyButton);
    connect(applyButton, &QPushButton::clicked, this, [this] { applySettings(); });
    actions->addWidget(applyButton);
    outer->addLayout(actions);
    statusLabel = new QLabel;
    statusLabel->setObjectName("statusMessage");
    statusLabel->setWordWrap(true);
    statusLabel->setTextFormat(Qt::PlainText);
    statusLabel->setMinimumHeight(34);
    outer->addWidget(statusLabel);

    loadSettings();
    for (auto *input : tabs->findChildren<QComboBox *>()) {
        connect(input, &QComboBox::currentIndexChanged, this, &MainWindow::updateControls);
    }
    for (auto *input : tabs->findChildren<QCheckBox *>()) {
        connect(input, &QCheckBox::toggled, this, &MainWindow::updateControls);
    }
    instrument->moveToThread(&workerThread);
    connect(&workerThread, &QThread::finished, instrument, &QObject::deleteLater);
    connect(instrument, &Afg::finished, this, &MainWindow::finish);
    workerThread.start();
    updateControls();
}

MainWindow::~MainWindow()
{
    if (workerThread.isRunning()) {
        QMetaObject::invokeMethod(instrument, &Afg::disconnectDevice, Qt::BlockingQueuedConnection);
        workerThread.quit();
        workerThread.wait();
    }
    qApp->removeTranslator(&appTranslator);
    qApp->removeTranslator(&qtTranslator);
}

QLabel *MainWindow::caption(const char *source)
{
    return localized(new QLabel, source, &QLabel::setText);
}

QPushButton *MainWindow::button(const char *source, const char *name)
{
    auto *widget = localized(new QPushButton, source, &QPushButton::setText);
    widget->setObjectName(QString::fromLatin1(name));
    return widget;
}

QCheckBox *MainWindow::checkBox(const char *source, const char *name)
{
    auto *widget = localized(new QCheckBox, source, &QCheckBox::setText);
    widget->setObjectName(QString::fromLatin1(name));
    return widget;
}

QComboBox *MainWindow::comboBox(const char *name, std::initializer_list<const char *> sources)
{
    auto *widget = new QComboBox;
    widget->setObjectName(QString::fromLatin1(name));
    int index = 0;
    for (auto *source : sources) {
        widget->addItem(tr(source));
        const int item = index++;
        textUpdates.push_back([widget, item, source] { widget->setItemText(item, tr(source)); });
    }
    return widget;
}

ScientificSpinBox *MainWindow::numberInput(const char *name, double lower, double upper, double initial, const char *unit, int decimals)
{
    auto *input = new ScientificSpinBox;
    input->setObjectName(QString::fromLatin1(name));
    input->setDecimals(decimals);
    input->setRange(lower, upper);
    input->setSingleStep(decimals == 0 ? 1 : 0.001);
    input->setValue(initial);
    input->setSuffix(QString::fromUtf8(unit));
    input->setKeyboardTracking(false);
    input->setButtonSymbols(QAbstractSpinBox::NoButtons);
    input->setMinimumWidth(155);
    return input;
}

void MainWindow::row(QFormLayout *form, const char *source, QWidget *input)
{
    auto *label = caption(source);
    label->setBuddy(input);
    form->addRow(label, input);
}

QHBoxLayout *MainWindow::unitField(ScientificSpinBox *input, Unit type, std::function<QString()> fieldName)
{
    const bool frequencyUnits = type == Unit::Frequency;
    auto *unit = new QComboBox;
    unit->setObjectName(input->objectName() + QStringLiteral("Unit"));
    unit->addItem(frequencyUnits ? QStringLiteral("Hz") : QStringLiteral("V"), 1.0);
    unit->addItem(frequencyUnits ? QStringLiteral("MHz") : QStringLiteral("mV"), frequencyUnits ? 1e-6 : 1e3);
    connect(unit, &QComboBox::currentIndexChanged, input, [input, unit] {
        input->setDisplayScale(unit->currentData().toDouble());
    });
    auto updateAccessibleName = [unit, fieldName = std::move(fieldName)] {
        unit->setAccessibleName(tr("Einheit für %1").arg(fieldName()));
    };
    textUpdates.push_back(updateAccessibleName);
    updateAccessibleName();

    auto *field = new QHBoxLayout;
    field->setSpacing(6);
    field->addWidget(input, 1);
    field->addWidget(unit);
    return field;
}

void MainWindow::unitRow(QFormLayout *form, const char *source, ScientificSpinBox *input, Unit type)
{
    auto *label = caption(source);
    label->setBuddy(input);
    form->addRow(label, unitField(input, type, [label] { return label->text(); }));
}

QFormLayout *MainWindow::panel(QBoxLayout *pageLayout, const char *source)
{
    auto *frame = new QFrame;
    frame->setObjectName("panel");
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 14, 18, 16);
    auto *title = caption(source);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(9);
    layout->addLayout(form);
    pageLayout->addWidget(frame, 0, Qt::AlignTop);
    return form;
}

QWidget *MainWindow::page(const char *source)
{
    auto *content = new QWidget;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    const int index = tabs->addTab(scroll, tr(source));
    textUpdates.push_back([this, index, source] { tabs->setTabText(index, tr(source)); });
    return content;
}

void MainWindow::createSignalPage()
{
    auto *layout = new QHBoxLayout(page(QT_TR_NOOP("Signal")));
    layout->setContentsMargins(0, 12, 0, 0);
    auto *form = panel(layout, QT_TR_NOOP("Signalparameter"));
    waveform = comboBox("waveform", {QT_TR_NOOP("Sinus"), QT_TR_NOOP("Dreieck"), QT_TR_NOOP("Rechteck"), QT_TR_NOOP("Trapez"), QT_TR_NOOP("Puls")});
    frequency = numberInput("frequency", 0.01, 20e6, 1000, "");
    level = numberInput("level", 0, 10, 0.01, "");
    offset = numberInput("offset", -5, 5, 0, "", 2);
    symmetry = numberInput("symmetry", 5, 95, 50, " %", 0);
    row(form, QT_TR_NOOP("Signalform"), waveform);
    unitRow(form, QT_TR_NOOP("Frequenz"), frequency, Unit::Frequency);
    unitRow(form, QT_TR_NOOP("Pegel, Spitze–Spitze an 50 Ω"), level, Unit::Voltage);
    unitRow(form, QT_TR_NOOP("DC-Offset an 50 Ω"), offset, Unit::Voltage);
    row(form, QT_TR_NOOP("Symmetrie"), symmetry);
    inverted = checkBox(QT_TR_NOOP("Ausgangssignal invertieren"), "inverted");
    form->addRow(inverted);

    auto *pulseForm = panel(layout, QT_TR_NOOP("Trapez- und Pulszeiten"));
    pulsePanel = pulseForm->parentWidget();
    period = numberInput("period", 1e-6, 1500, 0.01, " s");
    width = numberInput("width", 25e-9, 1499, 0.0005, " s");
    rise = numberInput("rise", 25e-9, 50, 0.0001, " s");
    fall = numberInput("fall", 25e-9, 50, 0.0001, " s");
    delay = numberInput("delay", 50e-9, 100, 0.001, " s");
    pulseCount = numberInput("pulseCount", 1, 4095, 2, "", 0);
    row(pulseForm, QT_TR_NOOP("Periodendauer"), period);
    row(pulseForm, QT_TR_NOOP("Breite"), width);
    row(pulseForm, QT_TR_NOOP("Anstiegszeit"), rise);
    row(pulseForm, QT_TR_NOOP("Abfallzeit"), fall);
    row(pulseForm, QT_TR_NOOP("Pulszahl"), pulseCount);
    row(pulseForm, QT_TR_NOOP("Abstand der Pulsflanken"), delay);
    connect(pulseCount, &QDoubleSpinBox::valueChanged, this, &MainWindow::updateControls);
}

void MainWindow::createTimingPage()
{
    auto *layout = new QHBoxLayout(page(QT_TR_NOOP("Betrieb & Trigger")));
    auto *form = panel(layout, QT_TR_NOOP("Betriebsart"));
    runMode = comboBox("runMode", {QT_TR_NOOP("Kontinuierlich"), QT_TR_NOOP("Burst"), QT_TR_NOOP("Gate"), QT_TR_NOOP("Halbzyklus")});
    phase = numberInput("phase", -90, 90, 0, " °", 1);
    cycles = numberInput("cycles", 1, 4095, 1, "", 0);
    interval = numberInput("interval", 1e-6, 1500, 0.01, " s");
    gateTime = numberInput("gateTime", 1e-6, 1500, 0.005, " s");
    gatePause = numberInput("gatePause", 1e-6, 1500, 0.005, " s");
    row(form, QT_TR_NOOP("Betriebsart"), runMode);
    row(form, QT_TR_NOOP("Start-/Stoppphase"), phase);
    row(form, QT_TR_NOOP("Perioden pro Burst"), cycles);
    row(form, QT_TR_NOOP("Triggerintervall"), interval);
    row(form, QT_TR_NOOP("Torzeit"), gateTime);
    row(form, QT_TR_NOOP("Pause"), gatePause);

    form = panel(layout, QT_TR_NOOP("Trigger"));
    triggerSource = comboBox("triggerSource", {QT_TR_NOOP("Intern"), QT_TR_NOOP("Extern / manuell")});
    triggerSlope = comboBox("triggerSlope", {QT_TR_NOOP("Steigende Flanke"), QT_TR_NOOP("Fallende Flanke")});
    triggerThreshold = comboBox("triggerThreshold", {QT_TR_NOOP("0 V"), QT_TR_NOOP("1 V")});
    row(form, QT_TR_NOOP("Quelle"), triggerSource);
    row(form, QT_TR_NOOP("Flanke"), triggerSlope);
    row(form, QT_TR_NOOP("Schwelle"), triggerThreshold);
    triggerButton = button(QT_TR_NOOP("Manuell triggern"), "manualTrigger");
    deviceButtons.append(triggerButton);
    form->addRow(triggerButton);
    connect(triggerButton, &QPushButton::clicked, this, [this] {
        start({"TRIGGER(MANUAL)"}, QT_TR_NOOP("Manueller Trigger gesendet."));
    });
}

void MainWindow::createSweepPage()
{
    auto *layout = new QVBoxLayout(page(QT_TR_NOOP("Sweep")));
    auto *columns = new QHBoxLayout;
    layout->addLayout(columns);
    auto *form = panel(columns, QT_TR_NOOP("Frequenz-Sweep"));
    sweepFrequency1 = numberInput("sweepFrequency1", 0.01, 20e6, 1000, "");
    sweepFrequency2 = numberInput("sweepFrequency2", 0.01, 20e6, 2000, "");
    sweepTime = numberInput("sweepTime", 0.012, 1500, 0.1, " s");
    sweepPause = numberInput("sweepPause", 0, 1500, 0, " s");
    sweepScale = comboBox("sweepScale", {QT_TR_NOOP("Linear"), QT_TR_NOOP("Logarithmisch")});
    sweepDirection = comboBox("sweepDirection", {QT_TR_NOOP("Aufwärts"), QT_TR_NOOP("Abwärts"), QT_TR_NOOP("Auf / ab")});
    sweepTrigger = comboBox("sweepTrigger", {QT_TR_NOOP("Automatisch wiederholen"), QT_TR_NOOP("Einmalig"), QT_TR_NOOP("Extern getriggert")});
    unitRow(form, QT_TR_NOOP("Frequenz 1"), sweepFrequency1, Unit::Frequency);
    unitRow(form, QT_TR_NOOP("Frequenz 2"), sweepFrequency2, Unit::Frequency);
    row(form, QT_TR_NOOP("Sweep-Zeit"), sweepTime);
    row(form, QT_TR_NOOP("Pause"), sweepPause);
    row(form, QT_TR_NOOP("Verlauf"), sweepScale);
    row(form, QT_TR_NOOP("Richtung"), sweepDirection);
    row(form, QT_TR_NOOP("Ablauf"), sweepTrigger);
    form = panel(columns, QT_TR_NOOP("Frequenzmarken"));
    for (int i = 0; i < 4; ++i) {
        auto *enabled = new QCheckBox;
        enabled->setObjectName(QString("markerEnabled%1").arg(i + 1));
        textUpdates.push_back([enabled, i] { enabled->setText(tr("Marke %1").arg(i + 1)); });
        enabled->setText(tr("Marke %1").arg(i + 1));
        markerEnabled[i] = enabled;
        const auto name = QString("markerFrequency%1").arg(i + 1).toLatin1();
        markerFrequency[i] = numberInput(name.constData(), 0.01, 20e6, 1500, "");
        form->addRow(enabled, unitField(markerFrequency[i], Unit::Frequency, [enabled] {
            return enabled->text();
        }));
    }
    auto *actions = new QHBoxLayout;
    sweepStartButton = button(QT_TR_NOOP("Sweep starten"), "startSweep");
    sweepStartButton->setProperty("primary", true);
    deviceButtons.append(sweepStartButton);
    connect(sweepStartButton, &QPushButton::clicked, this, [this] { applySettings(true); });
    actions->addWidget(sweepStartButton);
    for (const auto &entry : {std::pair{"BREAK", QT_TR_NOOP("Anhalten")}, std::pair{"RESET", QT_TR_NOOP("Zur Startfrequenz")}, std::pair{"OFF", QT_TR_NOOP("Sweep aus")}}) {
        auto *action = button(entry.second, entry.first);
        deviceButtons.append(action);
        actions->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, command = QString::fromLatin1(entry.first)] {
            start({"SWEEP(" + command + ')'}, QT_TR_NOOP("Sweep-Befehl übertragen."));
        });
    }
    layout->addLayout(actions);
    layout->addStretch();
}

void MainWindow::createModulationPage()
{
    auto *layout = new QHBoxLayout(page(QT_TR_NOOP("Modulation")));
    auto *form = panel(layout, QT_TR_NOOP("Modulation"));
    modulation = comboBox("modulation", {QT_TR_NOOP("Aus"), QT_TR_NOOP("FM"), QT_TR_NOOP("VCO"), QT_TR_NOOP("FSK")});
    am = checkBox(QT_TR_NOOP("AM, externer Eingang"), "am");
    pm = checkBox(QT_TR_NOOP("Pulsmodulation (PM)"), "pm");
    row(form, QT_TR_NOOP("Frequenzmodulation"), modulation);
    form->addRow(am);
    form->addRow(pm);
    modulationHelp = caption(QT_TR_NOOP("AM benötigt ein Signal an der Buchse AM. FM und VCO verwenden den Eingang FM/VCO."));
    modulationHelp->setWordWrap(true);
    modulationHelp->setObjectName("muted");
    form->addRow(modulationHelp);
    form = panel(layout, QT_TR_NOOP("FSK und Pulsmodulation"));
    fskFrequency1 = numberInput("fskFrequency1", 0.01, 20e6, 1000, "");
    fskFrequency2 = numberInput("fskFrequency2", 0.01, 20e6, 2000, "");
    modulationTime1 = numberInput("modulationTime1", 500e-6, 1500, 0.005, " s");
    modulationTime2 = numberInput("modulationTime2", 500e-6, 1500, 0.005, " s");
    unitRow(form, QT_TR_NOOP("FSK-Frequenz 1"), fskFrequency1, Unit::Frequency);
    unitRow(form, QT_TR_NOOP("FSK-Frequenz 2"), fskFrequency2, Unit::Frequency);
    row(form, QT_TR_NOOP("Zeit 1 / PM ein"), modulationTime1);
    row(form, QT_TR_NOOP("Zeit 2 / PM aus"), modulationTime2);
}

void MainWindow::createDevicePage()
{
    auto *layout = new QVBoxLayout(page(QT_TR_NOOP("Gerät")));
    auto *form = panel(layout, QT_TR_NOOP("Gerätespeicher"));
    memorySlot = numberInput("memorySlot", 0, 15, 1, "", 0);
    row(form, QT_TR_NOOP("Speicherplatz"), memorySlot);
    auto *memoryActions = new QHBoxLayout;
    for (const auto &entry : {std::pair{"STORE", QT_TR_NOOP("Im Gerät speichern")}, std::pair{"RECALL", QT_TR_NOOP("Aus Gerät aufrufen")}}) {
        auto *action = button(entry.second, entry.first);
        deviceButtons.append(action);
        memoryActions->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, command = QString::fromLatin1(entry.first)] {
            memorySlot->interpretText();
            start({command + ' ' + QString::number(int(memorySlot->value()))},
                  QT_TR_NOOP("Speicherbefehl ausgeführt. Geräteeinstellungen am AFG prüfen."));
        });
    }
    form->addRow(memoryActions);
    auto *help = caption(QT_TR_NOOP("Nach einem Speicheraufruf gelten die Werte im Gerät. Die Eingabefelder zeigen die am PC vorbereiteten Werte."));
    help->setWordWrap(true);
    help->setObjectName("muted");
    form->addRow(help);

    form = panel(layout, QT_TR_NOOP("Ausgang und Fehlermeldungen"));
    offResistance = comboBox("offResistance", {QT_TR_NOOP("50 Ω, DC-Offset bleibt anliegen"), QT_TR_NOOP("Hochohmig, Ausgang getrennt")});
    errorFormat = comboBox("errorFormat", {QT_TR_NOOP("Ausführlich"), QT_TR_NOOP("Kurz")});
    row(form, QT_TR_NOOP("Verhalten bei Ausgang aus"), offResistance);
    row(form, QT_TR_NOOP("Fehlermeldungen am Gerät"), errorFormat);
    auto *apply = button(QT_TR_NOOP("Geräteoptionen übertragen"), "applyDeviceOptions");
    deviceButtons.append(apply);
    form->addRow(apply);
    connect(apply, &QPushButton::clicked, this, [this] {
        start({offResistance->currentIndex() == 0 ? "SPECIALFUNCTION(RIOFF 50)" : "SPECIALFUNCTION(RIOFF U)",
               errorFormat->currentIndex() == 0 ? "SPECIALFUNCTION(ERRORMESSAGE)[FULL]" : "SPECIALFUNCTION(ERRORMESSAGE)[SHORT]"},
              QT_TR_NOOP("Geräteoptionen übertragen."));
    });

    auto *read = button(QT_TR_NOOP("Gerätestatus lesen"), "readStatus");
    deviceButtons.append(read);
    form->addRow(read);
    connect(read, &QPushButton::clicked, this, [this] {
        start({}, QT_TR_NOOP("Der AFG meldet keinen Fehler."));
    });

    form = panel(layout, QT_TR_NOOP("Grundeinstellung und Wartung"));
    auto *preset = button(QT_TR_NOOP("PRESET am Gerät"), "presetDevice");
    deviceButtons.append(preset);
    form->addRow(preset);
    connect(preset, &QPushButton::clicked, this, [this] {
        start({"PRESET"}, QT_TR_NOOP("PRESET ausgeführt. Ausgang ist ein, 1 kHz Sinus mit 10 mVss."), true);
    });
    maintenance = comboBox("maintenance", {QT_TR_NOOP("Displaytest"), QT_TR_NOOP("RAM-Test"), QT_TR_NOOP("ROM-Test"),
        QT_TR_NOOP("Frequenz kalibrieren"), QT_TR_NOOP("Pegel kalibrieren"), QT_TR_NOOP("Offset kalibrieren"), QT_TR_NOOP("Alles kalibrieren")});
    row(form, QT_TR_NOOP("Wartungsfunktion"), maintenance);
    auto *run = button(QT_TR_NOOP("Ausführen"), "runMaintenance");
    deviceButtons.append(run);
    form->addRow(run);
    connect(run, &QPushButton::clicked, this, [this] {
        static const QStringList commands{
            "SPECIALFUNCTION(TEST)[DISPLAY]", "SPECIALFUNCTION(TEST)[RAM]", "SPECIALFUNCTION(TEST)[ROM]",
            "SPECIALFUNCTION(CALIBRATION)[FREQUENCY]", "SPECIALFUNCTION(CALIBRATION)[LEVEL]",
            "SPECIALFUNCTION(CALIBRATION)[OFFSET_DC]", "SPECIALFUNCTION(CALIBRATION) ALL"
        };
        start({commands.at(maintenance->currentIndex())}, QT_TR_NOOP("Wartungsfunktion abgeschlossen. Anzeige am Gerät prüfen."));
    });
    layout->addStretch();
}
void MainWindow::setDarkTheme(bool dark)
{
    darkTheme = dark;
    QPalette palette;
    const auto setColor = [&](QPalette::ColorRole role, const char *darkColor, const char *lightColor) {
        palette.setColor(role, QColor(dark ? darkColor : lightColor));
    };
    setColor(QPalette::Window, "#10191d", "#eef4f0");
    setColor(QPalette::WindowText, "#e9f1f1", "#172c28");
    setColor(QPalette::Base, "#111e23", "#ffffff");
    setColor(QPalette::AlternateBase, "#1b2b31", "#e7f0ea");
    setColor(QPalette::Text, "#e9f1f1", "#172c28");
    setColor(QPalette::Button, "#21343a", "#e1ece5");
    setColor(QPalette::ButtonText, "#e9f1f1", "#172c28");
    setColor(QPalette::Highlight, "#b8ea98", "#c0e7a8");
    setColor(QPalette::HighlightedText, "#142517", "#142517");
    setColor(QPalette::PlaceholderText, "#9cb0b4", "#4b625c");
    for (const auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText}) {
        palette.setColor(QPalette::Disabled, role, QColor(dark ? "#82969a" : "#586b65"));
    }
    qApp->setPalette(palette);

    QFile theme(QStringLiteral(":/theme.qss"));
    QString style;
    if (theme.open(QIODevice::ReadOnly)) {
        style = QString::fromUtf8(theme.readAll());
    }
    if (!dark) {
        QFile light(QStringLiteral(":/theme-light.qss"));
        if (light.open(QIODevice::ReadOnly)) {
            style += QString::fromUtf8(light.readAll());
        }
    }
    qApp->setStyleSheet(style);
    if (languageMenu) {
        languageMenu->setIcon(globeIcon(dark));
    }
}


void MainWindow::createMenus()
{
    auto *bar = menuBar();
    bar->setNativeMenuBar(false);
    auto *file = localized(bar->addMenu(QString()), QT_TR_NOOP("&Datei"), &QMenu::setTitle);
    localized(file->addAction(QString(), QKeySequence::Quit, this, &QWidget::close),
              QT_TR_NOOP("&Beenden"), &QAction::setText);
    auto *settings = localized(bar->addMenu(QString()), QT_TR_NOOP("&Einstellungen"), &QMenu::setTitle);
    settings->setObjectName("settingsMenu");
    connectionAction = localized(settings->addAction(QString(), this, &MainWindow::editConnection),
                                 QT_TR_NOOP("Verbindung …"), &QAction::setText);
    connectionAction->setObjectName("connectionSettings");
    auto *appearance = localized(settings->addMenu(QString()), QT_TR_NOOP("Darstellung"), &QMenu::setTitle);
    auto *themes = new QActionGroup(appearance);
    for (bool dark : {false, true}) {
        auto *action = localized(appearance->addAction(QString()), dark ? QT_TR_NOOP("Dunkel") : QT_TR_NOOP("Hell"), &QAction::setText);
        action->setObjectName(dark ? "darkTheme" : "lightTheme");
        action->setCheckable(true);
        action->setChecked(dark == darkTheme);
        themes->addAction(action);
        connect(action, &QAction::triggered, this, [this, dark] { setDarkTheme(dark); });
    }
    settings->addSeparator();
    localized(settings->addAction(QString(), QKeySequence::Save, this, &MainWindow::saveSettings),
              QT_TR_NOOP("Einstellungen speichern"), &QAction::setText)->setObjectName("saveSettings");

    auto *right = new QMenuBar(bar);
    right->setNativeMenuBar(false);
    languageMenu = right->addMenu(QString());
    languageMenu->setObjectName("languageMenu");
    languageMenu->setIcon(globeIcon(darkTheme));
    localized(languageMenu->menuAction(), QT_TR_NOOP("Sprache wählen"), &QAction::setToolTip);
    auto *languages = new QActionGroup(languageMenu);
    const std::array choices{
        std::pair{"de", "Deutsch"}, std::pair{"en", "English"}, std::pair{"fr", "Français"}
    };
    for (const auto &[code, name] : choices) {
        auto *action = languageMenu->addAction(QString::fromUtf8(name));
        action->setObjectName(QString("language-") + code);
        action->setData(QString::fromLatin1(code));
        action->setCheckable(true);
        action->setChecked(language == QLatin1String(code));
        languages->addAction(action);
        connect(action, &QAction::triggered, this, [this, code] { changeLanguage(QString::fromLatin1(code)); });
    }
    auto *about = localized(right->addMenu(QString()), QT_TR_NOOP("Über"), &QMenu::setTitle);
    about->setObjectName("aboutMenu");
    localized(about->addAction(QString(), this, &MainWindow::showAbout), QT_TR_NOOP("Über"), &QAction::setText)->setObjectName("aboutApplication");
    localized(about->addAction(QString(), this, [this] { QMessageBox::aboutQt(this, tr("Über Qt")); }),
              QT_TR_NOOP("Über Qt"), &QAction::setText)->setObjectName("aboutQt");
    bar->setCornerWidget(right, Qt::TopRightCorner);
}

void MainWindow::installLanguage(const QString &code)
{
    qApp->removeTranslator(&appTranslator);
    qApp->removeTranslator(&qtTranslator);
    language = QStringLiteral("de");
    if ((code == "en" || code == "fr") && appTranslator.load(":/i18n/afg_" + code)) {
        language = code;
    }
    QLocale numberFormat(language);
    numberFormat.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(numberFormat);
    setLocale(numberFormat);
    if (qtTranslator.load(":/qt-i18n/qtbase_" + language)) {
        qApp->installTranslator(&qtTranslator);
    }
    if (language != "de") {
        qApp->installTranslator(&appTranslator);
    }
}

void MainWindow::changeLanguage(const QString &code)
{
    if (code == language) return;
    interpretInputs();
    installLanguage(code);
    preferences.setValue("language", language);
    for (auto *action : languageMenu->actions()) {
        action->setChecked(action->data().toString() == language);
    }
    for (const auto &update : textUpdates) update();
    for (auto *input : tabs->findChildren<QDoubleSpinBox *>()) {
        input->setLocale(locale());
    }
    auto *bar = menuBar();
    auto *corner = bar->cornerWidget(Qt::TopRightCorner);
    bar->setCornerWidget(nullptr, Qt::TopRightCorner);
    bar->setCornerWidget(corner, Qt::TopRightCorner);
    statusLabel->setText(statusText ? statusText() : QString());
    updateControls();
}

void MainWindow::showAbout()
{
    QMessageBox dialog(this);
    dialog.setWindowTitle(tr("Über"));
    dialog.setTextFormat(Qt::RichText);
    dialog.setText(QStringLiteral("<b>AFG Control</b><br>%1")
                       .arg(tr("Version %1").arg(QApplication::applicationVersion())));
    dialog.setStandardButtons(QMessageBox::Ok);
    QMovie logo(QStringLiteral(":/branding/logo_g.gif"));
    if (logo.isValid()) {
        connect(&logo, &QMovie::frameChanged, &dialog, [&] { dialog.setIconPixmap(logo.currentPixmap()); });
        logo.start();
    } else {
        dialog.setIconPixmap(windowIcon().pixmap(120));
    }
    dialog.exec();
}

void MainWindow::editConnection()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("GPIB-Verbindung"));
    auto *form = new QFormLayout(&dialog);
    form->setContentsMargins(20, 18, 20, 18);
    auto *driver = new QLabel(QStringLiteral("NI-488.2"));
    form->addRow(tr("Adapter"), driver);
    auto *board = new QSpinBox;
    board->setRange(0, 99);
    board->setValue(target.board);
    form->addRow(tr("GPIB-Bus"), board);
    auto *address = new QSpinBox;
    address->setRange(0, 30);
    address->setValue(target.address);
    form->addRow(tr("Primäradresse"), address);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        target = {board->value(), address->value()};
        updateControls();
    }
}

void MainWindow::loadSettings()
{
    for (auto *input : tabs->findChildren<QDoubleSpinBox *>()) {
        const auto key = "input/" + input->objectName();
        bool valid = false;
        const auto value = preferences.value(key, input->value()).toDouble(&valid);
        if (valid && std::isfinite(value)) input->setValue(value);
    }
    for (auto *input : tabs->findChildren<QComboBox *>()) {
        const auto key = "input/" + input->objectName();
        const int value = preferences.value(key, input->currentIndex()).toInt();
        if (value >= 0 && value < input->count()) input->setCurrentIndex(value);
    }
    for (auto *input : tabs->findChildren<QCheckBox *>()) {
        input->setChecked(preferences.value("input/" + input->objectName(), input->isChecked()).toBool());
    }
}

void MainWindow::saveSettings()
{
    interpretInputs();
    for (auto *input : tabs->findChildren<QDoubleSpinBox *>()) {
        preferences.setValue("input/" + input->objectName(), input->value());
    }
    for (auto *input : tabs->findChildren<QComboBox *>()) {
        preferences.setValue("input/" + input->objectName(), input->currentIndex());
    }
    for (auto *input : tabs->findChildren<QCheckBox *>()) {
        preferences.setValue("input/" + input->objectName(), input->isChecked());
    }
    preferences.setValue("gpib/board", target.board);
    preferences.setValue("gpib/address", target.address);
    preferences.setValue("appearance/dark", darkTheme);
    preferences.setValue("language", language);
    preferences.sync();
    const bool saved = preferences.status() == QSettings::NoError;
    setStatus([saved] { return saved ? tr("Einstellungen gespeichert.") : tr("Die Einstellungen konnten nicht gespeichert werden."); }, !saved);
}

void MainWindow::interpretInputs()
{
    for (auto *input : tabs->findChildren<QDoubleSpinBox *>()) input->interpretText();
}

SignalSettings MainWindow::signalSettings() const
{
    SignalSettings result;
    result.waveform = static_cast<Waveform>(waveform->currentIndex());
    result.frequency = frequency->value();
    result.level = level->value();
    result.offset = offset->value();
    result.symmetry = int(symmetry->value());
    result.period = period->value();
    result.width = width->value();
    result.rise = rise->value();
    result.fall = fall->value();
    result.delay = delay->value();
    result.pulses = int(pulseCount->value());
    result.inverted = inverted->isChecked();
    return result;
}

TimingSettings MainWindow::timingSettings() const
{
    TimingSettings result;
    result.mode = static_cast<RunMode>(runMode->currentIndex());
    result.external = triggerSource->currentIndex() == 1;
    result.falling = triggerSlope->currentIndex() == 1;
    result.threshold1V = triggerThreshold->currentIndex() == 1;
    result.phase = phase->value();
    result.cycles = int(cycles->value());
    result.interval = interval->value();
    result.time = gateTime->value();
    result.pause = gatePause->value();
    return result;
}

ModulationSettings MainWindow::modulationSettings() const
{
    ModulationSettings result;
    result.mode = static_cast<Modulation>(modulation->currentIndex());
    result.am = am->isChecked();
    result.pm = pm->isChecked();
    result.frequency1 = fskFrequency1->value();
    result.frequency2 = fskFrequency2->value();
    result.time1 = modulationTime1->value();
    result.time2 = modulationTime2->value();
    return result;
}

SweepSettings MainWindow::sweepSettings() const
{
    SweepSettings result;
    result.frequency1 = sweepFrequency1->value();
    result.frequency2 = sweepFrequency2->value();
    result.time = sweepTime->value();
    result.pause = sweepPause->value();
    result.logarithmic = sweepScale->currentIndex() == 1;
    result.direction = sweepDirection->currentIndex();
    result.trigger = sweepTrigger->currentIndex();
    for (int i = 0; i < 4; ++i) {
        result.markerEnabled[i] = markerEnabled[i]->isChecked();
        result.markerFrequency[i] = markerFrequency[i]->value();
    }
    return result;
}

void MainWindow::setStatus(std::function<QString()> text, bool error)
{
    statusText = std::move(text);
    statusLabel->setText(statusText ? statusText() : QString());
    statusLabel->setProperty("error", error);
    refreshStyle(statusLabel);
}

void MainWindow::updateControls()
{
    const bool standard = waveform->currentIndex() <= 2;
    const bool trapezoid = waveform->currentIndex() == 3;
    const bool pulse = waveform->currentIndex() == 4;
    frequency->setEnabled(standard);
    symmetry->setEnabled(standard);
    pulsePanel->setVisible(!standard);
    period->setMinimum(trapezoid ? 2e-6 : 1e-6);
    width->setRange(trapezoid ? 1e-6 : 25e-9, trapezoid ? 1499 : 50);
    rise->setEnabled(trapezoid);
    fall->setEnabled(trapezoid);
    pulseCount->setEnabled(pulse);
    delay->setEnabled(pulse && pulseCount->value() > 1);
    const int mode = runMode->currentIndex();
    const bool external = triggerSource->currentIndex() == 1;
    phase->setEnabled(mode != 0);
    cycles->setEnabled(mode == 1);
    interval->setEnabled(!external && (mode == 1 || mode == 3));
    gateTime->setEnabled(!external && mode == 2);
    gatePause->setEnabled(!external && mode == 2);
    triggerSlope->setEnabled(external);
    triggerThreshold->setEnabled(external);
    const bool fsk = modulation->currentIndex() == 3;
    fskFrequency1->setEnabled(fsk);
    fskFrequency2->setEnabled(fsk);
    modulationTime1->setEnabled(!external && (fsk || pm->isChecked()));
    modulationTime2->setEnabled(!external && (fsk || pm->isChecked()));
    for (int i = 0; i < 4; ++i) markerFrequency[i]->setEnabled(markerEnabled[i]->isChecked());

    tabs->setEnabled(!busy);
    connectButton->setEnabled(!busy);
    connectButton->setText(connected ? tr("Trennen") : tr("Verbinden"));
    connectionAction->setEnabled(!busy && !connected);
    for (auto *action : deviceButtons) action->setEnabled(connected && !busy);
    triggerButton->setEnabled(connected && !busy && external);
    connectionLabel->setText(busy ? tr("Übertragung …") : connected ? tr("Verbunden") : tr("Getrennt"));
    connectionLabel->setProperty("connected", connected);
    refreshStyle(connectionLabel);
    connectionLabel->setToolTip(tr("GPIB %1 · Adresse %2").arg(target.board).arg(target.address));
    if (outputState.has_value() && connected) {
        outputLabel->setText(*outputState ? tr("Ausgang ein") : tr("Ausgang aus"));
    } else {
        outputLabel->setText(tr("Ausgangsstatus unbekannt"));
    }
}

void MainWindow::start(const QStringList &commands, const char *success, std::optional<bool> nextOutput)
{
    if (!connected || busy) return;
    busy = true;
    pendingOutput = nextOutput.has_value() ? nextOutput : outputState;
    if (!commands.isEmpty() && commands.first().startsWith("RECALL ")) {
        pendingOutput.reset();
    }
    successMessage = success;
    setStatus([] { return tr("Befehle werden übertragen …"); });
    updateControls();
    QMetaObject::invokeMethod(instrument, [worker = instrument, commands] {
        if (commands.isEmpty()) worker->readStatus();
        else worker->execute(commands);
    }, Qt::QueuedConnection);
}

void MainWindow::applySettings(bool sweep)
{
    if (!connected || busy) return;
    interpretInputs();
    try {
        const auto commands = sweep ? sweepCommands(signalSettings(), timingSettings(), modulationSettings(), sweepSettings())
                                    : settingCommands(signalSettings(), timingSettings(), modulationSettings());
        start(commands, sweep ? QT_TR_NOOP("Sweep-Einstellungen übertragen.") : QT_TR_NOOP("Signal-, Trigger- und Modulationseinstellungen übertragen."));
    } catch (const std::exception &error) {
        setStatus([message = QString::fromUtf8(error.what())] { return message; }, true);
    }
}

void MainWindow::toggleConnection()
{
    if (busy) return;
    busy = true;
    outputState.reset();
    pendingOutput.reset();
    successMessage = connected ? QT_TR_NOOP("GPIB-Verbindung getrennt.") : QT_TR_NOOP("Verbunden mit GPIB-Adresse %1.");
    setStatus([] { return tr("GPIB-Verbindung wird umgeschaltet …"); });
    updateControls();
    if (connected) {
        QMetaObject::invokeMethod(instrument, &Afg::disconnectDevice, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(instrument, [worker = instrument, target = target] { worker->connectDevice(target); }, Qt::QueuedConnection);
    }
}

void MainWindow::finish(const InstrumentResult &result)
{
    connected = result.connected;
    busy = false;
    if (closing) {
        if (connected) toggleConnection();
        else close();
        return;
    }
    if (result.error.isEmpty()) {
        outputState = pendingOutput;
        const auto success = successMessage;
        setStatus([success, address = target.address] {
            const auto text = tr(success);
            return text.contains("%1") ? text.arg(address) : text;
        });
    } else {
        outputState.reset();
        setStatus([error = result.error] { return error; }, true);
    }
    if (!connected) outputState.reset();
    updateControls();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (connected || busy) {
        event->ignore();
        closing = true;
        if (!busy) toggleConnection();
        return;
    }
    event->accept();
}
