#include "GcsGUI.h" 
#include "Backendhandler.h"
#include "containers.h"
#include "constants.h"
#include <QtSerialPort/QSerialPortInfo>
#include <QQuickView>
#include <QWidget>
#include <QQmlContext>
#include "modernui.h"

QString HOVERED_STYLE = GcsGUI::getHoverStyle(THEME_COLOR);
QString HOVERED_STYLE_HOME = GcsGUI::getHoverStyle(HOME_BUTTON_COLOR);

QString HOVERED_STYLE_SETTINGS = QString(
    "QPushButton { background-color: %1; color: white; font - weight: bold; }"
    "QPushButton:hover { background-color: blue; }"
).arg(THEME_COLOR);

GcsGUI::GcsGUI(QWidget* parent) : QMainWindow(parent)
{    
    setWindowTitle(WINDOW_NAME);
    resize(RES_WIDTH, RES_HEIGHT);
    this->setWindowIcon(QIcon(ICONPATH));

    serial_port = new QSerialPort(this);
    graphic_timer = new QTimer(this);
    audio_handler = new AudioHandler(this);
    mqtt_data_timer = new QTimer(this);

    sharedQmlEngine = new QQmlEngine(this);
    sceneStack = new QStackedWidget(this);

    setCentralWidget(sceneStack);

    mainSceneWidget = new QWidget();
    controlSurfacesSceneWidget = new QWidget();
    EarthProjectionSceneWidget = new QWidget();

    batteryIndicator = new BatteryWidget();
    batteryIndicator->setObjectName("MyBatteryWidget");

    sceneStack->addWidget(mainSceneWidget);  // Index 0
    sceneStack->addWidget(controlSurfacesSceneWidget); // Index 1
    sceneStack->addWidget(EarthProjectionSceneWidget); // Index 2

    createMainScene();
    createControlSurfacesScene(); 
    createEarthProjectionScene();

    sceneStack->setCurrentIndex(0);

    setupSettingsFields();
    loadSettings();
    initLogFile();

    connect(graphic_timer, &QTimer::timeout, this, &GcsGUI::graphicUpdates);
    connect(mqtt_data_timer, &QTimer::timeout, this, &GcsGUI::updateMqttData);

    mqtt_data_timer->start(MQTT_DELAY);
    start_mqtt();
}

GcsGUI::~GcsGUI()
{
    close_program();
}

void GcsGUI::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) {
            showNormal();
        }
        else {
            showFullScreen();
        }
    }

    else if (event->key() == Qt::Key_Escape && isFullScreen()) {
        showNormal();
    }
    
    else {
        QMainWindow::keyPressEvent(event);
    }

}

void GcsGUI::close_program() {
    running = false;
    logToFile(false);
    handleSerial(false);
    disconnect_mqtt();
}

void GcsGUI::set_colors() {

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
    darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Highlight, QColor(66, 134, 244)); // THEME_COLOR equivalent
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);

    QApplication::setPalette(darkPalette);

}

void GcsGUI::closeEvent(QCloseEvent* event) {
    close_program();
    event->accept();
}

QWidget* GcsGUI::createCameraWidget(int cameraIndex, QWidget* parent) {
    QFrame* cameraBox = new QFrame(parent);
    
    QQuickView* miniView = new QQuickView(sharedQmlEngine, nullptr);

    miniView->rootContext()->setContextProperty("gcsBackend", this);
    miniView->rootContext()->setContextProperty("fixedCameraIndex", cameraIndex);

    miniView->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/MainView.qml"));
    miniView->setResizeMode(QQuickView::SizeRootObjectToView);

    QWidget* camWidget = QWidget::createWindowContainer(miniView, cameraBox);

    return cameraBox;
}

// SCENES 

void GcsGUI::createMainScene() {

    if (graphic_timer) graphic_timer->stop();

    sceneIconButton = nullptr;
    statusIconButton = nullptr;
    saveIconButton = nullptr;

    if (mainSceneWidget->layout()) {
        QLayoutItem* item;
        while ((item = mainSceneWidget->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete mainSceneWidget->layout();
    }

    mainSceneWidget->setObjectName("MainSceneWidget");

    QString backgroundStyle = QString(
        "QWidget#MainSceneWidget {"
        "background-color: #000000;"
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);
    mainSceneWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(mainSceneWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* topSection = new QWidget(mainSceneWidget);
    topSection->setStyleSheet("background: transparent;");

    QGridLayout* gridLayout = new QGridLayout(topSection);
    gridLayout->setContentsMargins(40, 30, 40, 20);
    gridLayout->setSpacing(24);

    const int TITLE_SIZE = 18;

    main_batt_currentGUI = new Containers("MainBatt Current", TITLE_SIZE, "white", main_batt_current, true, this, topSection);
    main_batt_voltageGUI = new Containers("MainBatt Voltage", TITLE_SIZE, "white", main_batt_voltage, false, this, topSection);
    servo_batt_currentGUI = new Containers("SBatt Current", TITLE_SIZE, "white", servo_batt_current, true, this, topSection);
    servo_batt_voltageGUI = new Containers("SBatt Voltage", TITLE_SIZE, "white", servo_batt_voltage, false, this, topSection);
    instantpowerGUI = new Containers("Instant Power", TITLE_SIZE, "white", current_power, false, this, topSection);
    instantthrottleGUI = new Containers("Instant Throttle", TITLE_SIZE, "white", current_throttle, false, this, topSection);

    // Δύο στήλες, χωρίς την ενδιάμεση "κενή" στήλη
    gridLayout->addWidget(main_batt_currentGUI, 0, 0);
    gridLayout->addWidget(main_batt_voltageGUI, 1, 0);
    gridLayout->addWidget(instantpowerGUI, 2, 0);

    gridLayout->addWidget(servo_batt_currentGUI, 0, 1);
    gridLayout->addWidget(servo_batt_voltageGUI, 1, 1);
    gridLayout->addWidget(instantthrottleGUI, 2, 1);

    for (int r = 0; r < 3; ++r) gridLayout->setRowStretch(r, 1);
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);

    mainVLayout->addWidget(topSection, 1);

    QFrame* bottomBar = createBottomToolbar();
    mainVLayout->addWidget(bottomBar);
    this->updateBottomToolbar();

    graphic_timer->start(GRAPHIC_UPDATES_REFRESH_RATE);
}

void GcsGUI::createControlSurfacesScene() {

    auto createCameraBox = [&](QWidget* parent, int cameraIndex) {
        QFrame* box = ModernUi::makePanel(parent);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout* layout = new QVBoxLayout(box);
        layout->setContentsMargins(8, 8, 8, 8);

        QQuickView* view = new QQuickView(sharedQmlEngine, nullptr);
        view->rootContext()->setContextProperty("gcsBackend", this);
        view->rootContext()->setContextProperty("fixedCameraIndex", cameraIndex);
        view->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/MainView.qml"));
        view->rootObject()->setProperty("fixedCameraIndex", cameraIndex);
        view->setResizeMode(QQuickView::SizeRootObjectToView);

        QWidget* container = QWidget::createWindowContainer(view, box);
        layout->addWidget(container);
        return box;
        };

    controlSurfacesSceneWidget->setObjectName("ControlSurfacesSceneWidget");
    QString backgroundStyle = QString(
        "QWidget#ControlSurfacesSceneWidget {"
        "background-color: #000000;"
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);
    controlSurfacesSceneWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(controlSurfacesSceneWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* dashboardWidget = new QWidget(controlSurfacesSceneWidget);
    dashboardWidget->setStyleSheet("background: transparent;");

    QGridLayout* grid = new QGridLayout(dashboardWidget);
    grid->setContentsMargins(40, 30, 40, 20);
    grid->setSpacing(24);

    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setRowStretch(2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 3);

    grid->addWidget(createCameraBox(dashboardWidget, 1), 0, 0);
    grid->addWidget(createCameraBox(dashboardWidget, 2), 1, 0);
    grid->addWidget(createCameraBox(dashboardWidget, 3), 2, 0);

    this->setup3DView(grid);

    // --- Terminal ---
    QFrame* terminalFrame = ModernUi::makePanel(dashboardWidget);
    QVBoxLayout* terminalLayout = new QVBoxLayout(terminalFrame);
    terminalLayout->setContentsMargins(22, 16, 22, 16);

    terminalLog = new QPlainTextEdit(terminalFrame);
    terminalLog->setReadOnly(true);
    terminalLog->setFrameShape(QFrame::NoFrame);
    terminalLog->setStyleSheet(
        "QPlainTextEdit { color: #4ade80; font-family: 'Consolas', 'Courier New', monospace;"
        "                 font-size: 14px; border: none; background: transparent;"
        "                 selection-background-color: #1e90ff; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,60); border-radius: 4px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,100); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    terminalLog->setMaximumBlockCount(150);
    terminalLog->appendPlainText("MAIN SCHEMATIC FEED\nWAITING FOR TELEMETRY...");

    terminalLayout->addWidget(terminalLog);

    grid->addWidget(terminalFrame, 2, 1);

    mainVLayout->addWidget(dashboardWidget, 1);
    mainVLayout->addWidget(createBottomToolbar());
    this->updateBottomToolbar();

    setCameraIndex(0);

    this->setRudderColor(QColor("red"));
    this->setElevatorColor(QColor("white"));
    this->setLeftAileronColor(QColor("white"));
    this->setRightAileronColor(QColor("white"));
}

void GcsGUI::createEarthProjectionScene() {

    auto createCameraBox = [&](QWidget* parent, int cameraIndex, bool loadCamera) {
        QFrame* box = ModernUi::makePanel(parent);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        QVBoxLayout* layout = new QVBoxLayout(box);
        layout->setContentsMargins(8, 8, 8, 8);

        if (loadCamera) {
            QQuickView* view = new QQuickView(sharedQmlEngine, nullptr);
            view->rootContext()->setContextProperty("gcsBackend", this);
            view->rootContext()->setContextProperty("fixedCameraIndex", cameraIndex);
            view->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/MainView.qml"));
            view->rootObject()->setProperty("fixedCameraIndex", cameraIndex);
            view->setResizeMode(QQuickView::SizeRootObjectToView);

            QWidget* container = QWidget::createWindowContainer(view, box);
            layout->addWidget(container);
        }
        return box;
        };

    // Γραμμή "ετικέτα αριστερά, τιμή δεξιά" (χρησιμοποιείται σε Status και GPS)
    auto addRow = [&](QVBoxLayout* parentLayout, QWidget* parent,
        const QString& label, const QString& initialValue, int valuePt) {
            QHBoxLayout* row = new QHBoxLayout();
            QLabel* lblName = ModernUi::makeCaption(label, parent, 10, 1.5);
            QLabel* lblVal = ModernUi::makeValue(initialValue, parent, valuePt);
            row->addWidget(lblName);
            row->addStretch();
            row->addWidget(lblVal);
            parentLayout->addLayout(row);
            return lblVal;
        };

    EarthProjectionSceneWidget->setObjectName("EarthProjectionScene");

    QString backgroundStyle = QString(
        "QWidget#EarthProjectionScene {"
        "background-color: #000000;"
        "background-image: url(%1);"
        "background-position: center;"
        "background-repeat: no-repeat;"
        "}").arg(MAINSCENE_BG);
    EarthProjectionSceneWidget->setStyleSheet(backgroundStyle);

    QVBoxLayout* mainVLayout = new QVBoxLayout(EarthProjectionSceneWidget);
    mainVLayout->setContentsMargins(0, 0, 0, 0);
    mainVLayout->setSpacing(0);

    QWidget* dashboardWidget = new QWidget(EarthProjectionSceneWidget);
    dashboardWidget->setStyleSheet("background: transparent;");

    QGridLayout* grid = new QGridLayout(dashboardWidget);
    grid->setContentsMargins(40, 30, 40, 20);
    grid->setSpacing(24);

    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    grid->setRowStretch(2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 3);

    // --- Camera ---
    grid->addWidget(createCameraBox(dashboardWidget, 1, true), 0, 0);

    // --- Altitude graph: το στυλ έρχεται από το GraphContainer ---
    altitudeGraph = new GraphContainer("Altitude Graph", "m", dashboardWidget);
    altitudeGraph->setFullData(timeHistory, altitudeHistory);
    altitudeGraph->setObjectName("AltitudeGraphContainer");
    grid->addWidget(altitudeGraph, 1, 0);

    // --- Status box ---
    QFrame* statusBox = ModernUi::makePanel(dashboardWidget);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setContentsMargins(28, 22, 28, 22);
    statusLayout->setSpacing(14);

    QLabel* statusTitle = ModernUi::makeCaption("System && Mission Status", statusBox, 11, 3);
    statusTitle->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusTitle);
    statusLayout->addStretch(1);

    satCountLabel = addRow(statusLayout, statusBox, "Satellite count", QString::number(sat_count), 20);
    gSpeedLabel = addRow(statusLayout, statusBox, "Ground speed", QString::number(ground_speed) + " km/h", 20);
    distToHomeLabel = addRow(statusLayout, statusBox, "Dist to home", QString::number(homedist) + " m", 20);
    piTempLabel = addRow(statusLayout, statusBox, "Pi temperature", QString::number(pi_temp) + " °C", 20);

    statusLayout->addStretch(1);
    grid->addWidget(statusBox, 2, 0);

    // --- Map ---
    QFrame* mapBox = ModernUi::makePanel(dashboardWidget);
    mapBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout* mapLayout = new QVBoxLayout(mapBox);
    mapLayout->setContentsMargins(8, 8, 8, 8);

    QQuickView* mapView = new QQuickView(sharedQmlEngine, nullptr);
    mapView->rootContext()->setContextProperty("gcsBackend", this);
    mapView->setSource(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/MapView.qml"));
    mapView->setResizeMode(QQuickView::SizeRootObjectToView);

    QWidget* mapContainer = QWidget::createWindowContainer(mapView, mapBox);
    mapContainer->setMinimumSize(640, 360);
    mapLayout->addWidget(mapContainer);

    grid->addWidget(mapBox, 0, 1, 2, 1);

    // --- Κάτω δεξιά: Servo + GPS ---
    QWidget* bottomRightWidget = new QWidget(dashboardWidget);
    bottomRightWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* bottomRightLayout = new QHBoxLayout(bottomRightWidget);
    bottomRightLayout->setContentsMargins(0, 0, 0, 0);
    bottomRightLayout->setSpacing(24);

    // --- Servo angles ---
    QFrame* controlSurfacesFrame = ModernUi::makePanel(bottomRightWidget);

    QLabel* csTitle = ModernUi::makeCaption("Servo motors desired angles", controlSurfacesFrame, 11, 3);

    auto createServoBox = [&](const QString& labelName, const QString& initialValue) {
        QFrame* sBox = new QFrame();
        sBox->setObjectName("ServoCard");
        sBox->setFixedSize(145, 78);
        sBox->setStyleSheet(
            "QFrame#ServoCard {"
            "  background-color: rgba(255,255,255,10);"
            "  border: 1px solid rgba(255,255,255,30);"
            "  border-radius: 14px;"
            "}");
        QVBoxLayout* sLayout = new QVBoxLayout(sBox);
        sLayout->setContentsMargins(6, 10, 6, 8);
        sLayout->setSpacing(2);

        QLabel* lblName = ModernUi::makeCaption(labelName, sBox, 8, 1.5);
        lblName->setAlignment(Qt::AlignCenter);

        QLabel* lblVal = ModernUi::makeValue(initialValue, sBox, 18);
        lblVal->setObjectName("ServoValueLabel");
        lblVal->setAlignment(Qt::AlignCenter);

        sLayout->addWidget(lblName);
        sLayout->addWidget(lblVal);

        return qMakePair(sBox, lblVal);
        };

    aileronLBox = createServoBox("AILERON L", "0.0").first;
    aileronRBox = createServoBox("AILERON R", "0.0").first;
    elevatorBox = createServoBox("ELEVATOR", "0.0").first;
    rudderBox = createServoBox("RUDDER", "0.0").first;
    flapsBox = createServoBox("FLAPS", "0.0").first;
    lgear1Box = createServoBox("LANDING GEAR 1", "0.0").first;
    lgear2Box = createServoBox("LANDING GEAR 2", "0.0").first;

    QHBoxLayout* row1Layout = new QHBoxLayout();
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(12);
    row1Layout->addWidget(aileronLBox);
    row1Layout->addWidget(aileronRBox);
    row1Layout->addWidget(elevatorBox);
    row1Layout->addWidget(rudderBox);

    QHBoxLayout* row2Layout = new QHBoxLayout();
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(12);
    row2Layout->addStretch();
    row2Layout->addWidget(flapsBox);
    row2Layout->addWidget(lgear1Box);
    row2Layout->addWidget(lgear2Box);
    row2Layout->addStretch();

    QVBoxLayout* csMainLayout = new QVBoxLayout(controlSurfacesFrame);
    csMainLayout->setContentsMargins(26, 20, 26, 20);
    csMainLayout->setSpacing(14);
    csMainLayout->addWidget(csTitle);
    csMainLayout->addLayout(row1Layout);
    csMainLayout->addLayout(row2Layout);
    csMainLayout->addStretch();

    bottomRightLayout->addWidget(controlSurfacesFrame, 65);

    // --- GPS ---
    QFrame* gpsBox = ModernUi::makePanel(bottomRightWidget);
    QVBoxLayout* gpsLayout = new QVBoxLayout(gpsBox);
    gpsLayout->setContentsMargins(26, 20, 26, 20);
    gpsLayout->setSpacing(14);

    QLabel* gpsTitle = ModernUi::makeCaption("GPS position", gpsBox, 11, 3);
    gpsLayout->addWidget(gpsTitle);
    gpsLayout->addStretch(1);

    this->latitudeLabel = addRow(gpsLayout, gpsBox, "Latitude", QString::number(latitude, 'f', 5) + "° N", 18);
    this->longitudeLabel = addRow(gpsLayout, gpsBox, "Longitude", QString::number(longitude, 'f', 5) + "° E", 18);

    gpsLayout->addStretch(1);
    bottomRightLayout->addWidget(gpsBox, 35);

    grid->addWidget(bottomRightWidget, 2, 1);

    mainVLayout->addWidget(dashboardWidget, 1);

    mainVLayout->addWidget(createBottomToolbar());
    this->updateBottomToolbar();
}

void GcsGUI::changeScene() {

    if (lastSceneChange.isValid() && lastSceneChange.elapsed() < 90) return;
    lastSceneChange.start();

    if (current_scene == "EARTH" || current_scene == "SERVO") {
        if (scene_icon_path == PLANEICONPATH) {
            current_scene = "SURFACES";
            sceneStack->setCurrentIndex(1);
        }
        else {
            current_scene = "MAIN";
            sceneStack->setCurrentIndex(0);
        }
    }

    else if (current_scene == "MAIN") {
        current_scene = "SURFACES";
        sceneStack->setCurrentIndex(1);
    }

    else if (current_scene == "SURFACES") {
        current_scene = "MAIN";
        sceneStack->setCurrentIndex(0);
    }

    this->updateBottomToolbar();
}

// CONTROL SURFACES SETTERS (QML SIGNALS) 

void GcsGUI::setLeftAileronValue(qreal value) {
    if (updateValue(m_leftAileronValue, value))
        emit leftAileronValueChanged(); // ΞΥΠΝΑ ΤΟ QML!
}

void GcsGUI::setRightAileronValue(qreal value) {
    if (updateValue(m_rightAileronValue, value))
        emit rightAileronValueChanged();
}

void GcsGUI::setLeftAileronColor(const QColor& color) {
    if (updateValue(m_leftAileronColor, color))
        emit leftAileronColorChanged();
}

void GcsGUI::setRightAileronColor(const QColor& color) {
    if (updateValue(m_rightAileronColor, color))
        emit rightAileronColorChanged();
}

void GcsGUI::setRudderValue(double value) {
    if (updateValue(m_rudderValue, value))
        emit rudderValueChanged();
}

void GcsGUI::setRudderColor(const QColor& color) {
    if (updateValue(m_rudderColor, color))
        emit rudderColorChanged(); // Αυτό λέει στο QML "άλλαξε τώρα!"
}

void GcsGUI::setElevatorValue(double value) {
    if (updateValue(m_elevatorValue, value))
        emit elevatorValueChanged();
}

void GcsGUI::setElevatorColor(const QColor& color) {
    if (updateValue(m_elevatorColor, color))
        emit elevatorColorChanged();
}

void GcsGUI::setLeftFlapsColor(const QColor& color) {
    if (updateValue(m_leftFlapsColor, color))
        emit leftFlapsColorChanged();
}

void GcsGUI::setRightFlapsColor(const QColor& color) {
    if (updateValue(m_rightFlapsColor, color))
        emit rightFlapsColorChanged();
}

qreal GcsGUI::leftAileronValue() const {
    return m_leftAileronValue;
}

qreal GcsGUI::rightAileronValue() const {
    return m_rightAileronValue;
}

// --------------------------------------------------------------

void GcsGUI::setThrottle(qreal val)
{
    if (val < 0) val = 0;

    if (qFuzzyCompare(m_throttle, val))
        return;

    m_throttle = val;
    emit throttleChanged();
    /*
    if (audio_handler) {
        audio_handler->setDynamicVolume("Engine", m_throttle);
    } */
}

void GcsGUI::setCameraIndex(int index) {
    if (m_cameraIndex == index)
        return;
    m_cameraIndex = index;
    emit cameraIndexChanged();
}

void GcsGUI::setup3DView(QGridLayout* layout) {

    QQuickView* view = new QQuickView(sharedQmlEngine, nullptr);
    view->rootContext()->setContextProperty("gcsBackend", this);

    QString qmlPath = QCoreApplication::applicationDirPath() + "/MainView.qml";
    view->setSource(QUrl::fromLocalFile(qmlPath));
    view->setResizeMode(QQuickView::SizeRootObjectToView);

    QWidget* container = QWidget::createWindowContainer(view, this);

    container->setMinimumSize(640, 360);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(container, 0, 1, 2, 1);
}

QFrame* GcsGUI::createBottomToolbar() {

    QFrame* toolbarFrame = new QFrame();
    toolbarFrame->setFixedHeight(100); 
    toolbarFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarFrame->setStyleSheet(
        "QFrame {" "   background-image: none;"  "   background-color: #121212;"          
        "   border-top: 1px solid #444;"  "   border-bottom: none;" "   border-left: none;"    
        "   border-right: none;" "}");
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarFrame);
    toolbarLayout->setContentsMargins(20, 0, 20, 0);
    toolbarLayout->setSpacing(10);

    batteryIndicator = new BatteryWidget();
    batteryIndicator->setObjectName("MyBatteryWidget"); 
    batteryIndicator->setFixedSize(120, 90); 

    auto createIcon = [&](const QString& iconPath, IconType type) {
        QPushButton* btn;

        if (type == Battery) {
            batteryIndicator->setObjectName("MyBatteryWidget");
            btn = batteryIndicator; 
            btn->setStyleSheet(
                "QPushButton {"
                "   background-color: rgba(0, 0, 0, 0);" 
                "   background: transparent;"
                "   border: none;"
                "   border-radius: 10px;"
                "}"
                "QPushButton:hover {"
                "   background-color: rgba(255, 255, 255, 30);" 
                "}"
            );
        }

        else {
            btn = new QPushButton();
            btn->setIcon(QIcon(iconPath));
            btn->setIconSize(QSize(70, 70));
            btn->setFixedSize(90, 90);
            btn->setStyleSheet(
                "QPushButton {" "   background: transparent;" "   border: none;" "   border-radius: 10px;" "}"
                "QPushButton:hover {" "   background-color: rgba(255, 255, 255, 30);"  "}");
        }

        switch (type) {

         case Scene:
            btn->setObjectName("SceneBtn");
            sceneIconButton = btn;
            QObject::connect(btn, &QPushButton::clicked, this, [this]() { 
                this->audio_handler->playSound(audio_click_sfx_volume, "MainClick");
                this->changeScene();
                this->updateBottomToolbar();
                });
            break;

         case Earth:
             btn->setObjectName("EarthBtn");
             QObject::connect(btn, &QPushButton::clicked, this, [this]() {
                 if (current_scene == "EARTH");
                 else {
                     this->audio_handler->playSound(audio_click_sfx_volume, "MainClick");
                     current_scene = "EARTH";
                     sceneStack->setCurrentIndex(2);
                 }

                 });
             break;

         case Settings:
            btn->setObjectName("SettingsBtn");
            QObject::connect(btn, &QPushButton::clicked, this, [this]() { 
                 this->audio_handler->playSound(audio_click_sfx_volume, "MainClick");
                 this->chooseSettings();
                 });
            break;

         case Logging:
             btn->setObjectName("LoggingBtn");
             loggingIconButton = btn;
             break;

         case Status:
             btn->setObjectName("StatusBtn");
             statusIconButton = btn;
             break;

         case Save:
             btn->setObjectName("SaveBtn");
             saveIconButton = btn;
             QObject::connect(btn, &QPushButton::clicked, this, [this]() { 
                 if (unsaved_changes) {
                     this->audio_handler->playSound(audio_click_sfx_volume, "MainClick");
                     this->saveSettings();
                 }; });
             break;

         case Battery:
             break;

        }
        return btn;

        };

    toolbarLayout->addWidget(createIcon(status_icon_path, Status));
    toolbarLayout->addWidget(createIcon(scene_icon_path, Scene));
    toolbarLayout->addWidget(createIcon(earth_icon_path, Earth));
    toolbarLayout->addWidget(createIcon(logging_icon_path, Logging));
    toolbarLayout->addWidget(batteryIndicator, 0, Qt::AlignVCenter);
    

    toolbarLayout->addStretch(2); 

    auto createBottomButton = [&](const QString& text, void (GcsGUI::*slotmethod)() ) {
        QPushButton* btn = new QPushButton(text);
        if (text == "Choose Log Location") // χρειαζεται περισσοτερο χωρο το συγκεκριμενο
         btn->setFixedSize(310, 70);
        else btn->setFixedSize(270, 70);
        btn->setStyleSheet(HOVERED_STYLE);
        QObject::connect(btn, &QPushButton::clicked, this, [this, slotmethod]() {
             this->audio_handler->playSound(audio_click_sfx_volume, "MainClick");
             (this->*slotmethod)();
            });
        return btn;
        };

    toolbarLayout->addWidget(createBottomButton("Open Log File", &GcsGUI::openLogFile));
    toolbarLayout->addWidget(createBottomButton("Choose Log Location", &GcsGUI::chooseLogLocation));
    toolbarLayout->addWidget(createBottomButton("Serial Control", &GcsGUI::serial_control));
    toolbarLayout->addWidget(createIcon(save_icon_path, Save));
    toolbarLayout->addWidget(createIcon(SETTINGSICONPATH, Settings));
    
    return toolbarFrame;
}

void GcsGUI::updateBottomToolbar() {

    if (current_scene == "SURFACES") {
        scene_icon_path = HOMEICONPATH;
    }
    else {
        scene_icon_path = PLANEICONPATH;
    }

    if (armed) status_icon_path = ARMED_ICONPATH;
    else status_icon_path = DISARMED_ICONPATH;

    if (write_logs) logging_icon_path = LOGGING_ON_ICONPATH;
    else logging_icon_path = LOGGING_OFF_ICONPATH;

    if (unsaved_changes) save_icon_path = SAVEICONPATH;
    else save_icon_path = SAVEICON_LOW_OPACITY_PATH;

    QList<QPushButton*> sceneBtns = sceneStack->findChildren<QPushButton*>("SceneBtn");
    for (QPushButton* btn : sceneBtns) {
        btn->setIcon(QIcon(scene_icon_path));
    }

    QList<QPushButton*> statusBtns = sceneStack->findChildren<QPushButton*>("StatusBtn");
    for (QPushButton* btn : statusBtns) {
        btn->setIcon(QIcon(status_icon_path));
    }

    QList<QPushButton*> loggingBtns = sceneStack->findChildren<QPushButton*>("LoggingBtn");
    for (QPushButton* btn : loggingBtns) {
        btn->setIcon(QIcon(logging_icon_path));
    }

    QList<QPushButton*> saveBtns = sceneStack->findChildren<QPushButton*>("SaveBtn");
    for (QPushButton* btn : saveBtns) {
        btn->setIcon(QIcon(save_icon_path));
    }

}

double GcsGUI::getServoAngle(ControlSurfaces* control_surface) {

    int pwm_value = control_surface->current_value;
    double min_angle = control_surface->min_angle;
    double max_angle = control_surface->max_angle;
    int low_pwm_value = control_surface->low_pwm_value;
    int high_pwm_value = control_surface->high_pwm_value;
    double servo_angle = min_angle  + (static_cast<double>(pwm_value - low_pwm_value) * (max_angle - min_angle) / (high_pwm_value- low_pwm_value));

    return servo_angle;

}

QString getPwmHexColor(const ControlSurfaces& surface) {

    if (surface.current_value == 0) return "#000000";

    int val = surface.current_value;
    val = std::clamp(val, surface.low_pwm_value, surface.high_pwm_value);

    // 2. Calculate R and G (Transition from Green to Red)
    // Percentage (0.0 to 1.0)
    float ratio = (float)(val - surface.low_pwm_value) / (surface.high_pwm_value - surface.low_pwm_value);

    int r = (int)(ratio * 255);
    int g = 255 - r;
    int b = 0;

    // 3. Format as Hex String
    char hexBuffer[8];
    std::snprintf(hexBuffer, sizeof(hexBuffer), "#%02X%02X%02X", r, g, b);

    return QString(hexBuffer);
}

void GcsGUI::updateSurfaces() {

    QString rudderColor = getPwmHexColor(rudder);

    QString leftaileronColor = getPwmHexColor(left_aileron);
    QString rightaileronColor = getPwmHexColor(right_aileron);

    QString leftflapsColor = getPwmHexColor(left_flap);
    QString rightflapsColor = getPwmHexColor(right_flap);

    QString elevatorColor = getPwmHexColor(elevator);

    this->setElevatorColor(QColor(elevatorColor));

    this->setRudderColor(QColor(rudderColor));

    this->setLeftAileronColor(QColor(leftaileronColor));
    this->setRightAileronColor(QColor(rightaileronColor));

    this->setLeftFlapsColor(QColor(leftflapsColor));   
    this->setRightFlapsColor(QColor(rightflapsColor));

}

double mapValue(double x, double in_min, double in_max, double out_min, double out_max) {
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void GcsGUI::processData(const QString& data, QString source) {

    QString cleanData = data.trimmed().replace(" ", "");
    QMap<QString, double> values;
    QStringList parts = cleanData.split(",");

    for (const QString& part : parts) {
        QStringList pair = part.split(":");
        if (pair.size() == 2) {
            bool ok;
            double val = pair[1].toDouble(&ok);
            if (ok) {
                values[pair[0]] = val;
            }
        }
    }

    main_batt_voltage = values.value("MV", main_batt_voltage);
    main_batt_current = values.value("MC", main_batt_current);
    servo_batt_voltage = values.value("SV", servo_batt_voltage);
    servo_batt_current = values.value("SC", servo_batt_current);
    ground_speed = values.value("GS", ground_speed);
    altitude = values.value("ALT", altitude);
    latitude = values.value("LAT", latitude);
    longitude = values.value("LON", longitude);
    homedist = values.value("HD", homedist);
    sat_count = values.value("SC", sat_count);
    pi_temp = values.value("PT", pi_temp);

    current_power = main_batt_voltage * main_batt_current;
    setLatitude(latitude);
    setLongitude(longitude);

    if (values.contains("CH1")) {
        int channels[17];
        for (int i = 1; i <= 16; i++) {
            QString chName = QString("CH%1").arg(i);
            int defaultVal = (i == motor.channel) ? 1000 : 1500;
            channels[i] = values.value(chName, defaultVal);
        }

        left_aileron.current_value = channels[left_aileron.channel];
        right_aileron.current_value = channels[right_aileron.channel];
        elevator.current_value = channels[elevator.channel];
        motor.current_value = channels[motor.channel];
        rudder.current_value = channels[rudder.channel];
        left_flap.current_value = channels[left_flap.channel];
        right_flap.current_value = channels[right_flap.channel];
        landing_gear1.current_value = channels[landing_gear1.channel];
        landing_gear2.current_value = channels[landing_gear2.channel];
        arm_status_switch.current_value = channels[arm_status_switch.channel];

        this->updateSurfaces();

        double aileron_Visual_Angle = mapValue(left_aileron.current_value, left_aileron.low_pwm_value, left_aileron.high_pwm_value, -30.0, 30.0);
        double elevator_Visual_Angle = mapValue(elevator.current_value, elevator.low_pwm_value, elevator.high_pwm_value, -30.0, 30.0);
        double throttlePct = mapValue(motor.current_value, motor.low_pwm_value, motor.high_pwm_value, 0.0, 100.0);
        double rudder_Visual_Angle = mapValue(rudder.current_value, rudder.low_pwm_value, rudder.high_pwm_value, -30.0, 30.0);

        current_throttle = throttlePct;
        this->setThrottle(throttlePct);

        if (move_control_surfaces) {
            this->setLeftAileronValue(aileron_Visual_Angle);
            this->setRightAileronValue(-aileron_Visual_Angle);
            this->setElevatorValue(elevator_Visual_Angle);
            this->setRudderValue(rudder_Visual_Angle);
        }

        QStringList pilotActions;
        if (channels[left_aileron.channel] < 1400) pilotActions << "Rolling LEFT 🛩️";
        else if (channels[left_aileron.channel] > 1600) pilotActions << "Rolling RIGHT 🛩️";
        if (channels[elevator.channel] < 1400) pilotActions << "Pitching DOWN ⬇️";
        else if (channels[elevator.channel] > 1600) pilotActions << "Pitching UP ⬆️";
        if (channels[rudder.channel] < 1400) pilotActions << "Yawing LEFT ⬅️";
        else if (channels[rudder.channel] > 1600) pilotActions << "Yawing RIGHT ➡️";
        if (channels[motor.channel] > 1900) pilotActions << "FULL THROTTLE 🔥";
        else if (channels[motor.channel] < 1100) pilotActions << "Throttle IDLE 🛑";
        if (channels[left_flap.channel] > 1600) pilotActions << "Flaps DEPLOYED ⏬";

        QString finalMessage;
        if (pilotActions.isEmpty()) finalMessage = "> PILOT: Level Flight (Sticks Centered)";
        else finalMessage = "> PILOT ACTION: " + pilotActions.join(" | ");

        QList<QPlainTextEdit*> allTerminals = this->findChildren<QPlainTextEdit*>();
        for (QPlainTextEdit* terminal : allTerminals) {
            if (!pilotActions.isEmpty()) {
                terminal->appendPlainText(finalMessage);
            }
        }

        // HANDLING ARM STATUS
        if (arm_status_switch.current_value <= 1500) {
            if (!armed) {
                armed = true;
                if (statusIconButton) this->updateBottomToolbar();
                if (!audio_handler->isSoundPlaying("TelemetryConnected")) this->audio_handler->playSound(100, "Armed");
            }
        }

        else {
            if (armed) {
                armed = false;
                if (statusIconButton) this->updateBottomToolbar();
                this->audio_handler->playSound(100, "Disarmed");
            }
            
        }

    }

    if (write_logs) logToFile(true);
}

void GcsGUI::initLogFile() {
    QString fullPath = log_file_path;

    if (!QFileInfo(fullPath).isAbsolute() || fullPath.isEmpty()) {
        fullPath = QCoreApplication::applicationDirPath() + QDir::separator() + log_file_path;
    }

    QFile file(fullPath);

    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << LOG_HEADERS.join(";") << "\n"; // ΕΡΩΤΗΜΑΤΙΚΟ ΓΙΑ ΤΟ EXCEL
            file.close();
            qDebug() << "Created initial log file:" << fullPath;
        }
    }

    log_file_path = fullPath;
}

void GcsGUI::logToFile(bool status) {

    if (!status || !write_logs) return;

    static qint64 lastLogTime = 0;
    qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();

    int logIntervalMs = 500;

    if (currentTimeMs - lastLogTime < logIntervalMs) {
        return; 
    }
    lastLogTime = currentTimeMs;


    QFile file(log_file_path);
    bool file_exists = file.exists();

    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Error opening log file for appending:" << file.errorString();
        return;
    }

    QTextStream out(&file);

    if (!file_exists || file.size() == 0) {
        out << LOG_HEADERS.join(";") << "\n"; // ΕΡΩΤΗΜΑΤΙΚΟ ΓΙΑ ΤΟ EXCEL
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    QStringList rowData;

    // Η σειρά ΕΔΩ πρέπει να ταιριάζει ΑΠΟΛΥΤΑ με τη σειρά στο LOG_HEADERS
    rowData << timestamp
        << (armed ? "ARMED" : "SAFE")
        << QString::number(main_batt_voltage, 'f', 2)
        << QString::number(main_batt_current, 'f', 2)
        << QString::number(current_power, 'f', 2)
        << QString::number(servo_batt_voltage, 'f', 2)
        << QString::number(servo_batt_current, 'f', 2)
        << QString::number(altitude, 'f', 2)
        << QString::number(ground_speed, 'f', 2)
        << QString::number(homedist, 'f', 2)
        << QString::number(latitude, 'f', 6)
        << QString::number(longitude, 'f', 6)
        << QString::number(pi_temp)
        << QString::number(sat_count)
        << QString::number(current_throttle, 'f', 2)
        << QString::number(motor.current_value)
        << QString::number(rudder.current_value)
        << QString::number(elevator.current_value)
        << QString::number(left_aileron.current_value)
        << QString::number(right_aileron.current_value)
        << QString::number(left_flap.current_value)
        << QString::number(right_flap.current_value)
        << QString::number(landing_gear1.current_value)
        << QString::number(landing_gear2.current_value);

    out << rowData.join(";") << "\n"; // ΕΡΩΤΗΜΑΤΙΚΟ ΓΙΑ ΤΟ EXCEL

    file.close();
}

void GcsGUI::openLogFile()
{
    QString fullFilePath = log_file_path;

    if (!QFileInfo(fullFilePath).isAbsolute() || fullFilePath.isEmpty()) {
        fullFilePath = QCoreApplication::applicationDirPath() + QDir::separator() + log_file_path;
    }

    if (!QFile::exists(fullFilePath)) {
        QMessageBox::warning(this, "File Not Found",
            "Log file does not exist at: " + fullFilePath);
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(fullFilePath))) {
        QMessageBox::critical(this, "Error",
            "Could not open file with the system default application.");
    }
}

void GcsGUI::setBatteryPercentage() {

    int batt_pct = (int)mapValue(main_batt_voltage, min_batt_voltage, max_batt_voltage, 0.0, 100.0);
    if (batt_pct > 100) batt_pct = 100;
    if (batt_pct < 0) batt_pct = 0;
    static int last_batt_pct = -1;

    if (batt_pct != last_batt_pct) {
        last_batt_pct = batt_pct;
        QList<BatteryWidget*> allBatteries = this->findChildren<BatteryWidget*>();
        for (BatteryWidget* batt : allBatteries) {
            batt->setPercentage(batt_pct);
        }
    }
}

void GcsGUI::graphicUpdates() {

    if (!process_begin) return;

    if (process_begin) {

        if (batteryIndicator) {
            setBatteryPercentage();
        }

        double currentTime = graphtimeCounter + ((double)GRAPHIC_UPDATES_REFRESH_RATE / 1000.0);
        graphtimeCounter = currentTime;
        
        auto appendData = [&](QVector<double>& buffer, double value) {
            buffer.append(value);
            if (buffer.size() > MAX_GRAPH_POINTS) buffer.removeFirst();
            }; 

        timeHistory.append(currentTime);
        if (timeHistory.size() > MAX_GRAPH_POINTS) timeHistory.removeFirst();
        appendData(altitudeHistory, altitude);
    
        if (current_scene == "MAIN") {

            double max_expected_power = max_batt_voltage * max_expected_amps;
            double power_gauge_pct = mapValue(current_power, 0.0, max_expected_power, 0.0, 100.0);

            if (power_gauge_pct > 100.0) power_gauge_pct = 100.0;
            if (power_gauge_pct < 0.0) power_gauge_pct = 0.0;

            main_batt_currentGUI->updateData(main_batt_current);
            main_batt_voltageGUI->updateData(main_batt_voltage);
            servo_batt_currentGUI->updateData(servo_batt_current);
            servo_batt_voltageGUI->updateData(servo_batt_voltage);
            instantthrottleGUI->updateData(current_throttle);
            instantpowerGUI->updateData(power_gauge_pct);
            
        }

        else if (current_scene == "EARTH") {
            altitudeGraph->addDataPoint(graphtimeCounter, altitude);

            updateSingleServoBox(aileronLBox, &left_aileron);
            updateSingleServoBox(aileronRBox, &right_aileron);
            updateSingleServoBox(elevatorBox, &elevator);
            updateSingleServoBox(rudderBox, &rudder);
            updateSingleServoBox(flapsBox, &left_flap);
            updateSingleServoBox(lgear1Box, &landing_gear1);
            updateSingleServoBox(lgear2Box, &landing_gear2);
         
        }
        if (satCountLabel) satCountLabel->setText(QString::number(sat_count));
        if (gSpeedLabel) gSpeedLabel->setText(QString::number(ground_speed) + "km/h");
        if (distToHomeLabel) distToHomeLabel->setText(QString::number(homedist) + "m");
        if (piTempLabel) piTempLabel->setText(QString::number(pi_temp) + "C");
        if (latitudeLabel) {
            QString latChar = (latitude >= 0) ? "° N" : "° S";
            latitudeLabel->setText(QString::number(qAbs(latitude), 'f', 5) + latChar);
        }
        if (longitudeLabel) {
            QString lonChar = (longitude >= 0) ? "° E" : "° W";
            longitudeLabel->setText(QString::number(qAbs(longitude), 'f', 5) + lonChar);
        }
    }

}

void GcsGUI::updateSingleServoBox(QWidget* box, ControlSurfaces* surface) {
    if (!box) return; 

    if (QLabel* lbl = box->findChild<QLabel*>("ServoValueLabel")) {
        lbl->setText(QString::number(getServoAngle(surface), 'f', 1));
    }

}

void GcsGUI::handleSerial(bool status) {

    if (status) {

        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Closed previous serial port.";
        }

        serial_port->setPortName(com_port);
        serial_port->setBaudRate(baudrate);

        if (serial_port->open(QIODevice::ReadWrite)) {
            connect(serial_port, &QSerialPort::readyRead, this, &GcsGUI::readSerialData);
            qDebug() << "Opened serial port" << com_port << "at" << baudrate;
        }

        else {
            qWarning() << "Cannot open port" << com_port << ":" << serial_port->errorString();
        }
    }

    else {

        if (serial_port->isOpen()) {
            serial_port->close();
            qDebug() << "Serial port closed.";
        }

    }
}

void GcsGUI::readSerialData() {
    while (serial_port->canReadLine()) {
        QByteArray data = serial_port->readLine();

        if (!data.isEmpty()) {
            QString serialData = QString::fromUtf8(data).trimmed();

            if (!process_begin) {
                process_begin = true;
            }

            if (terminalLog && !serialData.contains("----")) {
                terminalLog->appendPlainText("> " + serialData);
            }

            processData(serialData, "SERIAL");
        }
    }
}

void GcsGUI::serial_control() {

    QWidget* selection_window = new QWidget(nullptr, Qt::Window);
    selection_window->setWindowTitle("Serial Control");
    
    selection_window->setFixedSize(600, 300);
    selection_window->setWindowModality(Qt::ApplicationModal);
    selection_window->setAttribute(Qt::WA_DeleteOnClose);
    selection_window->setWindowIcon(QIcon(ICONPATH));

    QVBoxLayout* layout = new QVBoxLayout(selection_window);
    QFont labelFont("Arial", 14, QFont::Bold);
    QFont widgetFont("Arial", 11);

    QLabel* label1 = new QLabel("Available COM Ports:", selection_window);
    label1->setFont(labelFont);
    layout->addWidget(label1);

    QComboBox* dropdown1 = new QComboBox(selection_window);
    dropdown1->setFont(widgetFont);
    QStringList ports;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    dropdown1->addItems(ports);
    if (!com_port.isEmpty() && ports.contains(com_port)) {
        dropdown1->setCurrentText(com_port);
    }
    layout->addWidget(dropdown1);

    QLabel* label2 = new QLabel("Choose Baudrate:", selection_window);
    label2->setFont(labelFont);
    layout->addWidget(label2);

    QComboBox* dropdown2 = new QComboBox(selection_window);
    dropdown2->setFont(widgetFont);
    dropdown2->addItems(baudrates);
    dropdown2->setCurrentText(QString::number(baudrate));
    layout->addWidget(dropdown2);

    QPushButton* confirmButton = new QPushButton("Connect", selection_window);
    confirmButton->setFont(widgetFont);
    confirmButton->setStyleSheet(HOVERED_STYLE);
    layout->addWidget(confirmButton);

    auto updateButtonState = [=]() {
        // Ελέγχει αν η επιλογή των dropdowns ταυτίζεται με την ενεργή σύνδεση
        bool isCurrentActiveConnection = (serial_port && serial_port->isOpen() && com_port == dropdown1->currentText() &&
            baudrate == dropdown2->currentText().toInt());

        if (isCurrentActiveConnection) {
            confirmButton->setText("Connected ✓");
            confirmButton->setStyleSheet("background-color: green; color: white; font-weight: bold;");
        }
        else {
            confirmButton->setText("Connect");
            confirmButton->setStyleSheet(HOVERED_STYLE);
        }
        confirmButton->setEnabled(true);
        };

    connect(dropdown1, &QComboBox::currentTextChanged, this, [=]() { updateButtonState(); });
    connect(dropdown2, &QComboBox::currentTextChanged, this, [=]() { updateButtonState(); });

    // Initial Call
    updateButtonState();

    connect(confirmButton, &QPushButton::clicked, this, [=]() {

        if (serial_port && serial_port->isOpen() && com_port == dropdown1->currentText() && baudrate == dropdown2->currentText().toInt()) {
            return;
        }

        confirmButton->setEnabled(false);
        confirmButton->setText("Connecting...");
        confirmButton->setStyleSheet("background-color: #ff9800; color: white; font-weight: bold;"); // Πορτοκαλί
        QCoreApplication::processEvents(); // Ζωγραφίζει το UI άμεσα

        com_port = dropdown1->currentText();
        baudrate = dropdown2->currentText().toInt();
        audio_handler->playSound(audio_click_sfx_volume, "MainClick");

        confirmPort();

        if (serial_port && serial_port->isOpen()) {
            updateButtonState(); 
        }
        else {
            confirmButton->setText("Connection Failed ✖");
            confirmButton->setStyleSheet("background-color: red; color: white; font-weight: bold;");
            confirmButton->setEnabled(true);

            QTimer::singleShot(2000, selection_window, [=]() {
                updateButtonState();
                });
        }
        });

    selection_window->show();
}

void GcsGUI::confirmPort() {
    qDebug() << "Selected COM port:" << com_port;
    qDebug() << "Selected baudrate:" << baudrate;
    handleSerial(running);
}

// --- SETTINGS FIELDS ---
void GcsGUI::setupSettingsFields() {
    settings_fields = QVector<SettingsElements>{
        // --- General / Audio ---
        { "Sound", "", "", false, &this->soundOn, SettingsElements::Bool },
        { "Audio Click Sound Volume", "", "", true, &this->audio_click_sfx_volume, SettingsElements::Int },
        { "Write Logs", "", "", false, &this->write_logs, SettingsElements::Bool },
        { "MQTT Topic", "", "", false, &this->mqtt_receive_topic, SettingsElements::String },
        { "MQTT Broker", "", "", false, &this->mqtt_broker, SettingsElements::String },
        { "MQTT Port", "", "", true, &this->mqtt_port, SettingsElements::Int },
        { "Min Battery Voltage", "", "", true, &min_batt_voltage, SettingsElements::Double },
        { "Max Battery Voltage", "", "", true, &max_batt_voltage, SettingsElements::Double },
        // --- PWM Bounds ---
        { "Min Motor (us)", "", "", true, &motor.low_pwm_value, SettingsElements::Int },
        { "Max Motor (us)", "", "", true, &motor.high_pwm_value, SettingsElements::Int },
        { "Min Rudder (us)", "", "", true, &rudder.low_pwm_value, SettingsElements::Int },
        { "Max Rudder (us)", "", "", true, &rudder.high_pwm_value, SettingsElements::Int },
        { "Min Elevator (us)", "", "", true, &elevator.low_pwm_value, SettingsElements::Int },
        { "Max Elevator (us)", "", "", true, &elevator.high_pwm_value, SettingsElements::Int },
        { "Min Left Aileron (us)", "", "", true, &left_aileron.low_pwm_value, SettingsElements::Int },
        { "Max Left Aileron (us)", "", "", true, &left_aileron.high_pwm_value, SettingsElements::Int },
        { "Min Right Aileron (us)", "", "", true, &right_aileron.low_pwm_value, SettingsElements::Int },
        { "Max Right Aileron (us)", "", "", true, &right_aileron.high_pwm_value, SettingsElements::Int },
        { "Min Left Flap (us)", "", "", true, &left_flap.low_pwm_value, SettingsElements::Int },
        { "Max Left Flap (us)", "", "", true, &left_flap.high_pwm_value, SettingsElements::Int },
        { "Min Right Flap (us)", "", "", true, &right_flap.low_pwm_value, SettingsElements::Int },
        { "Max Right Flap (us)", "", "", true, &right_flap.high_pwm_value, SettingsElements::Int },
        { "Min Landing Gear 1 Flap (us)", "", "", true, &landing_gear1.low_pwm_value, SettingsElements::Int },
        { "Max Landing Gear 1 Flap (us)", "", "", true, &landing_gear1.high_pwm_value, SettingsElements::Int },
        { "Min Landing Gear 2 Flap (us)", "", "", true, &landing_gear2.low_pwm_value, SettingsElements::Int },
        { "Max Landing Gear 2 Flap (us)", "", "", true, &landing_gear2.high_pwm_value, SettingsElements::Int },

        // --- Transmitter Channel Mapping ---
        { "Motor Channel", "", "", true, &motor.channel, SettingsElements::Int },
        { "Rudder Channel", "", "", true, &rudder.channel, SettingsElements::Int },
        { "Elevator Channel", "", "", true, &elevator.channel, SettingsElements::Int },
        { "Left Aileron Channel", "", "", true, &left_aileron.channel, SettingsElements::Int },
        { "Right Aileron Channel", "", "", true, &right_aileron.channel, SettingsElements::Int },
        { "Left Flap Channel", "", "", true, &left_flap.channel, SettingsElements::Int },
        { "Right Flap Channel", "", "", true, &right_flap.channel, SettingsElements::Int },
        { "Arm Switch Channel", "", "", true, &arm_status_switch.channel, SettingsElements::Int },
        { "Landing Gear 1 Channel", "", "", true, &landing_gear1.channel, SettingsElements::Int },
        { "Landing Gear 2 Channel", "", "", true, &landing_gear2.channel, SettingsElements::Int }

    };
}

void GcsGUI::chooseSettings() {

    QWidget* settings_window = new QWidget(nullptr, Qt::Window);
    settings_window->setWindowTitle("Settings");
    settings_window->setFixedSize(500, 750);
    settings_window->setWindowModality(Qt::ApplicationModal);
    settings_window->setAttribute(Qt::WA_DeleteOnClose);
    settings_window->setWindowIcon(QIcon(ICONPATH));

    QVBoxLayout* main_layout = new QVBoxLayout(settings_window);
    QTabWidget* tabWidget = new QTabWidget(settings_window);

    QWidget* generalTab = new QWidget(), * audioTab = new QWidget(), * controlsurfacesTab = new QWidget(), * tcmTab = new QWidget();

    QVBoxLayout* generalLayout = new QVBoxLayout(generalTab),
        * audioLayout = new QVBoxLayout(audioTab), * tcmLayout = new QVBoxLayout(tcmTab);

    QScrollArea* controlsurfacesScrollArea = new QScrollArea();
    QWidget* controlsurfacesScrollWidget = new QWidget();
    QVBoxLayout* controlsurfacesLayout = new QVBoxLayout(controlsurfacesScrollWidget);
    controlsurfacesScrollArea->setWidget(controlsurfacesScrollWidget);
    controlsurfacesScrollArea->setWidgetResizable(true);
    controlsurfacesScrollArea->setFrameShape(QFrame::NoFrame);
    QVBoxLayout* controlsurfacesTabMainLayout = new QVBoxLayout(controlsurfacesTab);
    controlsurfacesTabMainLayout->addWidget(controlsurfacesScrollArea);

    settings_fields = QVector<SettingsElements>{
        {"Sound", "", "", false, &this->soundOn, SettingsElements::Bool},
        {"Audio Click Sound Volume", "", "", true, &this->audio_click_sfx_volume, SettingsElements::Int},
        {"Write Logs", "", "", false, &this->write_logs, SettingsElements::Bool},
        {"MQTT Topic", "", "", false, &this->mqtt_receive_topic, SettingsElements::String},
        {"MQTT Broker", "", "", false, &this->mqtt_broker, SettingsElements::String},
        {"MQTT Port", "", "", true, &this->mqtt_port, SettingsElements::Int},
        {"Min Battery Voltage", "", "", true, &min_batt_voltage, SettingsElements::Double},
        {"Max Battery Voltage", "", "", true, &max_batt_voltage, SettingsElements::Double},
        {"Min Motor (us)", "", "", true, &motor.low_pwm_value, SettingsElements::Int},
        {"Max Motor (us)", "", "", true, &motor.high_pwm_value, SettingsElements::Int},
        {"Min Rudder (us)", "", "", true, &rudder.low_pwm_value, SettingsElements::Int},
        {"Max Rudder (us)", "", "", true, &rudder.high_pwm_value, SettingsElements::Int},
        {"Min Elevator (us)", "", "", true, &elevator.low_pwm_value, SettingsElements::Int},
        {"Max Elevator (us)", "", "", true, &elevator.high_pwm_value, SettingsElements::Int},
        {"Min Left Aileron (us)", "", "", true, &left_aileron.low_pwm_value, SettingsElements::Int},
        {"Max Left Aileron (us)", "", "", true, &left_aileron.high_pwm_value, SettingsElements::Int},
        {"Min Right Aileron (us)", "", "", true, &right_aileron.low_pwm_value, SettingsElements::Int},
        {"Max Right Aileron (us)", "", "", true, &right_aileron.high_pwm_value, SettingsElements::Int},
        {"Min Left Flap (us)", "", "", true, &left_flap.low_pwm_value, SettingsElements::Int},
        {"Max Left Flap (us)", "", "", true, &left_flap.high_pwm_value, SettingsElements::Int},
        {"Min Right Flap (us)", "", "", true, &right_flap.low_pwm_value, SettingsElements::Int},
        {"Max Right Flap (us)", "", "", true, &right_flap.high_pwm_value, SettingsElements::Int},
        {"Min Landing Gear 1 Flap (us)", "", "", true,&landing_gear1.low_pwm_value, SettingsElements::Int },
        {"Max Landing Gear 1 Flap (us)", "", "", true, &landing_gear1.high_pwm_value, SettingsElements::Int },
        {"Min Landing Gear 2 Flap (us)", "", "", true, &landing_gear2.low_pwm_value, SettingsElements::Int },
        {"Max Landing Gear 2 Flap (us)", "", "", true, &landing_gear2.high_pwm_value, SettingsElements::Int },
        {"Motor Channel", "", "", true, &motor.channel, SettingsElements::Int},
        {"Rudder Channel", "", "", true, &rudder.channel, SettingsElements::Int},
        {"Elevator Channel", "", "", true, &elevator.channel, SettingsElements::Int},
        {"Left Aileron Channel", "", "", true, &left_aileron.channel, SettingsElements::Int},
        {"Right Aileron Channel", "", "", true, &right_aileron.channel, SettingsElements::Int},
        {"Left Flap Channel", "", "", true, &left_flap.channel, SettingsElements::Int},
        {"Right Flap Channel", "", "", true, &right_flap.channel, SettingsElements::Int},
        {"Arm Switch Channel", "", "", true, &arm_status_switch.channel, SettingsElements::Int},
        {"Landing Gear 1 Channel", "", "", true,& landing_gear1.channel, SettingsElements::Int },
        {"Landing Gear 2 Channel", "", "", true, &landing_gear2.channel, SettingsElements::Int}
        
    };

    for (auto& field : settings_fields) {

        if (field.label) { delete field.label; field.label = nullptr; }
        if (field.entry) { delete field.entry; field.entry = nullptr; }

        field.label = new QLabel(field.labeltext + ":", settings_window);

        if (field.type == SettingsElements::Bool) {
            QCheckBox* checkBox = new QCheckBox(field.labeltext.contains("Sound") ? "Enable Sound" : "Enabled", settings_window);
            checkBox->setChecked(*(static_cast<bool*>(field.targetVar)));
            field.entry = checkBox;
        }

        else if (field.type == SettingsElements::String) {
            QString currentVal = *(static_cast<QString*>(field.targetVar));
            QLineEdit* lineEdit = new QLineEdit(currentVal, settings_window);
            field.entry = lineEdit;
        }

        else if (field.type == SettingsElements::Int) {
            QString currentValStr = QString::number(*(static_cast<int*>(field.targetVar)));
            QLineEdit* lineEdit = new QLineEdit(currentValStr, settings_window);

            if (field.labeltext.contains("Channel"))
                lineEdit->setValidator(new QIntValidator(1, 16, lineEdit));
            else
                lineEdit->setValidator(new QIntValidator(0, 100000, lineEdit));

            field.entry = lineEdit;
        }

        else if (field.type == SettingsElements::Double) {
            double currentVal = *(static_cast<double*>(field.targetVar));
            QString currentValStr = QString::number(currentVal, 'f', 1); // 1 δεκαδικό
            QLineEdit* lineEdit = new QLineEdit(currentValStr, settings_window);
            lineEdit->setValidator(new QDoubleValidator(0.0, 1000.0, 2, lineEdit)); 
            field.entry = lineEdit;
        }

        QVBoxLayout* target = generalLayout;

        if (field.labeltext.contains("Sound")) target = audioLayout;
        else if (field.labeltext.contains("Channel")) target = tcmLayout;
        else if (field.labeltext.contains("Motor") || field.labeltext.contains("Rudder") ||
            field.labeltext.contains("Elevator") || field.labeltext.contains("Aileron") ||
            field.labeltext.contains("Flap")) target = controlsurfacesLayout;
        else if (field.labeltext.contains("MQTT") || field.labeltext.contains("Battery") ) target = generalLayout;

        target->addWidget(field.label);
        target->addWidget(field.entry);
    }

    for (auto l : { generalLayout, audioLayout, controlsurfacesLayout, tcmLayout }) l->addStretch();

    tabWidget->addTab(generalTab, "General");
    tabWidget->addTab(tcmTab, "Transmitter Channel Mapping");
    tabWidget->addTab(controlsurfacesTab, "Control Surfaces PWM Bounds");
    tabWidget->addTab(audioTab, "Audio");

    QPushButton* confirmButton = new QPushButton("Confirm Settings", settings_window);
    confirmButton->setStyleSheet(HOVERED_STYLE_SETTINGS);

    main_layout->addWidget(tabWidget);
    main_layout->addWidget(confirmButton);

    connect(confirmButton, &QPushButton::clicked, this, [this, settings_window]() {
        bool mqtt_changed = false;

        for (const auto& field : settings_fields) {

            if (field.targetVar == nullptr) continue;

            if (field.type == SettingsElements::Bool) {
                QCheckBox* checkbox = qobject_cast<QCheckBox*>(field.entry);
                if (checkbox) {
                    bool old_val = *(static_cast<bool*>(field.targetVar));
                    bool new_val = checkbox->isChecked();
                    if (old_val != new_val) {
                        unsaved_changes = true;
                        *(static_cast<bool*>(field.targetVar)) = new_val;

                        if (field.labeltext == "Write Logs") this->write_logs = new_val;
                    }
                }
            }

            else if (field.type == SettingsElements::Int) {
                QLineEdit* le = qobject_cast<QLineEdit*>(field.entry);
                if (le) {
                    int old_val = *(static_cast<int*>(field.targetVar));
                    int new_val = le->text().toInt();
                    if (old_val != new_val) {
                        unsaved_changes = true;
                        *(static_cast<int*>(field.targetVar)) = new_val;
                    }
                }
            }

            else if (field.type == SettingsElements::String) {
                QLineEdit* le = qobject_cast<QLineEdit*>(field.entry);
                if (le) {
                    QString old_string = *(static_cast<QString*>(field.targetVar));
                    QString new_string = le->text();
                    if (old_string != new_string) {
                        unsaved_changes = true;
                        *(static_cast<QString*>(field.targetVar)) = new_string;

                        if (field.labeltext.contains("MQTT")) {
                            mqtt_changed = true;
                        }
                    }
                }
            }
            else if (field.type == SettingsElements::Double) {
                QLineEdit* le = qobject_cast<QLineEdit*>(field.entry);
                if (le) {
                    double old_val = *(static_cast<double*>(field.targetVar));
                    double new_val = le->text().replace(',', '.').toDouble(); // Ασφάλεια για κόμμα vs τελεία
                    if (old_val != new_val) {
                        unsaved_changes = true;
                        *(static_cast<double*>(field.targetVar)) = new_val;
                    }
                }
            }

        }

        this->audio_handler->setSoundStatus(this->soundOn);

        if (unsaved_changes) this->updateBottomToolbar();

        if (mqtt_changed) {
            qDebug() << "MQTT settings changed! Restarting connection...";
            this->start_mqtt();
        }

        settings_window->close();
        });

    settings_window->show();
}

void GcsGUI::loadSettings() {
    QFile file(save_file_path);

    if (!file.exists()) {
        qDebug() << "No settings file found. Using defaults.";
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open settings file for reading.";
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        if (line.startsWith("-") || line.trimmed().isEmpty()) continue;

        // Κόβουμε τη γραμμή στο πρώτο ':' 
        // (χρησιμοποιούμε indexOf αντί για split σε περίπτωση που πχ το MQTT URL έχει μέσα ':')
        int colonIndex = line.indexOf(":");
        if (colonIndex == -1) continue;


        QString key = line.left(colonIndex).trimmed();
        QString value = line.mid(colonIndex + 1).trimmed();

        for (const auto& field : settings_fields) {

            if (field.labeltext == key && field.targetVar != nullptr) {

                if (field.type == SettingsElements::Bool) {
                    *(static_cast<bool*>(field.targetVar)) = (value.toLower() == "yes" || value.toLower() == "true");
                }
                else if (field.type == SettingsElements::Int) {
                    *(static_cast<int*>(field.targetVar)) = value.toInt();
                }
                else if (field.type == SettingsElements::Double) {
                    *(static_cast<double*>(field.targetVar)) = value.toDouble();
                }
                else if (field.type == SettingsElements::String) {
                    *(static_cast<QString*>(field.targetVar)) = value;
                }

                break;
            }
        }
    }

    file.close();

    if (this->audio_handler) {
        this->audio_handler->setSoundStatus(soundOn);
    }

    this->updateBottomToolbar();

    qDebug() << "Settings loaded successfully!";
}

void GcsGUI::saveSettings() {

    int label_width = 30;

    if (save_file == nullptr) {
        save_file = new QFile(save_file_path, this);

        if (!save_file->open(QIODevice::Append | QIODevice::Text)) {
            delete save_file;
            save_file = nullptr;
            return;
        }
        QTextStream out(save_file);
        out << "\nLast Save Settings Session: "
            << QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss")
            << "\n";
    }

    QTextStream out(save_file);
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    
    for (const auto& field : settings_fields) {
        if (field.targetVar == nullptr) continue; // Ασφάλεια: αν ξεχάσεις κάποιον pointer

        QString valueStr;

        if (field.type == SettingsElements::Bool) {
            bool val = *(static_cast<bool*>(field.targetVar));
            valueStr = val ? "Yes" : "No";
        }
        else if (field.type == SettingsElements::Int) {
            int val = *(static_cast<int*>(field.targetVar));
            valueStr = QString::number(val);
        }
        else if (field.type == SettingsElements::Double) {
            double val = *(static_cast<double*>(field.targetVar));
            valueStr = QString::number(val, 'f', 2);
        }
        else if (field.type == SettingsElements::String) {
            QLineEdit* le = qobject_cast<QLineEdit*>(field.entry);
            if (le) {
                QString* target = static_cast<QString*>(field.targetVar);
                if (*target != le->text()) {
                    unsaved_changes = true;
                    *target = le->text();
                }
            }
        }

        QString cleanLabel = field.labeltext;
        cleanLabel.remove(":");
        cleanLabel = cleanLabel.trimmed();

        out << QString("%1 : %2\n").arg(cleanLabel, -label_width).arg(valueStr);
    }  
    out << "------------------------------------------------\n";
    unsaved_changes = false;
    this->updateBottomToolbar();
    save_file->flush();
}
// --------------------------------------------------------------
void GcsGUI::chooseLogLocation() {

    QString initialDir = QFileInfo(log_file_path).path();
    QString newFilePath = QFileDialog::getSaveFileName(
        this,
        "Select Log File Location",
        initialDir, 
        "CSV Log (*.csv);;Text Log (*.txt);;All files (*.*)"
    );

    if (!newFilePath.isEmpty()) {
        if (!newFilePath.endsWith(".csv", Qt::CaseInsensitive) && !newFilePath.endsWith(".txt", Qt::CaseInsensitive)) {
            newFilePath += ".csv";
        }

        log_file_path = newFilePath;

        initLogFile();
        qDebug() << "New log file location set to:" << log_file_path;
    }
}

QString GcsGUI::getHoverStyle(const QString& baseColor) {

    QString styleString =
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 %1, stop:1 #00c6ff);"
        "color: white;" "border: 1px solid %1;" "border-radius: 8px;" 
        "padding: 10px 20px;" "font-weight: bold;" "font-size: 25px;" 
        "text-align: center;" "}" "QPushButton:hover {" "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #007bff, stop:1 #0056b3);"    
        "border-color: #007bff;" "}" "QPushButton:pressed {" "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #003c7d, stop:1 #0099cc);"
        "padding-top: 12px;" "padding-bottom: 8px;" "}";
        
    return styleString.arg(baseColor);
}
