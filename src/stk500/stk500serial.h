#ifndef STK500SERIAL_H
#define STK500SERIAL_H

#include "stk500.h"
#include "stk500task.h"
#include <QQueue>

class stk500_ProcessThread;

class stk500Serial : public QObject
{
    Q_OBJECT

    /* Process thread needs access to notify his parent */
    friend class stk500_ProcessThread;

public:
    stk500Serial(QWidget *owner);
    bool isOpen();
    void open(QString & portName);
    void close();
    void execute(stk500Task &task, bool asynchronous = false);
    void executeAll(QList<stk500Task*> tasks, bool asynchronous = false);
    void cancelTasks();
    void openSerial(int baudrate);
    void closeSerial();
    bool isSerialOpen();
    int read(char* buff, int buffLen);
    void write(const char* buff, int buffLen);
    void write(const QString &message);

protected:
    void notifyStatus(stk500_ProcessThread *sender, QString status);
    void notifyClosed(stk500_ProcessThread *sender);
    void notifySerialOpened(stk500_ProcessThread *sender);
    void notifyTaskFinished(stk500_ProcessThread *sender, stk500Task *task);

signals:
    void statusChanged(QString status);
    void serialOpened();
    void closed();
    void taskFinished(stk500Task *task);

protected:
    bool cleanKilledProcesses();

private:
    stk500_ProcessThread *process;
};

// Thread that processes stk500 tasks
class stk500_ProcessThread : public QThread {

public:
    stk500_ProcessThread(stk500Serial *owner, QString portName);
    void cancelTasks();
    void wake();
    stk500Task *currentTask();

protected:
    virtual void run();
    bool trySignOn(stk500 *protocol);
    void updateStatus(QString status);

public:
    stk500Serial *owner;
    QMutex sync;
    QWaitCondition cond;
    QQueue<stk500Task*> tasks;
    QMutex tasksLock;
    QMutex readBuffLock;
    QMutex writeBuffLock;
    QByteArray readBuff;
    QByteArray writeBuff;
    bool closeRequested;
    bool isRunning;
    bool isProcessing;
    bool isBaudChanged;
    int serialBaud;
    QString protocolName;
    QString portName;
    QString status;
};

#endif // stk500Serial_H
