#include <raiselimit.h>
#include <QString>
#include <QFile>
#include <QTextStream>

using namespace KAuth;

/* Make the new inotify limit persist across reboots by creating a file in /etc/sysctl.d
 * Using /etc/sysctl.d is much easier than /etc/sysctl.conf, and should work
 * on all systemd distros, debian (including derivatives such as ubuntu) and gentoo.
 * This could potentially be a separate action, but that would require authenticating twice.
 * Also, the user's file system isn't going away - if they wanted a larger limit once, they
 * almost certainly want it again. */
bool raiselimitPermanently(int newLimit)
{
    QFile sysctl("/etc/sysctl.d/97-kde-nepomuk-filewatch-inotify.conf");
    //Just overwrite the existing file.
    if( sysctl.open(QIODevice::WriteOnly) ){
        QTextStream sysc(&sysctl);
        sysc << "fs.inotify.max_user_watches = " << newLimit << "\n";
        sysctl.close();
        return true;
    }
    return false;
}

ActionReply FileWatchHelper::raiselimit(QVariantMap args)
{
    Q_UNUSED( args );
    // Open the procfs file that controls the number of inotify user watches
    QFile inotctl("/proc/sys/fs/inotify/max_user_watches");
    if( !inotctl.open(QIODevice::ReadWrite) )
        return ActionReply::HelperErrorReply;
    QTextStream inot(&inotctl);
    // Read the current number
    QString curr = inot.readLine();
    bool ok;
    int now = curr.toInt(&ok);
    if( !ok )
        return ActionReply::HelperErrorReply;
    // Write the new number, which is double the current number
    int next = now*2;
    // Check for overflow
    if( next < now )
        return ActionReply::HelperErrorReply;

    inot << next << "\n";
    inotctl.close();
    raiselimitPermanently(next);
    return ActionReply::SuccessReply;
}

KDE4_AUTH_HELPER_MAIN("org.kde.nepomuk.filewatch", FileWatchHelper)
