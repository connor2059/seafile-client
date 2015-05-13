#include "network-mgr.h"
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <algorithm>
#include <QStringRef>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslCipher>
namespace {
QNetworkProxy proxy_;
const char *const kWhitelistCiphers[] = {"ECDHE-RSA-AES256-GCM-SHA384"
                                         "ECDHE-RSA-AES128-GCM-SHA256"
                                         "DHE-RSA-AES256-GCM-SHA384"
                                         "DHE-RSA-AES128-GCM-SHA256"
                                         "ECDHE-RSA-AES256-SHA384"
                                         "ECDHE-RSA-AES128-SHA256"
                                         "ECDHE-RSA-AES256-SHA"
                                         "ECDHE-RSA-AES128-SHA"
                                         "DHE-RSA-AES256-SHA256"
                                         "DHE-RSA-AES128-SHA256"
                                         "DHE-RSA-AES256-SHA"
                                         "DHE-RSA-AES128-SHA"
                                         "ECDHE-RSA-DES-CBC3-SHA"
                                         "EDH-RSA-DES-CBC3-SHA"
                                         "AES256-GCM-SHA384"
                                         "AES128-GCM-SHA256"
                                         "AES256-SHA256"
                                         "AES128-SHA256"
                                         "AES256-SHA"
                                         "AES128-SHA"
                                         "DES-CBC3-SHA"};
// return false if it contains RC4, RSK, CBC, MD5, DES, DSS, EXPORT, NULL
bool isWeakCipher(const QString& cipher_name)
{
    int current_begin = 0;
    int current_end;
    QStringRef name;
    while((current_end = cipher_name.indexOf("-", current_begin)) != -1) {
        name = QStringRef(&cipher_name, current_begin, current_end - current_begin);
        if (name == "RC4")
            return true;
        else if (name == "PSK")
            return true;
        else if (name == "CBC")
            return true;
        else if (name == "MD5")
            return true;
        else if (name == "DES")
            return true;
        else if (name == "DSS")
            return true;
        else if (name == "EXP")
            return true;
        else if (name == "NULL")
            return true;

        current_begin = current_end + 1;
    }
    return false;
}
} // anonymous namespace

NetworkManager* NetworkManager::instance_ = NULL;

NetworkManager::NetworkManager() : should_retry_(true) {
    // remove unsafe cipher
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    const QList<QSslCipher> ciphers = QSslSocket::supportedCiphers();

    QList<QSslCipher> new_ciphers;
    Q_FOREACH(const QSslCipher &cipher, ciphers)
    {
        bool whitelisted = false;
        for (unsigned i = 0; i < sizeof(kWhitelistCiphers); ++i) {
            if (cipher.name() == kWhitelistCiphers[i]) {
                whitelisted = true;
                break;
            }
        }
        if (!whitelisted) {
            // blacklist eNULL, no encryption
            if (cipher.encryptionMethod().isEmpty())
                continue;
            // blacklist aNULL, no authentication
            if (cipher.authenticationMethod().isEmpty())
                continue;
            // blacklist RC4, RSK, CBC, MD5, DES, DSS, EXPORT, NULL
            const QString cipher_name = cipher.name();
            if (isWeakCipher(cipher_name))
                continue;
        }
        new_ciphers.push_back(cipher);
    }
    configuration.setCiphers(new_ciphers);
    QSslConfiguration::setDefaultConfiguration(configuration);
}

void NetworkManager::addWatch(QNetworkAccessManager* manager)
{
    if (std::find(managers_.begin(), managers_.end(), manager) == managers_.end()) {
        connect(manager, SIGNAL(destroyed()), this, SLOT(onCleanup()));
        managers_.push_back(manager);
    }
}

void NetworkManager::applyProxy(const QNetworkProxy& proxy)
{
    proxy_ = proxy;
    should_retry_ = true;
    QNetworkProxy::setApplicationProxy(proxy_);
    for(std::vector<QNetworkAccessManager*>::iterator pos = managers_.begin();
        pos != managers_.end(); ++pos)
        (*pos)->setProxy(proxy_);
    emit proxyChanged(proxy_);
}

void NetworkManager::reapplyProxy()
{
    applyProxy(proxy_);
}

void NetworkManager::onCleanup()
{
    QNetworkAccessManager *manager = qobject_cast<QNetworkAccessManager*>(sender());
    if (manager) {
        managers_.erase(std::remove(managers_.begin(), managers_.end(), manager),
                        managers_.end());
    }
}
