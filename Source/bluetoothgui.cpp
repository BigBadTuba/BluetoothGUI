#include "bluetoothgui.h"
#include "ui_bluetoothgui.h"
#include <QTimer>


BluetoothGUI::BluetoothGUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BluetoothGUI), localDevice(new QBluetoothLocalDevice)
{
    ui->setupUi(this);

    // Can only change focus with mouse clicks
    setFocusPolicy(Qt::ClickFocus);
    // Allow graphical widget to accept focus by mouseclick
    ui->openGLWidget->setFocusPolicy(Qt::ClickFocus);

    elapsed = 0;
    ui->openGLWidget->setAutoFillBackground(false);
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BluetoothGUI::animate);
    timer->start(50);
    image_scale = 1;
    translation.setX(0);
    translation.setY(0);
    ui->connected_device->setText("None");

    connect(localDevice, SIGNAL(deviceDisconnected(QBluetoothAddress)), this, SLOT(lostConnection(QBluetoothAddress)));
    connect(localDevice, SIGNAL(deviceConnected(QBluetoothAddress)), this, SLOT(newConnection(QBluetoothAddress)));

    //    connect(ui->search, SIGNAL (released()), this, SLOT (SearchForDevices()));
    //    connect(ui->connect, SIGNAL (released()), this, SLOT (ConnectToDevice()));

    // These are not implemented since it doesn't work to connect through the GUI
    //    connect(ui->search, SIGNAL(released()), this, SLOT(startScan()));
    //    connect(ui->connect, SIGNAL(released()), this, SLOT(ConnectToDevice()));

//    connect(localDevice, SIGNAL(pairingFinished(QBluetoothAddress,QBluetoothLocalDevice::Pairing))
//            , this, SLOT(pairingDone(QBluetoothAddress,QBluetoothLocalDevice::Pairing)));

    //     Check if Bluetooth is available on this device
    if (localDevice->isValid()) {
        // Turn Bluetooth on
        localDevice->powerOn();

        // Read local device name
        localDeviceName = localDevice->name();

        // Make it visible to others
        localDevice->setHostMode(QBluetoothLocalDevice::HostDiscoverable);

        // Get connected devices
        QList<QBluetoothAddress> remotes;
        remotes = localDevice->connectedDevices();

        qDebug() << "My name is: " << localDeviceName;
        int n = remotes.length();
        qDebug() << "Connected to " << n << " device(s): ";
        for(int i = 0; i < n; i ++){
            qDebug() << remotes.at(i) << ", ";
        }

        // The address for HM-10 on arduino is: D4:36:39:D8:D8:23, check if it is connected
        for(int i = 0; i < remotes.length(); i++){
            if(remotes.at(i).toString() == "D4:36:39:D8:D8:23"){
                connectedDeviceAddress = remotes.at(i);
                qDebug() << "Found HM10!";
                ui->connected_device->setText("HM-10");
            }
        }
    }
}

BluetoothGUI::~BluetoothGUI()
{
    delete ui;
}

void BluetoothGUI::animate()
{
    elapsed = (elapsed + qobject_cast<QTimer*>(sender())->interval()) % 1000;
    update();
}

void BluetoothGUI::paintEvent(QPaintEvent *event)
{
    QSize size =  ui->openGLWidget->size();
    int origin_x = size.width()/2;
    int origin_y = size.height()/2;

    QPainter painter;
    painter.begin(ui->openGLWidget);
    painter.setRenderHint(QPainter::Antialiasing);
    brush.paint(&painter, event, elapsed, origin_x, origin_y, image_scale, translation);
    painter.end();
}

/*
 * Handles events produces when scrolling mouse wheel
 * */
void BluetoothGUI::wheelEvent(QWheelEvent *event){
    // If map is in focus
    if(ui->openGLWidget->hasFocus()){
        // Calculate the number of degrees the wheel has been turned
        QPoint numDegrees = event->angleDelta() / 8 / 15;
        int scrolls = numDegrees.y();
        // If the image is larger than 0.01 and the direction is positive
        if(image_scale > 0.01 || scrolls > 0){
            image_scale += 0.01*scrolls;
        }
        // If the directions is negative or the scale somehow ended up smaller than 0.01
        else if(scrolls < 0 && image_scale <= 0.01){
            image_scale = 0.01;
        }
        qDebug() << "Scale: " << image_scale;
    }
}

/*
 * Handles events produces when pressing any mouse button
 * */
void BluetoothGUI::mousePressEvent(QMouseEvent *event){
    // If left button is pressed
    if(event->button() == Qt::LeftButton){
        // If the map is in focus
        if(ui->openGLWidget->hasFocus()){
            dragging_map = true;
            // Start position of the cursor
            cursor_start = event->pos() + translation;

        }
    }
}

/*
 * Handles events produces when any mouse button is released
 * */
void BluetoothGUI::mouseReleaseEvent(QMouseEvent *event){
    // If the left button is released and dragging_map is true
    if (event->button() == Qt::LeftButton && dragging_map) {
        // Calculate new translsation
        translateMap(event->pos());
        dragging_map = false;
    }
}


/*
 * Handles events produces when the mouse is moved
 * */
void BluetoothGUI::mouseMoveEvent(QMouseEvent *event){
    // If leftmouse is pressed and the map is being dragged
    if((event->buttons() & Qt::LeftButton) && dragging_map){
        // Calculate new translation
        translateMap(event->pos());
    }
}

/*
 * Translates the map in the openGLwidget
 * */
void BluetoothGUI::translateMap(QPoint mouse_position){
    // Calculate translation from the starting position of the cursor
    QPoint new_translation = (cursor_start - mouse_position);
    qDebug() << "Translation: " << new_translation;
    translation = new_translation;
}


void BluetoothGUI::sendTestMessage(const QString &message)
{
    QByteArray text = message.toUtf8();
}

void BluetoothGUI::startScan()
{
    qDebug() << "Start scan ...";
    startDeviceDiscovery();
    addDevicesToList();
    qDebug() << "Scanning complete.";
}

/*
 *  Search for nearvy devices
 */

void BluetoothGUI::startDeviceDiscovery()
{
    // Clear list of nearby devices
    nearbyDevices.clear();

    // Create a discovery agent and connect to its signals
    QBluetoothDeviceDiscoveryAgent *discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));

    // Start a discovery
    discoveryAgent->start();
}

/*
 * When a device is discovered it is added to list of nearby devices
 */
void BluetoothGUI::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    nearbyDevices.append(device);

    qDebug() << "Found new device:" << device.name() << '(' << device.address().toString() << ')';
}

/*
 * When a device has disconnected
 * */
void BluetoothGUI::lostConnection(const QBluetoothAddress &address)
{
    if(address.toString() == "D4:36:39:D8:D8:23"){
        qDebug() << "Lost connection to HM-10!";
        ui->connected_device->setText("None");
    }
}

/*
 * When a device is connected
 * */
void BluetoothGUI::newConnection(const QBluetoothAddress &address)
{
    if(address.toString() == "D4:36:39:D8:D8:23"){
        qDebug() << "Found connection to HM-10!";
        ui->connected_device->setText("HM-10");
    }
}

/*
 * Adds devives in list nearbyDevices to the list presented in the GUI
 * */
void BluetoothGUI::addDevicesToList()
{
    // Clear list
    ui->devices->clear();

    for(int i = 0; i < nearbyDevices.length(); i++){
        ui->devices->addItem(nearbyDevices.at(i).name());
    }
}

/*
 * Function that attempts to connect to the selected device.
 * */
void BluetoothGUI::ConnectToDevice()
{
    int index = ui->devices->currentIndex();
    qDebug() << "Connecting to device: " << nearbyDevices.at(index).name() << " ...";

    // Establish connection
    connectedDevice = nearbyDevices.at(index);
    QBluetoothAddress address = connectedDevice.address();
    localDevice->requestPairing(address, QBluetoothLocalDevice::Paired);
}


/*
 * When pairing has been performed this funciton handles the output, if it succeeded or not.
 * */
void BluetoothGUI::pairingDone(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
    //    QList<QListWidgetItem *> items = ui->devices->findItems(address.toString(), Qt::MatchContains);

    if (pairing == QBluetoothLocalDevice::Paired || pairing == QBluetoothLocalDevice::AuthorizedPaired ) {
        ui->connected_device->setText(connectedDevice.name());
    } else {
        ui->connected_device->setText("None");
    }
}

// Slots
//void BluetoothGUI::SearchForDevices()
//{
//    // Clear list
//    ui->devices->clear();

//    for(int i = 0; i < 5; i++){
//        QString s = "Testing";
//        ui->devices->addItem(s);
//    }
//}


