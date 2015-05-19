#include "stk500session.h"
#include <QDebug>
#include <QApplication>
#include <QSerialPortInfo>
#include "progressdialog.h"

stk500Session::stk500Session() {
    process = NULL;
}

bool stk500Session::isOpen() {
    return (process != NULL);
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
            process->closeReason = "Requested by program";
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

void stk500Session::notifyClosed(stk500_ProcessThread *sender, bool running) {
    if (running) {
        qDebug() << "THREAD IS BEING CLOSED...: " << sender->closeReason;
    } else {
        qDebug() << "THREAD WAS CLOSED";
    }
}

void stk500Session::execute(QWidget *owner, stk500Task *task) {
    QList<stk500Task*> tasks;
    tasks.append(task);
    executeAll(owner, tasks);
}

void stk500Session::executeAll(QWidget *owner, QList<stk500Task*> tasks) {
    /* No tasks - don't do anything */
    if (tasks.isEmpty()) return;

    /* Start process thread as needed */
    if (!process->isRunning) {
        process->start();
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
    if (!process->isReady) {
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
                QThread::msleep(50);
                continue;
            }

            // No longer waiting, hide wait cursor and show dialog
            if (isWaitCursor) {
                isWaitCursor = false;
                QApplication::restoreOverrideCursor();
                dialog = new ProgressDialog(owner);
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
                task->setError(ProtocolException("Port is not opened."));
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
    this->closeReason = "Unknown";
    this->isRunning = true;
    this->isProcessing = false;
    this->isReady = false;
    this->currentTask = NULL;
    this->portName = portName;
}

void stk500_ProcessThread::run() {
    /* First check whether the port can be opened at all */
    QSerialPortInfo info(portName);
    if (info.isBusy()) {
        this->closeRequested = true;
        this->closeReason = "Serial port is busy";
    }
    if (!info.isValid()) {
        this->closeRequested = true;
        this->closeReason = "Serial port is invalid";
    }

    if (this->closeRequested) {
        /* Didn't even get to open the port... oh well */
        this->owner->notifyClosed(this, true);

    } else {
        /* First attempt to open the Serial port */
        QSerialPort port;
        stk500 protocol(&port);

        /* Initialize the port */
        port.setPortName(portName);
        port.setBaudRate(115200);
        port.setReadBufferSize(1024);
        port.setSettingsRestoredOnClose(false);
        port.open(QIODevice::ReadWrite);
        port.clearError();

        //TMP: Read stuff here...

        char read_buffer[1024];
        while (!this->closeRequested) {
            /* If no data available, wait for reading to be done shortly */
            if (!port.bytesAvailable()) {
                port.waitForReadyRead(20);
            }

            /* Read in data */
            int cnt = port.read((char*) read_buffer, sizeof(read_buffer));

            /* Copy to the read buffer */
            this->inputBuffLock.lock();
            this->inputBuff.append(read_buffer, cnt);
            this->inputBuffLock.unlock();
        }

        if (this->closeRequested) {
            /* Notify owner that this port is (being) closed */
            this->owner->notifyClosed(this, true);

            /* Close the serial port; this can hang in some cases */
            port.close();

            this->isRunning = false;
            this->isProcessing = false;
            this->owner->notifyClosed(this, false);
            return;
        }

        /* Reset the port for first use */
        protocol.reset();

        /* Sign on with the bootloader */
        if (trySignOn(&protocol)) {
            isReady = true;
            qDebug() << "STK500 Opened: " << protocolName;
        } else {
            /* Failure to initiate protocol - close port */
            this->closeRequested = true;
            this->closeReason = "STK500 Protocol Failed";
        }

        /* Run process loop while not being closed */
        while (!this->closeRequested) {

            // Routinely process commands, wait for commands to be passed

            sync.lock();
            if (currentTask == NULL) {
                this->isProcessing = false;

                if (port.isOpen()) {
                    // Waited for the full interval time, ping with a signOn command
                    // Still alive?
                    if (!trySignOn(&protocol)) {
                        this->closeRequested = true;
                        this->closeReason = "Connection lost";
                    }

                    /* Wait for a short time to keep the bootloader in the right mode */
                    cond.wait(&sync, STK500_CMD_MIN_INTERVAL);
                } else {
                    /* Wait for stuff to happen, infinitely long */
                    cond.wait(&sync);
                }
                sync.unlock();
            } else {
                // Take next task from the queue and process it
                sync.unlock();
                try {
                    if (!port.isOpen()) {
                        throw ProtocolException("Port is closed.");
                    }
                    // Before processing, force the Micro-SD to re-read information
                    protocol.sd().reset();
                    protocol.sd().discardCache();

                    // Process the task after setting the protocol
                    currentTask->setProtocol(protocol);
                    currentTask->run();

                    // Flush the data on the Micro-SD now all is well
                    protocol.sd().flushCache();
                } catch (ProtocolException &ex) {
                    currentTask->setError(ex);

                    /* Flush here as well, but eat up any errors... */
                    try {
                        protocol.sd().flushCache();
                    } catch (ProtocolException&) {
                    }
                }
                currentTask = NULL;

            }
        }

        /* Notify owner that this port is (being) closed */
        this->owner->notifyClosed(this, true);

        /* Close the serial port; this can hang in some cases */
        port.close();
    }

    this->isRunning = false;
    this->isProcessing = false;
    this->owner->notifyClosed(this, false);
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
