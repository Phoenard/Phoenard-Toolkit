#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Process arguments passed into the application
    QCommandLineParser parser;
    parser.setApplicationDescription("Test helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("source", "Source input file or folder");
    parser.addPositionalArgument("target", "Destination or target output file or folder");

    // Option to perform image conversion (-i)
    QCommandLineOption imageConvertOption("i", "Convert one image format into another");
    parser.addOption(imageConvertOption);

    // Option to read the firmware version of a hex file (--fv)
    QCommandLineOption firmwareVersionOption("fv", "Get firmware version token");
    parser.addOption(firmwareVersionOption);

    // A boolean option with multiple names (--ski)
    QCommandLineOption skiFormat("ski", "Convert image into SKI (headerless 1-bit LCD) format");
    parser.addOption(skiFormat);

    // Process the actual command line arguments given by the user
    parser.process(app);

    // Contains source at 0 and target at 1
    const QStringList args = parser.positionalArguments();

    // Read the options and perform an operation accordingly
    if (parser.isSet(imageConvertOption)) {
        QString source = args.at(0);
        QString dest = args.at(1);
        PHNImage image;

        //TODO: Handle raw input formats here
        image.loadFile(source);

        //TODO: Handle other output image formats here
        if (parser.isSet(skiFormat)) {
            image.setFormat(LCD1);
            image.setHeader(false);
            image.saveFile(dest);
        }
        return 0;
    }

    // Read program data, print out firmware version and quit
    if (parser.isSet(firmwareVersionOption)) {
        QString source = args.at(0);
        ProgramData program;
        program.loadFile(source);
        printf(program.firmwareVersion().toStdString().c_str());
        return 0;
    }

    // Load fonts before GUI launches
    loadFont(":/fonts/OpenSans-Regular.ttf");
    loadFont(":/fonts/Inconsolata-Regular.ttf");

    // Run main application
    MainWindow w;
    w.show();

    return app.exec();
}

void loadFont(const QString& fontPath) {
    const bool log_font_names = false;
    int id = QFontDatabase::addApplicationFont(fontPath);
    if (log_font_names) {
        QStringList families = QFontDatabase::applicationFontFamilies(id);
        qDebug() << "Families of font:" << fontPath;
        for (int i = 0; i < families.count(); i++) {
            qDebug() << " " << families[i];
        }
    }
}
