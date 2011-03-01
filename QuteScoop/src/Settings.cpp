/**************************************************************************
 *  This file is part of QuteScoop. See README for license
 **************************************************************************/

#include "Settings.h"

#include "Whazzup.h"
#include "Window.h"

QSettings *settings_instance = 0;

QSettings* Settings::getSettings() {
    if(settings_instance == 0) {
        settings_instance = new QSettings();
        qDebug() << "Expecting settings at" << settings_instance->fileName();
    }
    return settings_instance;
}

// .ini File Functions
void Settings::exportToFile(QString fileName) {
    QSettings* settings_file = new QSettings(fileName, QSettings::IniFormat);
    for (int i = 0; i < getSettings()->allKeys().length(); i++) {
        settings_file->setValue(getSettings()->allKeys()[i], getSettings()->value(getSettings()->allKeys()[i]));
    }
    delete settings_file;
}

void Settings::importFromFile(QString fileName) {
    QSettings* settings_file = new QSettings(fileName, QSettings::IniFormat);
    for (int i = 0; i < settings_file->allKeys().length(); i++) {
        getSettings()->setValue(settings_file->allKeys()[i], settings_file->value(settings_file->allKeys()[i]));
    }
    delete settings_file;
}


// returns NotOpen / ReadOnly / ReadWrite
QIODevice::OpenMode Settings::testDirectory(QString &dir) {
    QIODevice::OpenMode capabilities = QIODevice::NotOpen;
    if (QDir(dir).exists()) {
        QFile testFile(dir + "/test");
        if (testFile.open(QIODevice::ReadWrite)) {
            capabilities |= QIODevice::ReadWrite;
            testFile.close();
            testFile.remove();
        } else {
            capabilities |= QIODevice::ReadOnly;
        }
    }
    //qDebug() << "Settings::testDirectory()" << dir << capabilities;
    return capabilities;
}

// priority:
// 1) DataLocation ('dirs.first()') has subdirs and is writeable
    // on Mac: /Users/<user>/Library/Application Support/QuteScoop/QuteScoop
    // on Ubuntu: /home/<user>/.local/share/data/QuteScoop/QuteScoop
    // on WinXP 32: C:\Dokumente und Einstellungen\<user>\Lokale Einstellungen\Anwendungsdaten\QuteScoop\QuteScoop
    // on Win7 64: \Users\<user>\AppData\local\QuteScoop\QuteScoop
// 2) other location in 'dirs' has subdirs and is writeable
// 3) any location has subdirs
// 3a) if DataLocation is writeable: copy data there
// 4) if all that fails: fall back to executable-directory
void Settings::calculateApplicationDataDirectory() {
    QStringList dirs; // possible locations, 1st preferred
    dirs << QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    dirs << QCoreApplication::applicationDirPath();

    QStringList subdirs; // needed subDirs
    subdirs << "data" << "downloaded" << "screenshots" << "textures"; // 'texture' does not have to be writeable

    QList< QIODevice::OpenMode> dirCapabilities;
    for (int i = 0; i < dirs.size(); i++) {
        dirCapabilities << QIODevice::ReadWrite; // init, might be diminished by the AND-combine
        // looking for capabilities of the subdirs
        for (int j = 0; j < subdirs.size(); j++) {
            QString dir = QString("%1/%2").arg(dirs[i], subdirs[j]);
            dirCapabilities[i] &= testDirectory(dir); // AND-combine: returns lowest capability
        }
        //qDebug() << "Settings::calculateApplicationsDirectory():" << dirs[i] << "has capabilities:" << dirCapabilities[i];
        if (i == 0) { // preferred location writeable - no further need to look
            if (dirCapabilities[0].testFlag(QIODevice::ReadWrite)) {
                getSettings()->setValue("general/calculatedApplicationDataDirectory", dirs[0]);
                return;
            }
        } else { // another location is writeable - also OK.
            if (dirCapabilities[i].testFlag(QIODevice::ReadWrite)) {
                getSettings()->setValue("general/calculatedApplicationDataDirectory", dirs[i]);
                return;
            }
        }
    }

    // last ressort: looking for readable subdirs
    for (int i = 0; i < dirs.size(); i++) {
        if (dirCapabilities[i].testFlag(QIODevice::ReadOnly)) { // we found the data at least readonly
            QString warningStr(QString(
                    "The directories '%1' where found at '%2' but are readonly. This means that neither automatic sectorfile-download "
                    "nor saving logs, screenshots or downloaded Whazzups will work.\n"
                    "Preferrably, data should be at '%3' and this location should be writable.")
                                 .arg(subdirs.join("', '"))
                                 .arg(dirs[i])
                                 .arg(dirs.first()));
            qWarning() << warningStr;
            QMessageBox::warning(0, "Warning", warningStr);

            // data found in other location than the preferred one and
            // preferred location creatable or already existing: Copy data directories/files there?
            if ((i != 0) && (QDir(dirs.first()).exists() || QDir().mkpath(dirs.first()))) {
                QString questionStr(QString(
                        "The preferred data directory '%1' exists or could be created.\n"
                        "Do you want QuteScoop to install its data files there [recommended]?")
                                     .arg(dirs.first()));
                qDebug() << questionStr;
                if (QMessageBox::question(0, "Install data files?", questionStr, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                    == QMessageBox::Yes) {
                    QString copyErrors;
                    for (int j = 0; j < subdirs.size(); j++) {
                        QString sourceDir = QString("%1/%2").arg(dirs[i], subdirs[j]);
                        QString destDir = QString("%1/%2").arg(dirs.first(), subdirs[j]);

                        // subdirectory exists or could be created
                        if (QDir(destDir).exists()
                            || QDir(dirs.first()).mkpath(subdirs[j])) {
                            QStringList fileNames = QDir(sourceDir)
                                                    .entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::NoSymLinks | QDir::Readable);
                            QMessageBox::information(0, "Copying", QString("Now copying files\n'%1'\n to directory '%2'")
                                                     .arg(fileNames.join("',\n'"), destDir));
                            foreach(const QString &fileName, fileNames) {
                                QString sourceFilePath = QString("%1/%2").arg(sourceDir, fileName);
                                QString destFilePath = QString("%1/%2").arg(destDir, fileName);
                                if (QFile::exists(destFilePath)) // remove if file is already existing
                                    if (!QFile::remove(destFilePath))
                                        QMessageBox::critical(0, "Error", QString(
                                                "Error removing existing file '%1'. Please consider removing it by hand.")
                                                              .arg(destFilePath));
                                if (QFile::copy(sourceFilePath, destFilePath)) {
                                    qDebug() << "copied" << sourceFilePath << "to" << destFilePath;
                                } else {
                                    copyErrors.append(QString("Error copying file '%1' to '%2'\n").arg(sourceFilePath, destFilePath));
                                    qDebug() << "Error copying file" << sourceFilePath << "to" << destFilePath;
                                }
                            }
                        } else { // error creating subdirectory
                            copyErrors.append(QString("Error creating directory '%1'\n").arg(destDir));
                            qDebug() << QString("Error creating %1").arg(destDir);
                        }
                    }
                    if (copyErrors.isEmpty()) {
                        QMessageBox::information(0, "Success",
                                                 QString("Data files installed. QuteScoop will now use '%1' as data directory.")
                                                 .arg(dirs.first()));
                        qDebug() << "Datafiles installed to" << dirs.first();
                        getSettings()->setValue("general/calculatedApplicationDataDirectory", dirs.first());
                        return;
                    } else { // errors during the copy operations
                        QMessageBox::critical(0, "Error", QString("The following errors occurred during copy:\n %1\n"
                                                                  "When in doubt if important files where left out, "
                                                                  "delete the new data directory and let "
                                                                  "QuteScoop copy the files again.")
                                              .arg(copyErrors));
                    }
                }
            }
            getSettings()->setValue("general/calculatedApplicationDataDirectory", dirs[i]);
            return;
        }
    }

    QString criticalStr = QString("No complete data directory, neither read- nor writable, was found. QuteScoop "
                                  "might be behaving unexpectedly.\n"
                                  "Preferrably, '%1' should have the subdirectories '%2' and "
                                  "these locations should be writable.\n"
                                  "QuteScoop will look for the data files in the following locations, too:\n"
                                  "'%3'")
           .arg(dirs.first(), subdirs.join("', '"), QStringList(dirs.mid(1, -1)).join("',\n'"));
    QMessageBox::critical(0, "Critical", criticalStr);
    qCritical() << criticalStr;
    getSettings()->setValue("general/calculatedApplicationDataDirectory", QCoreApplication::applicationDirPath());
    return;
}

QString Settings::applicationDataDirectory(const QString &composeFilePath) {
    return QString("%1/%2")
            .arg(getSettings()->value("general/calculatedApplicationDataDirectory", QVariant()).toString())
            .arg(composeFilePath);
}

//

bool Settings::shootScreenshots() {
    return getSettings()->value("screenshots/shootScreenshots", false).toBool();
}

void Settings::setShootScreenshots(bool value) {
    getSettings()->setValue("screenshots/shootScreenshots", value);
}

int Settings::downloadInterval() {
    return getSettings()->value("download/interval", 2).toInt();
}

void Settings::setDownloadInterval(int value) {
    getSettings()->setValue("download/interval", value);
}

bool Settings::downloadOnStartup() {
    return getSettings()->value("download/downloadOnStartup", true).toBool();
}

void Settings::setDownloadOnStartup(bool value) {
    getSettings()->setValue("download/downloadOnStartup", value);
}

bool Settings::downloadPeriodically() {
    return getSettings()->value("download/downloadPeriodically", true).toBool();
}

void Settings::setDownloadPeriodically(bool value) {
    getSettings()->setValue("download/downloadPeriodically", value);
}

bool Settings::downloadBookings() {
    return getSettings()->value("download/downloadBookings", downloadNetwork() == 1).toBool();
}

void Settings::setDownloadBookings(bool value) {
    getSettings()->setValue("download/downloadBookings", value);
}

QString Settings::bookingsLocation() {
    return getSettings()->value("download/bookingsLocation", "http://vatbook.euroutepro.com/servinfo.asp").toString();
}

void Settings::setBookingsLocation(const QString& value) {
    getSettings()->setValue("download/bookingsLocation", value);
}

bool Settings::bookingsPeriodically() {
    return getSettings()->value("download/bookingsPeriodically", true).toBool();
}

void Settings::setBookingsPeriodically(bool value) {
    getSettings()->setValue("download/bookingsPeriodically", value);
}

int Settings::bookingsInterval() {
    return getSettings()->value("download/bookingsInterval", 30).toInt();
}

void Settings::setBookingsInterval(int value) {
    getSettings()->setValue("download/bookingsInterval", value);
}

//
bool Settings::useSupFile() {
    return getSettings()->value("data/useSupFile", true).toBool();
}

void Settings::setUseSupFile(bool value) {
    getSettings()->setValue("data/useSupFile", value);
}

int Settings::downloadNetwork() {
    return getSettings()->value("download/network", 1).toInt();
}

void Settings::setDownloadNetwork(int i) {
    getSettings()->setValue("download/network", i);
}

QString Settings::downloadNetworkName() {
    switch(downloadNetwork()) {
    case 0: return "IVAO"; break;
    case 1: return "VATSIM"; break;
    case 2: return "User Network"; break;
    }
    return "Unknown";
}

QString Settings::userDownloadLocation() {
    return getSettings()->value("download/userLocation", "http://www.network.org/status.txt").toString();
}

void Settings::setUserDownloadLocation(const QString& location) {
    getSettings()->setValue("download/userLocation", location);
}

bool Settings::checkForUpdates() {
    return getSettings()->value("download/checkForUpdates", true).toBool();
}

void Settings::setCheckForUpdates(bool value) {
    getSettings()->setValue("download/checkForUpdates", value);
}

bool Settings::sendVersionInformation() {
    return getSettings()->value("download/sendVersionInfo", true).toBool();
}

void Settings::setSendVersionInformation(bool value) {
    getSettings()->setValue("download/sendVersionInfo", value);
}

QString Settings::updateVersionNumber() {
    return getSettings()->value("download/updateVersionNumber", "-1").toString();
}

void Settings::setUpdateVersionNumber(const QString& version) {
    getSettings()->setValue("download/updateVersionNumber", version);
}

QString Settings::statusLocation() {
    return getSettings()->value("download/statusLocation", "http://status.vatsim.net/").toString();
}

void Settings::setStatusLocation(const QString& location) {
    getSettings()->setValue("download/statusLocation", location);
    Whazzup::getInstance()->setStatusLocation(location);
}

bool Settings::useProxy() {
    return getSettings()->value("proxy/useProxy", false).toBool();
}

void Settings::setUseProxy(bool value) {
    getSettings()->setValue("proxy/useProxy", value);
}

QString Settings::proxyServer() {
    return getSettings()->value("proxy/server").toString();
}

void Settings::setProxyServer(const QString& server) {
    getSettings()->setValue("proxy/server", server);
}

int Settings::proxyPort() {
    return getSettings()->value("proxy/port", 8080).toInt();
}

void Settings::setProxyPort(int value) {
    getSettings()->setValue("proxy/port", value);
}

QString Settings::proxyUser() {
    return getSettings()->value("proxy/user").toString();
}

void Settings::setProxyUser(QString user) {
    getSettings()->setValue("proxy/user", user);
}

QString Settings::proxyPassword() {
    return getSettings()->value("proxy/password").toString();
}

void Settings::setProxyPassword(QString password) {
    getSettings()->setValue("proxy/password", password);
}


void Settings::applyProxySetting(QHttp *http) {
    if(!useProxy() || http == 0)
        return;

    QString user = Settings::proxyUser();
    QString pass = Settings::proxyPassword();
    if(user.isEmpty()) user = QString();
    if(pass.isEmpty()) pass = QString();

    if(!proxyServer().isEmpty())
        http->setProxy(proxyServer(), proxyPort(), user, pass);
}

int Settings::timelineSeconds() {
    return getSettings()->value("display/timelineSeconds", 120).toInt();
}

void Settings::setTimelineSeconds(int value) {
    getSettings()->setValue("display/timelineSeconds", value);
}

QString Settings::navdataDirectory() {
    return getSettings()->value("database/path").toString();
}

void Settings::setNavdataDirectory(const QString& directory) {
    getSettings()->setValue("database/path", directory);
}

bool Settings::useNavdata() {
    return getSettings()->value("database/use", false).toBool();
}

void Settings::setUseNavdata(bool value) {
    getSettings()->setValue("database/use", value);
}

bool Settings::showFixes() {
    return getSettings()->value("database/showfixes", false).toBool();
}

void Settings::setShowFixes(bool value) {
    getSettings()->setValue("database/showfixes", value);
}

int Settings::metarDownloadInterval() {
    return getSettings()->value("display/metarInterval", 10).toInt();
}

void Settings::setMetarDownloadInterval(int minutes) {
    getSettings()->setValue("download/metarInterval", minutes);
}

// OpenGL
bool Settings::displaySmoothLines() {
    return getSettings()->value("gl/smoothLines", true).toBool();
}

void Settings::setDisplaySmoothLines(bool value) {
    getSettings()->setValue("gl/smoothLines", value);
}

bool Settings::glStippleLines() {
    return getSettings()->value("gl/stippleLines", false).toBool();
}

void Settings::setGlStippleLines(bool value) {
    getSettings()->setValue("gl/stippleLines", value);
}

bool Settings::displaySmoothDots() {
    return getSettings()->value("gl/smoothDots", true).toBool();
}

void Settings::setDisplaySmoothDots(bool value) {
    getSettings()->setValue("gl/smoothDots", value);
}

int Settings::maxLabels() {
    return getSettings()->value("gl/maxLabels", 150).toInt();
}

void Settings::setMaxLabels(int maxLabels) {
    getSettings()->setValue("gl/maxLabels", maxLabels);
}

bool Settings::glBlending() {
    return getSettings()->value("gl/blend", true).toBool();
}
void Settings::setGlBlending(bool value) {
    getSettings()->setValue("gl/blend", value);
}

int Settings::glCirclePointEach() {
    return getSettings()->value("gl/circlePointEach", 3).toInt();
}
void Settings::setGlCirclePointEach(int value) {
    getSettings()->setValue("gl/circlePointEach", value);
}

bool Settings::glLighting() {
    return getSettings()->value("gl/lighting", true).toBool();
}
void Settings::setEnableLighting(bool value) {
    getSettings()->setValue("gl/lighting", value);
}

int Settings::glLights() {
    return getSettings()->value("gl/lights", 6).toInt();
}
void Settings::setGlLights(int value) {
    getSettings()->setValue("gl/lights", value);
}

int Settings::glLightsSpread() {
    return getSettings()->value("gl/lightsSpread", 25).toInt();
}
void Settings::setGlLightsSpread(int value) {
    getSettings()->setValue("gl/lightsSpread", value);
}

bool Settings::glTextures() {
    return getSettings()->value("gl/earthTexture", true).toBool();
}
void Settings::setGlTextures(bool value) {
    getSettings()->setValue("gl/earthTexture", value);
}
QString Settings::glTextureEarth() {
    return getSettings()->value("gl/textureEarth", "earth2048x1024").toString();
}
void Settings::setGlTextureEarth(QString value) {
    getSettings()->setValue("gl/textureEarth", value);
}

QColor Settings::sunLightColor() {
    return getSettings()->value("gl/sunLightColor", QColor::fromRgb(255, 249, 201)).value<QColor>();
}

void Settings::setSunLightColor(const QColor& color) {
    getSettings()->setValue("gl/sunLightColor", color);
}

QColor Settings::specularColor() {
    return getSettings()->value("gl/sunSpecularColor", QColor::fromRgb(50, 22, 3)).value<QColor>();
}

void Settings::setSpecularColor(const QColor& color) {
    getSettings()->setValue("gl/sunSpecularColor", color);
}

double Settings::earthShininess() {
    return getSettings()->value("gl/earthShininess", 25).toDouble();
}

void Settings::setEarthShininess(double strength) {
    getSettings()->setValue("gl/earthShininess", strength);
}


// Stylesheet
QString Settings::stylesheet() {
    return getSettings()->value("display/stylesheet", QString()).toString();
}

void Settings::setStylesheet(const QString& value) {
    getSettings()->setValue("display/stylesheet", value);
}

// earthspace
int Settings::earthGridEach() {
    return getSettings()->value("earthSpace/earthGridEach", 30).toInt();
}
void Settings::setEarthGridEach(int value) {
    getSettings()->setValue("earthSpace/earthGridEach", value);
}

QColor Settings::backgroundColor() {
    return getSettings()->value("earthSpace/backgroundColor", QColor::fromRgbF(0, 0, 0)).value<QColor>();
}

void Settings::setBackgroundColor(const QColor& color) {
    getSettings()->setValue("earthSpace/backgroundColor", color);
}

QColor Settings::globeColor() {
    return getSettings()->value("earthSpace/globeColor", QColor::fromRgb(130, 160, 190)).value<QColor>();
}

void Settings::setGlobeColor(const QColor& color) {
    getSettings()->setValue("earthSpace/globeColor", color);
}

QColor Settings::gridLineColor() {
    return getSettings()->value("earthSpace/gridLineColor", QColor::fromRgb(100, 100, 100, 80)).value<QColor>();
}

void Settings::setGridLineColor(const QColor& color) {
    getSettings()->setValue("earthSpace/gridLineColor", color);
}

double Settings::gridLineStrength() {
    return getSettings()->value("earthSpace/gridLineStrength", 0.5).toDouble();
}

void Settings::setGridLineStrength(double strength) {
    getSettings()->setValue("earthSpace/gridLineStrength", strength);
}

QColor Settings::countryLineColor() {
    return getSettings()->value("earthSpace/countryLineColor", QColor::fromRgb(170, 255, 127, 50)).value<QColor>();
}

void Settings::setCountryLineColor(const QColor& color) {
    getSettings()->setValue("earthSpace/countryLineColor", color);
}

double Settings::countryLineStrength() {
    return getSettings()->value("earthSpace/countryLineStrength", 0.3).toDouble();
}

void Settings::setCountryLineStrength(double strength) {
    getSettings()->setValue("earthSpace/countryLineStrength", strength);
}

QColor Settings::coastLineColor() {
    return getSettings()->value("earthSpace/coastLineColor", QColor::fromRgb(102, 85, 67, 200)).value<QColor>();
}

void Settings::setCoastLineColor(const QColor& color) {
    getSettings()->setValue("earthSpace/coastLineColor", color);
}

double Settings::coastLineStrength() {
    return getSettings()->value("earthSpace/coastLineStrength", 0.8).toDouble();
}

void Settings::setCoastLineStrength(double strength) {
    getSettings()->setValue("earthSpace/coastLineStrength", strength);
}

QColor Settings::firBorderLineColor() {
    return getSettings()->value("firDisplay/borderLineColor", QColor::fromRgbF(0.0, 0.0, 1.0)).value<QColor>();
}

void Settings::setFirBorderLineColor(const QColor& color) {
    getSettings()->setValue("firDisplay/borderLineColor", color);
}

double Settings::firBorderLineStrength() {
    return getSettings()->value("firDisplay/borderLineStrength", 1.5).toDouble();
}

void Settings::setFirBorderLineStrength(double strength) {
    getSettings()->setValue("firDisplay/borderLineStrength", strength);
}

QColor Settings::firFontColor() {
    return getSettings()->value("firDisplay/fontColor", QColor::fromRgb(255, 0, 127)).value<QColor>();
}

void Settings::setFirFontColor(const QColor& color) {
    getSettings()->setValue("firDisplay/fontColor", color);
}

QColor Settings::firFillColor() {
    return getSettings()->value("firDisplay/fillColor", QColor::fromRgb(42, 163, 214, 100)).value<QColor>();
}

void Settings::setFirFillColor(const QColor& color) {
    getSettings()->setValue("firDisplay/fillColor", color);
}

QFont Settings::firFont() {
    QFont defaultFont;
    defaultFont.setBold(true);
    defaultFont.setPixelSize(11);
    QFont result = getSettings()->value("firDisplay/font", defaultFont).value<QFont>();
    result.setStyleHint( QFont::SansSerif, QFont::PreferAntialias );
    return result;
}

void Settings::setFirFont(const QFont& font) {
    getSettings()->setValue("firDisplay/font", font);
}

//airport
QColor Settings::airportFontColor() {
    return getSettings()->value("airportDisplay/fontColor", QColor::fromRgb(255, 255, 127)).value<QColor>();
}

void Settings::setAirportFontColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/fontColor", color);
}

QColor Settings::airportDotColor() {
    return getSettings()->value("airportDisplay/dotColor", QColor::fromRgb(85, 170, 255)).value<QColor>();
}

void Settings::setAirportDotColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/dotColor", color);
}

double Settings::airportDotSize() {
    return getSettings()->value("airportDisplay/dotSizer", 4).toDouble();
}

void Settings::setAirportDotSize(double value) {
    getSettings()->setValue("airportDisplay/dotSizer", value);
}

QFont Settings::airportFont() {
    QFont defaultResult;
    defaultResult.setPixelSize(9);
    QFont result = getSettings()->value("airportDisplay/font", defaultResult).value<QFont>();
    result.setStyleHint( QFont::SansSerif, QFont::PreferAntialias );
    return result;
}

void Settings::setAirportFont(const QFont& font) {
    getSettings()->setValue("airportDisplay/font", font);
}

QColor Settings::inactiveAirportFontColor() {
    return getSettings()->value("airportDisplay/inactiveFontColor", QColor::fromRgbF(0.4, 0.4, 0.4, 1)).value<QColor>();
}

void Settings::setInactiveAirportFontColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/inactiveFontColor", color);
}

QColor Settings::inactiveAirportDotColor() {
    return getSettings()->value("airportDisplay/inactiveDotColor", QColor::fromRgbF(0.5, 0.5, 0.5, 1)).value<QColor>();
}

void Settings::setInactiveAirportDotColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/inactiveDotColor", color);
}

double Settings::inactiveAirportDotSize() {
    return getSettings()->value("airportDisplay/inactiveDotSizer", 2).toDouble();
}

void Settings::setInactiveAirportDotSize(double value) {
    getSettings()->setValue("airportDisplay/inactiveDotSizer", value);
}

QFont Settings::inactiveAirportFont() {
    QFont defaultResult;
    defaultResult.setPixelSize(7);
    QFont result = getSettings()->value("airportDisplay/inactiveFont", defaultResult).value<QFont>();
    result.setStyleHint( QFont::SansSerif, QFont::PreferAntialias );
    return result;
}

void Settings::setInactiveAirportFont(const QFont& font) {
    getSettings()->setValue("airportDisplay/inactiveFont", font);
}

bool Settings::showInactiveAirports() {
    return getSettings()->value("airportDisplay/showInactive", false).toBool(); // time-intensive function
}

void Settings::setShowInactiveAirports(const bool& value) {
    getSettings()->setValue("airportDisplay/showInactive", value);
}

QColor Settings::appBorderLineColor() {
    return getSettings()->value("airportDisplay/appBorderLineColor", QColor::fromRgb(255, 255, 127)).value<QColor>();
}

void Settings::setAppBorderLineColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/appBorderLineColor", color);
}

double Settings::appBorderLineStrength() {
    return getSettings()->value("airportDisplay/appBorderLineStrength", 1.5).toDouble();
}

void Settings::setAppBorderLineStrength(double value) {
    getSettings()->setValue("airportDisplay/appBorderLineStrength", value);
}

QColor Settings::appCenterColor() {
    return getSettings()->value("airportDisplay/appCenterColor", QColor::fromRgbF(0.0, 0.0, 1.0, 0.0)).value<QColor>();
}

void Settings::setAppCenterColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/appCenterColor", color);
}

QColor Settings::appMarginColor() {
    return getSettings()->value("airportDisplay/appMarginColor", QColor::fromRgb(85, 170, 255)).value<QColor>();
}

void Settings::setAppMarginColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/appMarginColor", color);
}

QColor Settings::twrMarginColor() {
    return getSettings()->value("airportDisplay/twrMarginColor", QColor::fromRgb(102, 85, 67)).value<QColor>();
}

void Settings::setTwrMarginColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/twrMarginColor", color);
}

QColor Settings::twrCenterColor() {
    return getSettings()->value("airportDisplay/twrCenterColor", QColor::fromRgbF(0.8, 0.8, 0.0, 0.0)).value<QColor>();
}

void Settings::setTwrCenterColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/twrCenterColor", color);
}

QColor Settings::gndBorderLineColor() {
    return getSettings()->value("airportDisplay/gndBorderLineColor", QColor::fromRgb(179, 0, 179)).value<QColor>();
}

void Settings::setGndBorderLineColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/gndBorderLineColor", color);
}

double Settings::gndBorderLineStrength() {
    return getSettings()->value("airportDisplay/gndBorderLineStrength", 0.2).toDouble();
}

void Settings::setGndBorderLineStrength(double value) {
    getSettings()->setValue("airportDisplay/gndBorderLineStrength", value);
}

QColor Settings::gndFillColor() {
    return getSettings()->value("airportDisplay/gndFillColor", QColor::fromRgb(255, 255, 127)).value<QColor>();
}

void Settings::setGndFillColor(const QColor& color) {
    getSettings()->setValue("airportDisplay/gndFillColor", color);
}

// pilot
QColor Settings::pilotFontColor() {
    return getSettings()->value("pilotDisplay/fontColor", QColor::fromRgb(255, 0, 127)).value<QColor>();
}

void Settings::setPilotFontColor(const QColor& color) {
    getSettings()->setValue("pilotDisplay/fontColor", color);
}

QFont Settings::pilotFont() {
    QFont defaultFont;
    defaultFont.setPixelSize(8);
    return getSettings()->value("pilotDisplay/font", defaultFont).value<QFont>();
}

void Settings::setPilotFont(const QFont& font) {
    getSettings()->setValue("pilotDisplay/font", font);
}

QColor Settings::pilotDotColor() {
    return getSettings()->value("pilotDisplay/dotColor", QColor::fromRgb(255, 0, 127)).value<QColor>();
}

void Settings::setPilotDotColor(const QColor& color) {
    getSettings()->setValue("pilotDisplay/dotColor", color);
}

double Settings::pilotDotSize() {
    return getSettings()->value("pilotDisplay/dotSize", 3).toDouble();
}

void Settings::setPilotDotSize(double value) {
    getSettings()->setValue("pilotDisplay/dotSize", value);
}

QColor Settings::timeLineColor() {
    return getSettings()->value("pilotDisplay/timeLineColor", QColor::fromRgb(143, 0, 71)).value<QColor>();
}

void Settings::setTimeLineColor(const QColor& color) {
    getSettings()->setValue("pilotDisplay/timeLineColor", color);
}

QColor Settings::trackLineColor() {
    return getSettings()->value("pilotDisplay/trackLineColor", QColor::fromRgb(170, 255, 127)).value<QColor>();
}

void Settings::setTrackLineColor(const QColor& color) {
    getSettings()->setValue("pilotDisplay/trackLineColor", color);
}

QColor Settings::planLineColor() {
    return getSettings()->value("pilotDisplay/planLineColor", QColor::fromRgb(255, 170, 0)).value<QColor>();
}

void Settings::setPlanLineColor(const QColor& color) {
    getSettings()->setValue("pilotDisplay/planLineColor", color);
}

void Settings::setDashedTrackInFront(bool value) {
    getSettings()->setValue("pilotDisplay/dashedTrackInFront", value);
}

bool Settings::dashedTrackInFront() {
    return getSettings()->value("pilotDisplay/dashedTrackInFront", true).toBool();
}

void Settings::setTrackFront(bool value) {
    getSettings()->setValue("pilotDisplay/trackFront", value);
}

bool Settings::trackFront() {
    return getSettings()->value("pilotDisplay/trackFront", true).toBool();
}

void Settings::setTrackAfter(bool value) {
    getSettings()->setValue("pilotDisplay/trackAfter", value);
}

bool Settings::trackAfter() {
    return getSettings()->value("pilotDisplay/trackAfter", false).toBool();
}

double Settings::timeLineStrength() {
    return getSettings()->value("pilotDisplay/timeLineStrength", 1.0).toDouble();
}

void Settings::setTimeLineStrength(double value) {
    getSettings()->setValue("pilotDisplay/timeLineStrength", value);
}

double Settings::trackLineStrength() {
    return getSettings()->value("pilotDisplay/trackLineStrength", 0).toDouble();
}

void Settings::setTrackLineStrength(double value) {
    getSettings()->setValue("pilotDisplay/trackLineStrength", value);
}

double Settings::planLineStrength() {
    return getSettings()->value("pilotDisplay/planLineStrength", 1).toDouble();
}

void Settings::setPlanLineStrength(double value) {
    getSettings()->setValue("pilotDisplay/planLineStrength", value);
}

void Settings::getRememberedMapPosition(double *xrot, double *yrot, double *zrot, double *zoom, int nr) {
    if(xrot == 0 || yrot == 0 || zrot == 0 || zoom == 0)
        return;

    *xrot = getSettings()->value("defaultMapPosition/xrot" + QString("%1").arg(nr), *xrot).toDouble();
    // ignore yRot: no Earth tilting
    //*yrot = getSettings()->value("defaultMapPosition/yrot" + QString("%1").arg(nr), *yrot).toDouble();
    *zrot = getSettings()->value("defaultMapPosition/zrot" + QString("%1").arg(nr), *zrot).toDouble();
    *zoom = getSettings()->value("defaultMapPosition/zoom" + QString("%1").arg(nr), *zoom).toDouble();
}

void Settings::setRememberedMapPosition(double xrot, double yrot, double zrot, double zoom, int nr) {
    getSettings()->setValue("defaultMapPosition/xrot" + QString("%1").arg(nr), xrot);
    getSettings()->setValue("defaultMapPosition/yrot" + QString("%1").arg(nr), yrot);
    getSettings()->setValue("defaultMapPosition/zrot" + QString("%1").arg(nr), zrot);
    getSettings()->setValue("defaultMapPosition/zoom" + QString("%1").arg(nr), zoom);
}

void Settings::saveState(const QByteArray& state) {
    getSettings()->setValue("mainWindowState/state", state);
}

QByteArray Settings::getSavedState() {
    return getSettings()->value("mainWindowState/state", QByteArray()).toByteArray();
}

void Settings::saveGeometry(const QByteArray& state) {
    getSettings()->setValue("mainWindowState/geometry", state);
}

QByteArray Settings::getSavedGeometry() {
    return getSettings()->value("mainWindowState/geometry", QByteArray()).toByteArray();
}

void Settings::saveSize(const QSize& size) {
    getSettings()->setValue("mainWindowState/size", size);
}

QSize Settings::getSavedSize() {
    return getSettings()->value("mainWindowState/size", QSize()).toSize();
}

void Settings::savePosition(const QPoint& pos) {
    getSettings()->setValue("mainWindowState/position", pos);
}

QPoint Settings::getSavedPosition() {
    return getSettings()->value("mainWindowState/position", QPoint()).toPoint();
}

QStringList Settings::friends() {
    return getSettings()->value("friends/friendList", QStringList()).toStringList();
}

void Settings::addFriend(const QString& friendId) {
    QStringList fl = friends();
    if(!fl.contains(friendId))
        fl.append(friendId);
    getSettings()->setValue("friends/friendList", fl);
}

void Settings::removeFriend(const QString& friendId) {
    QStringList fl = friends();
    int i = fl.indexOf(friendId);
    if(i >= 0 && i < fl.size())
        fl.removeAt(i);
    getSettings()->setValue("friends/friendList", fl);
}

bool Settings::resetOnNextStart() {
    return getSettings()->value("general/resetConfiguration", false).toBool();
}

void Settings::setResetOnNextStart(bool value) {
    getSettings()->setValue("general/resetConfiguration", value);
}

Settings::VoiceType Settings::voiceType() {
    return (VoiceType) getSettings()->value("voice/type", NONE).toInt();
}

void Settings::setVoiceType(Settings::VoiceType type) {
    getSettings()->setValue("voice/type", (int)type);
}

QString Settings::voiceCallsign() {
    return getSettings()->value("voice/callsign").toString();
}

void Settings::setVoiceCallsign(const QString& value) {
    getSettings()->setValue("voice/callsign", value);
}

QString Settings::voiceUser() {
    return getSettings()->value("voice/user").toString();
}

void Settings::setVoiceUser(const QString& value) {
    getSettings()->setValue("voice/user", value);
}

QString Settings::voicePassword() {
    return getSettings()->value("voice/password").toString();
}

void Settings::setVoicePassword(const QString& value) {
    getSettings()->setValue("voice/password", value);
}

// Airport traffic
bool Settings::filterTraffic() {
    return getSettings()->value("airportTraffic/filterTraffic", true).toBool();
}

void Settings::setFilterTraffic(bool v) {
    getSettings()->setValue("airportTraffic/filterTraffic", v);
}

int Settings::filterDistance() {
    return getSettings()->value("airportTraffic/filterDistance", 5).toInt();
}

void Settings::setFilterDistance(int v) {
    getSettings()->setValue("airportTraffic/filterDistance", v);
}

double Settings::filterArriving() {
    return getSettings()->value("airportTraffic/filterArriving", 1.0).toDouble();
}

void Settings::setFilterArriving(double v) {
    getSettings()->setValue("airportTraffic/filterArriving", v);
}
// airport congestion
bool Settings::showAirportCongestion() {
    return getSettings()->value("airportTraffic/showCongestion", true).toBool();
}
void Settings::setAirportCongestion(bool value) {
    getSettings()->setValue("airportTraffic/showCongestion", value);
}

int Settings::airportCongestionMinimum() {
    return getSettings()->value("airportTraffic/minimumMovements", 8).toInt();
}

void Settings::setAirportCongestionMinimum(int value) {
    getSettings()->setValue("airportTraffic/minimumMovements", value);
}

QColor Settings::airportCongestionBorderLineColor() {
    return getSettings()->value("airportTraffic/borderLineColor", QColor::fromRgb(255, 0, 127, 150)).value<QColor>();
}

void Settings::setAirportCongestionBorderLineColor(const QColor& color) {
    getSettings()->setValue("airportTraffic/borderLineColor", color);
}

double Settings::airportCongestionBorderLineStrength() {
    return getSettings()->value("airportTraffic/borderLineStrength", 2).toDouble();
}

void Settings::setAirportCongestionBorderLineStrength(double value) {
    getSettings()->setValue("airportTraffic/borderLineStrength", value);
}

// zooming
int Settings::wheelMax() {
    return getSettings()->value("mouseWheel/wheelMax", 120).toInt();
}

void Settings::setWheelMax(int value) {
    getSettings()->setValue("mouseWheel/wheelMax", value);
}

double Settings::zoomFactor() {
    return getSettings()->value("zoom/zoomFactor", 1.0).toDouble();
}

void Settings::setZoomFactor(double value) {
    getSettings()->setValue("zoom/zoomFactor", value);
}

bool Settings::saveWhazzupData()
{
    return getSettings()->value("general/saveWhazzupData", false).toBool();
}

void Settings::setSaveWhazzupData(bool value)
{
    getSettings()->setValue("general/saveWhazzupData" , value);
}
