#include "mainwindow.h"
#include "trodesinterface.h"
#include <QApplication>


#include <vector>
#include <string>
#include <iostream>

int main(int argc, char *argv[])
{

    std::string server_address = "tcp://127.0.0.1";
    int server_port = 10000;
    std::string config_filename;

    std::vector<std::string> all_args;

    if (argc > 1) {
        all_args.assign(argv + 1, argv + argc);
        std::vector<std::string>::iterator iter = all_args.begin();

        while (iter != all_args.end()) {
            if ((iter->compare("-serverAddress")==0) && (iter != all_args.end())) {
                iter++;
                server_address = *iter;
            } else if ((iter->compare("-serverPort")==0) && (iter != all_args.end())) {
                iter++;
                auto server_port_str = *iter;
                try {
                    server_port = std::stoi(server_port_str);
                    
                }
                catch (std::invalid_argument const& ex)
                {
                    std::cerr << "Invalid server port number." << std::endl;
                }

            } else if ((iter->compare("-trodesConfig")==0) && (iter != all_args.end())) {
                iter++;
                config_filename = *iter;
            }
            // else if ((arguments.at(optionInd).compare("-trodesConfig",Qt::CaseInsensitive)==0) && (arguments.length() > optionInd+1)) {
            iter++;
        }
        
    }

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    TrodesInterface trodesInterface(&w, server_address, server_port);

    return a.exec();
}
