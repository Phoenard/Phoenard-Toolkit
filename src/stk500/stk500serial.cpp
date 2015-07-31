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

void stk500Serial::execute(stk500Task &task) {
    QList<stk500Task*> tasks;
    tasks.append(&task);
    executeAll(tasks);
}

void stk500Serial::executeAll(QList<stk500Task*> tasks) {
    /* No tasks - don't do anything */
    if (tasks.isEmpty()) return;

    /* If not open, fail all tasks right away */
    if (!isOpen()) {
        for (stk500Task *task : tasks) {
            task->setError(ProtocolException("Port is not opened"));
        }
        return;
    }

    ProgressDialog *dialog = NULL;
    qint64 currentTime;
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    bool isWaitCursor = true;
    int totalTaskCount = tasks.count();
    int processedCount = 0;
    qint64 waitTimeout = 1000;
    if (!process->isSignedOn) {
        waitTimeout += 300; // Give 300 MS to open port
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    while (!tasks.isEmpty()) {
        // Obtain next task, check if it's not cancelled
        stk500Task *task = tasks.takeFirst();
        if (task->isCancelled() || task->hasError()) {
            continue;
        }

        // Put into the process thread for processing
        process->sync.lock();
        process->currentTask = task;
        process->sync.unlock();
        process->wake();
        process->isProcessing = true;

        if (dialog != NULL) {
            dialog->setWindowTitle(task->title());
        }

        // Wait until it's done processing
        while (process->isProcessing) {
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
            progress /= totalTaskCount;
            progress += ((double) processedCount / (double) totalTaskCount);

            // Update progress
            dialog->updateProgress(progress, task->status());

            // Do events; handle cancelling
            QCoreApplication::processEvents();
            if (dialog->isCancelClicked() && !task->isCancelled()) {
                task->cancel();
                for (stk500Task *task : tasks) {
                    task->cancel();
                }
            }

            if (!process->isRunning) {
                task->setError(ProtocolException("Port is not opened"));
            }
        }

        // If task has an error, stop other tasks too (errors are dangerous)
        if (task->hasError()) {
            for (stk500Task *task : tasks) {
                task->setError(ProtocolException("Cancelled due to previous error"));
            }
        }
        if (task->hasError()) {
            task->showError();
        }
        processedCount++;
    }
    if (isWaitCursor) {
        QApplication::restoreOverrideCursor();
    }
    if (dialog != NULL) {
        dialog->close();
        delete dialog;
    }
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
    this->isSignedOn = false;
    this->isBaudChanged = false;
    this->serialBaud = 0;
    this->currentTask = NULL;
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
        QSerialPort port;
        stk500 protocol(&port);
        updateStatus("Opening...");

        /* Initialize the port */
        port.setPortName(portName);
        port.setReadBufferSize(4096);
        port.setSettingsRestoredOnClose(false);
        port.open(QIODevice::ReadWrite);
        port.clearError();
        port.setBaudRate(115200);

        /* Run process loop while not being closed */
        bool needSignOn = true;
        bool hasData = false;
        qint64 start_time;
        long currSerialBaud = 0;
        char serialBuffer[1024];
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
                    this->owner->notifySerialOpened(this);
                    if (isSignedOn) {
                        isSignedOn = false;
                        protocol.signOut();
                    } else {
                        protocol.reset();
                    }
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

                /* If no data available, wait for reading to be done shortly */
                if (!port.bytesAvailable()) {
                    port.waitForReadyRead(20);
                }

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
                int cnt = port.read((char*) serialBuffer, sizeof(serialBuffer));

                /* Copy to the read buffer */
                if (cnt) {
                    if (!hasData) {
                        hasData = true;
                        qDebug() << "Program started after " << (QDateTime::currentMSecsSinceEpoch() - start_time) << "ms";
                    }
                    this->readBuffLock.lock();
                    this->readBuff.append(serialBuffer, cnt);
                    this->readBuffLock.unlock();
                }
            } else {
                /* Command mode: process bootloader I/O */
                stk500Task *task = this->currentTask;

                /* If not signed on and a task is to be handled, sign on first */
                if (task != NULL && !isSignedOn) {
                    needSignOn = true;
                }

                /* Sign on with the bootloader */
                if (needSignOn) {
                    needSignOn = false;
                    updateStatus("[STK500] Signing on");
                    isSignedOn = trySignOn(&protocol);
                    if (isSignedOn) {
                        qDebug() << "[STK500] Protocol: " << protocolName;

                        /* Analog read test */
                        try {
                            for (int i = 0; i < 5; i++) {
                                qDebug() << "ADC: " << protocol.ANALOG_read(0);
                                QThread::msleep(30);
                            }
                        } catch (ProtocolException &ex) {
                            qDebug() << ex.what();
                        }


                        /* RAM Debug test: blink pin 13 a few times */
                        try {
                            /* Set DDRB7 to OUTPUT */
                            protocol.RAM_writeByte(0x24, 0xFF, 1 << 7);

                            /* Set PORTB7 alternating */
                            for (int i = 0; i < 5; i++) {
                                QThread::msleep(50);
                                protocol.RAM_writeByte(0x25, 0x00, 1 << 7);
                                QThread::msleep(50);
                                protocol.RAM_writeByte(0x25, 0xFF, 1 << 7);
                            }
                        } catch (ProtocolException &ex) {
                            qDebug() << ex.what();
                        }
                    } else {
                        updateStatus("[STK500] Sign-on error");
                    }
                }

                if (task != NULL) {
                    /* Update status */
                    updateStatus("[STK500] Busy");

                    /* Process the current task */
                    try {
                        if (!isSignedOn) {
                            throw ProtocolException("Could not communicate with device");
                        }
                        // Before processing, force the Micro-SD to re-read information
                        protocol.sd().reset();
                        protocol.sd().discardCache();

                        // Process the task after setting the protocol
                        task->setProtocol(protocol);
                        task->run();

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

                    // Clear the currently executed task
                    currentTask = NULL;
                    isProcessing = false;

                } else if (protocol.idleTime() >= STK500_CMD_MIN_INTERVAL) {
                    /* No task and protocol inactive - ping while waiting for tasks to be sent our way */

                    // Waited for the full interval time, ping with a signOn command
                    // Still alive?
                    if (isSignedOn) {
                        updateStatus("[STK500] Idle");

                        isSignedOn = trySignOn(&protocol);
                        if (!isSignedOn) {
                            updateStatus("[STK500] Session lost");
                        }
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
    stk500Task *curr = currentTask;
    if (curr != NULL) {
        curr->cancel();
    }
    wake();
}

void stk500_ProcessThread::wake() {
    cond.wakeAll();
}
