#include <kauth.h>

using namespace KAuth;

class FileWatchHelper : public QObject
{
    Q_OBJECT

    public slots:
        ActionReply raiselimit(QVariantMap args);
};

