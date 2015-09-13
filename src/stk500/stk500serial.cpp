#include "stk500serial.h"
#include <QDebug>
#include <QApplication>
#include <QSerialPortInfo>
#include "../dialogs/progressdialog.h"

stk500Serial::stk500Serial(QWidget *owner)
    : QObject(owner) {
    process = NULL;
}

bool stk500Serial::isOpen() {
    return (process != NULL && !process->closeRequested);
}

bool stk500Serial::isSerialOpen() {
    return isOpen() && process->serialBaud;
}

void stk500Serial::open(QString & portName) {
    // If already opened; ignore
    if (isOpen() && (process->portName == portName)) {
        return;
    }
    // Close the current process if opened
    close();

    // Start a new process for the new port
    process = new stk500_ProcessThread(this, portName);
    process->start();
}

void stk500Serial::close() {
    if (isOpen()) {
        // Tell the process to quit if running
        if (process->isRunning) {
            process->closeRequested = true;
            process->cancelTasks();

            // Wait with 1s timeout for the process to exit
            for (int i = 0; process->isRunning && i < 100; i++) {
                QThread::msleep(10);
            }

            // Force-terminate the thread if still running (locked)
            if (process->isRunning) {
                process->terminate();
            }
        }

        delete process;
        process = NULL;
    }
}

void stk500Serial::notifyStatus(stk500_ProcessThread*, QString status) {
    emit statusChanged(status);
}

void stk500Serial::notifyClosed(stk500_ProcessThread *) {
    emit closed();
}

void stk500Serial::notifySerialOpened(stk500_ProcessThread *) {
    emit serialOpened();
}

void stk500Serial::notifyTaskFinished(stk500_ProcessThread *, stk500Task *task) {
    task->finish();
    emit taskFinished(task);
}

void stk500Serial::execute(stk500Task &task, bool asynchronous, bool dialogDelay) {
    QList<stk500Task*> tasks;
    tasks.append(&task);
    executeAll(tasks, asynchronous, dialogDelay);
}

void stk500Serial::executeAll(QList<stk500Task*> tasks, bool asynchronous, bool dialogDelay) {
    /* No tasks - don't do anything */
    if (tasks.isEmpty()) return;

    /* If not open, fail all tasks right away */
    if (!isOpen()) {
        for (int i = 0; i < tasks.count(); i++) {
            tasks[i]->setError(ProtocolException("Port is not opened"));
        }
        return;
    }

    /* Enqueue the other tasks */
    process->tasksLock.lock();
    int totalSyncTasks = 0;
    for (int i = 0; i < tasks.length(); i++) {
        if (asynchronous) {
            process->asyncTasks.enqueue(tasks[i]);
        } else {
            process->syncTasks.enqueue(tasks[i]);
            totalSyncTasks = process->syncTasks.count();
        }
    }
    process->isProcessing = true;
    process->tasksLock.unlock();
    process->wake();

    /* If specified, wait until processing completes */
    if (asynchronous) {
        return;
    }

    /* Show a dialog while processing all synchronized tasks */
    ProgressDialog *dialog = NULL;
    qint64 currentTime;
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    bool isWaitCursor = true;
    qint64 waitTimeout = dialogDelay ? 1200 : 0;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    stk500Task *task;
    int processedCount;
    while (process->isProcessing) {
        // Obtain currently executing synchronized task
        process->tasksLock.lock();
        if (process->syncTasks.empty()) {
            task = NULL;
        } else {
            task = process->syncTasks.head();
        }
        processedCount = totalSyncTasks - process->syncTasks.count();
        process->tasksLock.unlock();

        // Break out when completed
        if (task == NULL) {
            break;
        }

        /* Update dialog task title */
        if (dialog != NULL) {
            dialog->setWindowTitle(task->title());
        }

        // Wait until the task is finished processing
        while (!task->isFinished()) {
            currentTime = QDateTime::currentMSecsSinceEpoch();
            if ((currentTime - startTime) < waitTimeout) {
                /* Allows for active UI updates, but is slightly dangerous */
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                continue;
            }

            // No longer waiting, hide wait cursor and show dialog
            if (isWaitCursor) {
                isWaitCursor = false;
                QApplication::restoreOverrideCursor();
                dialog = new ProgressDialog((QWidget*) this->parent());
                dialog->setWindowTitle(task->title());
                dialog->show();
            }

            // Calculate progress value
            double progress = task->progress();
            if (progress < 0.0 && processedCount) {
                progress = 0.0;
            }
            progress /= totalSyncTasks;
            progress += ((double) processedCount / (double) totalSyncTasks);

            // Update progress
            dialog->updateProgress(progress, task->status());

            // Do events; handle cancelling
            QCoreApplication::processEvents();
            if (dialog->isCancelClicked() && !task->isCancelled()) {
                task->cancel();
            }

            if (!process->isRunning) {
                task->setError(ProtocolException("Port is not opened"));
            }
        }

        // Show task errors in a dialog box
        if (task->hasError()) {
            task->showError();
        }
    }
    if (isWaitCursor) {
        QApplication::restoreOverrideCursor();
    }
    if (dialog != NULL) {
        dialog->close();
        delete dialog;
    }
}

bool stk500Serial::isExecuting() {
    return process->isProcessing ||
            !process->asyncTasks.isEmpty() ||
            !process->syncTasks.isEmpty();
}

void stk500Serial::cancelTasks() {
    process->cancelTasks();
}

void stk500Serial::closeSerial() {
    openSerial(0);
}

void stk500Serial::openSerial(int baudrate) {
    // Don't do anything if not open
    if (!isOpen()) {
        return;
    }
    // If already closed; ignore
    if (!baudrate && !this->process->serialBaud) {
        return;
    }
    // Update baud rate and notify the change
    this->process->serialBaud = baudrate;
    this->process->isBaudChanged = true;
}

int stk500Serial::read(char* buff, int len) {
    if (!isSerialOpen()) {
        return 0;
    }

    // Lock the reading buffer and read from it
    this->process->readBuffLock.lock();
    int count = this->process->readBuff.length();
    int remaining = 0;
    if (count > len) {
        remaining = (count - len);
        count = len;
    }
    if (count) {
        char* input_buff = this->process->readBuff.data();

        // Read the contents
        memcpy(buff, input_buff, count);

        // Shift data around
        memcpy(input_buff, input_buff + count, remaining);
        this->process->readBuff.truncate(remaining);
    }
    this->process->readBuffLock.unlock();

    // All done!
    return count;
}

void stk500Serial::write(const char* buff, int len) {
    if (!isSerialOpen()) {
        return;
    }

    // Lock the writing buffer and write to it
    this->process->writeBuffLock.lock();
    this->process->writeBuff.append(buff, len);
    this->process->writeBuffLock.unlock();
}

void stk500Serial::write(const QString &message) {
    QByteArray messageData = message.toLatin1();
    write(messageData.data(), messageData.length());
}

/* Task processing thread */

stk500_ProcessThread::stk500_ProcessThread(stk500Serial *owner, QString portName) {
    this->owner = owner;
    this->closeRequested = false;
    this->isRunning = true;
    this->isProcessing = false;
    this->isBaudChanged = false;
    this->serialBaud = 0;
    this->portName = portName;
    this->status = "";
}

void stk500_ProcessThread::run() {
    /* First check whether the port can be opened at all */
    QSerialPortInfo info(portName);
    if (info.isBusy()) {
        this->closeRequested = true;
        updateStatus("Serial port busy");
    }
    if (!info.isValid()) {
        this->closeRequested = true;
        updateStatus("Serial port invalid");
    }
    if (!this->closeRequested) {
        /* First attempt to open the Serial port */
        stk500Port port;
        stk500 protocol(&port);
        updateStatus("Opening...");

        /* Attempt tp open the serial port */
        if (!port.open(portName)) {
            this->closeRequested = true;
            qDebug() << "Could not open port: " << port.errorString();
        }

        /* Run process loop while not being closed */
        bool needSignOn = true;
        bool hasData = false;
        qint64 start_time = QDateTime::currentMSecsSinceEpoch();
        long currSerialBaud = 0;
        bool wasIdling = true;
        while (!this->closeRequested) {
            if (this->isBaudChanged) {
                this->isBaudChanged = false;
                currSerialBaud = this->serialBaud;

                /* Clear input/output buffers */
                this->readBuffLock.lock();
                this->readBuff.clear();
                this->readBuffLock.unlock();
                this->writeBuffLock.lock();
                this->writeBuff.clear();
                this->writeBuffLock.unlock();

                if (currSerialBaud) {
                    // Changing to a different baud rate
                    start_time = QDateTime::currentMSecsSinceEpoch();

                    // If currently signed on the reset can be skipped
                    try {
                        if (protocol.isSignedOn()) {
                            protocol.signOut();
                        } else {
                            /* Simple hardware reset */
                            port.reset();
                        }
                    } catch (ProtocolException &) {}

                    this->owner->notifySerialOpened(this);
                    port.setBaudRate(currSerialBaud);
                    updateStatus("[Serial] Active");

                } else {
                    // Leaving Serial mode - sign on needed
                    needSignOn = true;
                    port.setBaudRate(115200);
                }
            }

            if (currSerialBaud) {
                /* Serial mode: process serial I/O */

                /* Write out data */
                if (!this->writeBuff.isEmpty()) {
                    this->writeBuffLock.lock();

                    char* buff = this->writeBuff.data();
                    int buff_len = this->writeBuff.length();
                    int written = port.write(buff, buff_len);
                    int remaining = (buff_len - written);
                    if (!remaining) {
                        this->writeBuff.clear();
                    } else if (written == -1) {
                        /* Do something here? Error conditions are unclear */
                        qDebug() << "An error occurred while writing";
                    } else {
                        /* Not sure if overflow is allowed to happen, but it's handled */
                        memcpy(buff, buff + written, remaining);
                        this->writeBuff.truncate(remaining);
                    }

                    this->writeBuffLock.unlock();
                }

                /* Read in data */
                QByteArray serialData = port.readAll(20);

                /* Copy to the read buffer */
                if (!serialData.isEmpty()) {
                    if (!hasData) {
                        hasData = true;
                        qDebug() << "Program started after " << (QDateTime::currentMSecsSinceEpoch() - start_time) << "ms";
                    }
                    this->readBuffLock.lock();
                    this->readBuff.append(serialData);
                    this->readBuffLock.unlock();
                }
            } else {
                /* Command mode: process bootloader I/O */

                /* Poll the next task to execute */
                stk500Task *task;
                bool taskIsSync = false;
                tasksLock.lock();
                if (syncTasks.empty()) {
                    if (asyncTasks.empty()) {
                        task = NULL;
                    } else {
                        task = asyncTasks.head();
                    }
                } else {
                    task = syncTasks.head();
                    taskIsSync = true;
                }
                tasksLock.unlock();

                /* If not signed on and a task is to be handled, sign on first */
                if (task != NULL && task->usesFirmware() && !protocol.isSignedOn()) {
                    needSignOn = true;
                }

                /* Sign on with the bootloader */
                if (needSignOn && !protocol.isServiceMode()) {
                    needSignOn = false;
                    wasIdling = true;
                    updateStatus("[STK500] Signing on");
                    if (trySignOn(&protocol)) {
                        runTests(protocol);
                    } else {
                        updateStatus("[STK500] Sign-on error");
                    }
                }

                if (task != NULL) {
                    /* Update status */
                    if (protocol.isServiceMode()) {
                        updateStatus("[Service] Busy");
                    } else {
                        updateStatus("[STK500] Busy");
                    }

                    /* Process the current task */
                    try {
                        task->init();
                        if (task->usesFirmware() && !protocol.isSignedOn()) {
                            if (protocol.isServiceMode()) {
                                throw ProtocolException("Device is in service mode and can not "
                                                        "handle standard STK commands.\nPlease "
                                                        "install the latest firmware by uploading "
                                                        "the bootloader hex file in the control tab.");
                            } else {
                                throw ProtocolException("Device failed to enter firmware mode: No communication");
                            }
                        }

                        // Before processing, force the Micro-SD to re-read information
                        // Only needed if we were idling before (and user may have switched card)
                        if (wasIdling) {
                            protocol.sd().reset();
                        }

                        // Process the task after setting the protocol
                        if (!task->isCancelled()) {
                            task->setProtocol(&protocol);
                            task->run();
                        }

                        // Flush the data on the Micro-SD now all is well
                        protocol.sd().flushCache();
                    } catch (ProtocolException &ex) {
                        task->setError(ex);

                        /* Flush here as well, but eat up any errors... */
                        try {
                            protocol.sd().flushCache();
                        } catch (ProtocolException&) {
                        }
                    }

                    // If an error occurred, cancel all synchronized tasks
                    if (taskIsSync && task->hasError()) {

                        // Take all old tasks and clear them
                        this->tasksLock.lock();
                        QList<stk500Task*> tasks;
                        tasks.append(syncTasks);
                        syncTasks.clear();
                        this->tasksLock.unlock();

                        // Cancel all tasks following
                        for (int i = 1; i < tasks.count(); i++) {
                            tasks[i]->cancel();
                        }

                        // Finish the tasks
                        for (int i = 0; i < tasks.count(); i++) {
                            owner->notifyTaskFinished(this, tasks[i]);
                        }
                    } else {

                        // Remove task from queue
                        this->tasksLock.lock();
                        QQueue<stk500Task*> &tasks = taskIsSync ? syncTasks : asyncTasks;
                        if (!tasks.empty() && (tasks.head() == task)) {
                            tasks.dequeue();
                        }
                        this->tasksLock.unlock();

                        // Notify we finished (or failed, or cancelled...)
                        owner->notifyTaskFinished(this, task);
                    }

                    // Check if still processing or not
                    this->tasksLock.lock();
                    if (syncTasks.empty() && asyncTasks.empty()) {
                        isProcessing = false;
                    }
                    this->tasksLock.unlock();

                    wasIdling = false;

                } else if (protocol.idleTime() >= STK500_CMD_MIN_INTERVAL) {
                    /* No task and protocol inactive - ping while waiting for tasks to be sent our way */

                    // Waited for the full interval time, ping with a signOn command
                    // Still alive?
                    wasIdling = true;
                    if (protocol.isServiceMode()) {
                        updateStatus("[Service] Idle");
                    } else if (trySignOn(&protocol)) {
                        updateStatus("[STK500] Idle");
                    } else {
                        updateStatus("[STK500] Session lost");
                    }

                    /* Wait for a short time to keep the bootloader in the right mode */
                    sync.lock();
                    cond.wait(&sync, STK500_CMD_MIN_INTERVAL);
                    sync.unlock();
                }
            }

            /* If port is closed, exit the thread */
            if (!port.isOpen()) {
                this->closeRequested = true;
            }
        }

        /* Notify owner that this port is (being) closed */
        updateStatus("Closing...");

        /* Close the serial port; this can hang in some cases */
        port.close();

        /* Notify owner that port has been closed */
        updateStatus("Port closed");
    }

    this->isRunning = false;
    this->isProcessing = false;
    this->owner->notifyClosed(this);
}

bool stk500_ProcessThread::trySignOn(stk500 *protocol) {
    for (int i = 0; i < 2; i++) {
        try {
            protocolName = protocol->signOn();
            return true;
        } catch (ProtocolException& ex) {
            qDebug() << "Failed to sign on: " << ex.what();
            protocol->resetDelayed();
        }
    }
    return false;
}

void stk500_ProcessThread::updateStatus(QString status) {
    if (this->status != status) {
        this->status = status;
        this->owner->notifyStatus(this, this->status);
    }
}

void stk500_ProcessThread::cancelTasks() {
    tasksLock.lock();
    for (int i = 0; i < syncTasks.count(); i++) {
        syncTasks[i]->cancel();
    }
    for (int i = 0; i < asyncTasks.count(); i++) {
        asyncTasks[i]->cancel();
    }
    tasksLock.unlock();
}

void stk500_ProcessThread::wake() {
    cond.wakeAll();
}

void stk500_ProcessThread::runTests(stk500 &protocol) {
    qDebug() << "[STK500]" << protocol.getPort()->portName() << "Protocol:" << protocolName;

    /* SPI Communication test */
    /*
    try {
        char src[] = {1, 3, 7, 32, 65, 75, 23, 125};
        char dst[sizeof(src)];

        protocol.SPI_transfer(src, dst, sizeof(src));

        for (int i = 0; i < sizeof(src); i++) {
            qDebug() << (quint16) ((unsigned char) dst[i]);
        }
    } catch (ProtocolException &ex) {
        qDebug() << ex.what();
    }
    */

    /* Speed test: execute command timed */
    /*
    try {
        char d[512];
        while (true) {
            // Time: 67 ms
            qint64 timestart = QDateTime::currentMSecsSinceEpoch();
            protocol.RAM_read(0, d, sizeof(d));
            qint64 spent = QDateTime::currentMSecsSinceEpoch() - timestart;
            qDebug() << "Time: " << spent;
            msleep(200);
        }
    } catch (ProtocolException &ex) {
        qDebug() << ex.what();
    }
    */

    /* EEPROM Settings reading test */
    /*
    try {
        PHN_Settings settings = protocol.readSettings();
        qDebug() << "Current: " << settings.getCurrent();
        qDebug() << "Sketch size: " << settings.sketch_size;
    } catch (ProtocolException &ex) {
        qDebug() << ex.what();
    }
    */

    /* Analog read test */
    /*
    try {
        for (int i = 0; i < 5; i++) {
            qDebug() << "ADC: " << protocol.ANALOG_read(0);
            QThread::msleep(30);
        }
    } catch (ProtocolException &ex) {
        qDebug() << ex.what();
    }
    */


    /* RAM Debug test: blink pin 13 a few times */
    /*
    try {
        // Set DDRB7 to OUTPUT
        protocol.RAM_writeByte(0x24, 0xFF, 1 << 7);

        // Set PORTB7 alternating
        for (int i = 0; i < 5; i++) {
            QThread::msleep(50);
            protocol.RAM_writeByte(0x25, 0x00, 1 << 7);
            QThread::msleep(50);
            protocol.RAM_writeByte(0x25, 0xFF, 1 << 7);
        }
    } catch (ProtocolException &ex) {
        qDebug() << ex.what();
    }
    */
}
