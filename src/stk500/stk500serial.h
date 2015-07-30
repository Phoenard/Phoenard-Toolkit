#ifndef STK500SERIAL_H
#define STK500SERIAL_H

#include "stk500.h"
#include "stk500task.h"

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
    void execute(stk500Task &task);
    void executeAll(QList<stk500Task*> tasks);
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

signals:
    void statusChanged(QString status);
    void serialOpened();
    void closed();

protected:
    bool cleanKilledProcesses();

private:
    stk500_ProcessThread *process;
};

// Thread that processes stk500 tasks
class stk500_ProcessThread : public QThread {
   Q_OBJECT

public:
    stk500_ProcessThread(stk500Serial *owner, QString portName);
    void cancelTasks();
    void wake();

protected:
    virtual void run();
    bool trySignOn(stk500 *protocol);
    void updateStatus(QString status);

public:
    stk500Serial *owner;
    QMutex sync;
    QWaitCondition cond;
    stk500Task *currentTask;
    QMutex readBuffLock;
    QMutex writeBuffLock;
    QByteArray readBuff;
    QByteArray writeBuff;
    bool closeRequested;
    bool isRunning;
    bool isProcessing;
    bool isSignedOn;
    bool isBaudChanged;
    int serialBaud;
    QString protocolName;
    QString portName;
    QString status;
};

#endif // stk500Serial_H
