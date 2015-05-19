#ifndef STK500SESSION_H
#define STK500SESSION_H

#include "stk500.h"
#include "stk500task.h"

class stk500_ProcessThread;

class stk500Session : public QObject
{
    Q_OBJECT

public:
    stk500Session(QWidget *owner);
    bool isOpen();
    void open(QString & portName);
    void close();
    bool abort();
    void execute(QWidget *owner, stk500Task *task);
    void executeAll(QWidget *owner, QList<stk500Task*> tasks);
    void cancelTasks();
    void openSerial(int baudrate);
    void closeSerial();
    int read(char* buff, int buffLen);
    void write(char* buff, int buffLen);

    void notifyClosed(stk500_ProcessThread *sender, bool running);

protected:
    bool cleanKilledProcesses();

private:
    stk500_ProcessThread *process;
    QList<stk500_ProcessThread*> killedProcesses;
};

// Thread that processes stk500 tasks
class stk500_ProcessThread : public QThread {
   Q_OBJECT

public:
    stk500_ProcessThread(stk500Session *owner, QString portName);
    void cancelTasks();
    void wake();
    void clearSerialBuffer();

protected:
    virtual void run();
    bool trySignOn(stk500 *protocol);

public:
    stk500Session *owner;
    QList<stk500Task*> queue;
    QMutex sync;
    QWaitCondition cond;
    stk500Task *currentTask;
    QMutex inputBuffLock;
    QByteArray inputBuff;
    bool closeRequested;
    bool isRunning;
    bool isProcessing;
    bool isReady;
    int serialBaud;
    QString protocolName;
    QString portName;
    QString closeReason;
};

#endif // STK500SESSION_H
