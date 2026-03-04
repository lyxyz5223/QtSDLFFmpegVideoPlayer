#include <Logger.h>

#define DefineLoggerSinks(variableName, fileLoggerNameWithoutSuffix) \
const std::vector<LoggerSinkPtr> variableName{ \
    std::make_shared<ConsoleLoggerSink>(), \
    std::make_shared<FileLoggerSink>(std::string(".\\logs\\") + fileLoggerNameWithoutSuffix + std::string(".class.log")), \
    /*End*/}
/////
