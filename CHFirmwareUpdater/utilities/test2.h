#ifndef TEST2_H
#define TEST2_H

#include <QtCore>


class test2:public QObject
{
    Q_OBJECT

    public:
        test2();
        test2(int a);

//        void send()
//        {
//            emit newPaper("dwasds");
//        }

//    signals:
//        void newPaper(const QString &name);

        void send()
        {
            emit sig_test();
        }

signals:
    void sig_test();


};

#endif // TEST2_H
