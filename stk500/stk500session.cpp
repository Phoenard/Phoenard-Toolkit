#include "stk500session.h"
#include <QDebug>
#include <QApplication>
#include <QSerialPortInfo>
#include "progressdialog.h"

stk500Session::stk500Session(QWidget *owner)
    : QObject(owner) {
    process = NULL;
}

bool stk500Session::isOpen() {
    return (process != NULL && !process->closeRequested);
}

bool stk500Session::isSerialOpen() {
    return isOpen() && process->serialBaud;
}

void stk500Session::open(QString & portName) {
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

void stk500Session::close() {
    if (isOpen()) {
        // Tell the process to quit if running
        if (process->isRunning) {
            process->closeRequested = true;
            process->cancelTasks();

            // Add process to the processes being killed
            killedProcesses.append(process);
            process = NULL;
        } else {
            delete process;
            process = NULL;
        }
    }
}

bool stk500Session::abort() {
    close();

    // Wait for a maximum of 1 second until all processes are killed
    for (int i = 0; i < 20 && !cleanKilledProcesses(); i++) {
        QThread::msleep(50);
    }

    return cleanKilledProcesses();
}

bool stk500Session::cleanKilledProcesses() {
    if (killedProcesses.isEmpty()) {
        return true;
    }

    int i = 0;
    bool isAllKilled = true;
    while (i < killedProcesses.length()) {
        stk500_ProcessThread *process = killedProcesses.at(i);
        if (process->isRunning) {
            isAllKilled = false;
            i++;
        } else {
            delete process;
            killedProcesses.removeAt(i);
        }
    }
    return isAllKilled;
}

void stk500Session::notifyStatus(stk500_ProcessThread*, QString status) {
    emit statusChanged(status);
}

void stk500Session::notifyClosed(stk500_ProcessThread *) {
    emit closed();
}

void stk500Session::notifySerialOpened(stk500_ProcessThread *) {
    emit serialOpened();
}

void stk500Session::execute(stk500Task *task) {
    QList<stk500Task*> tasks;
    tasks.append(task);
    executeAll(tasks);
}

void stk500Session::executeAll(QList<stk500Task*> tasks) {
    /* No tasks - don't do anything */
    if (tasks.isEmpty()) return;

    /* If not open, fail all tasks right away */
    if (!isOpen()) {
        for (stk500Task *task : tasks) {
            task->setError(ProtocolException("Port is not opened"));
        }
        return;
    }

    /* Show just a wait cursor for processing < 1000ms */
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (int i = 0; i < 20 && process->isProcessing; i++) {
        QThread::msleep(50);
    }
    QApplication::restoreOverrideCursor();

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

void stk500Session::cancelTasks() {
    process->cancelTasks();
}

void stk500Session::closeSerial() {
    openSerial(0);
}

void stk500Session::openSerial(int baudrate) {
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

int stk500Session::read(char* buff, int len) {
    stk500_ProcessThread *thread = this->process;
    if (thread == NULL) {
        return 0;
    }

    // Lock the reading buffer and read from it
    thread->inputBuffLock.lock();
    int count = thread->inputBuff.length();
    int remaining = 0;
    if (count > len) {
        remaining = (count - len);
        count = len;
    }
    if (count) {
        char* input_buff = thread->inputBuff.data();

        // Read the contents
        memcpy(buff, input_buff, count);

        // Shift data around
        memcpy(input_buff, input_buff + count, remaining);
        thread->inputBuff.truncate(remaining);
    }
    thread->inputBuffLock.unlock();

    // All done!
    return count;
}


/* Task processing thread */

stk500_ProcessThread::stk500_ProcessThread(stk500Session *owner, QString portName) {
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
        port.setReadBufferSize(1024);
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
                this->inputBuffLock.lock();
                this->inputBuff.clear();
                this->inputBuffLock.unlock();

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

                /* Read in data */
                int cnt = port.read((char*) serialBuffer, sizeof(serialBuffer));

                /* Copy to the read buffer */
                if (cnt) {
                    if (!hasData) {
                        hasData = true;
                        qDebug() << "Program started after " << (QDateTime::currentMSecsSinceEpoch() - start_time) << "ms";
                    }
                    this->inputBuffLock.lock();
                    this->inputBuff.append(serialBuffer, cnt);
                    this->inputBuffLock.unlock();
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
                    if (!isSignedOn) {
                        updateStatus("[STK500] Sign-on error");
                    }
                }

                if (task == NULL) {
                    /* No task - wait for tasks to be sent our way */
                    this->isProcessing = false;

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
                } else {
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
