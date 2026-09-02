#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPainter>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QScreen>
#include <QGuiApplication>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <iostream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/input.h>
#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

// Evdev Global Hotkey Listener (Runs in a background thread, monitors all keyboards)
class EvdevHotkeyWorker : public QThread {
    Q_OBJECT
public:
    void stop() {
        m_running = false;
        wait();
    }

signals:
    void hotkeyPressed();

protected:
    void run() override {
        m_running = true;

        // Detect all keyboard input devices
        std::vector<int> fds;
        DIR *dir = opendir("/dev/input");
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != nullptr) {
                if (strncmp(ent->d_name, "event", 5) == 0) {
                    std::string path = std::string("/dev/input/") + ent->d_name;
                    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
                    if (fd >= 0) {
                        unsigned long evbit = 0;
                        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit) >= 0) {
                            if (evbit & (1 << EV_KEY)) {
                                fds.push_back(fd);
                            } else {
                                close(fd);
                            }
                        } else {
                            close(fd);
                        }
                    }
                }
            }
            closedir(dir);
        }

        if (fds.empty()) {
            std::cerr << "No readable input event devices found." << std::endl;
            return;
        }

        bool ctrlPressed = false;
        bool altPressed = false;

        while (m_running) {
            fd_set readfds;
            FD_ZERO(&readfds);
            int maxfd = -1;

            for (int fd : fds) {
                FD_SET(fd, &readfds);
                if (fd > maxfd) maxfd = fd;
            }

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms timeout

            int res = select(maxfd + 1, &readfds, nullptr, nullptr, &tv);
            if (res > 0) {
                for (int fd : fds) {
                    if (FD_ISSET(fd, &readfds)) {
                        struct input_event ev;
                        while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
                            if (ev.type == EV_KEY) {
                                if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                                    ctrlPressed = (ev.value != 0);
                                } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                                    altPressed = (ev.value != 0);
                                } else if (ev.code == KEY_SPACE && ev.value == 1) { // Key down
                                    if (ctrlPressed && altPressed) {
                                        emit hotkeyPressed();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (int fd : fds) {
            close(fd);
        }
    }

private:
    std::atomic<bool> m_running{false};
};

// HUD Overlay Widget (Frameless, translucent, stays on top)
class HudOverlay : public QWidget {
    Q_OBJECT
public:
    explicit HudOverlay(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);

        m_iconLabel = new QLabel(this);
        m_iconLabel->setAlignment(Qt::AlignCenter);

        m_textLabel = new QLabel(this);
        m_textLabel->setAlignment(Qt::AlignCenter);
        m_textLabel->setStyleSheet("color: #FFFFFF; font-size: 14px; font-weight: bold;");

        layout->addWidget(m_iconLabel);
        layout->addWidget(m_textLabel);

        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &QWidget::hide);
    }

    void showStatus(bool muted, const QIcon &icon) {
        m_iconLabel->setPixmap(icon.pixmap(64, 64));
        m_textLabel->setText(muted ? tr("Micrófono Muteado") : tr("Micrófono Activo"));

        adjustSize();

        // Center on the active / primary screen
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            QRect geom = screen->geometry();
            int x = geom.x() + (geom.width() - width()) / 2;
            int y = geom.y() + (geom.height() - height()) * 4 / 5; // bottom area
            move(x, y);
        }

        show();
        raise();
        m_timer->start(1400);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(18, 22, 28, 220));
        painter.setPen(QPen(QColor(255, 255, 255, 40), 1.5));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 16, 16);
    }

private:
    QLabel *m_iconLabel;
    QLabel *m_textLabel;
    QTimer *m_timer;
};

// Main Controller
class MicIndicator : public QObject {
    Q_OBJECT
public:
    MicIndicator(QApplication *app) : m_app(app) {
        // Load Icons
        QString appDir = QApplication::applicationDirPath();
        QString iconOnPath = appDir + "/resources/mic-on.png";
        QString iconMutePath = appDir + "/resources/mic-mute.png";

        if (!QFile::exists(iconOnPath)) iconOnPath = ":/resources/mic-on.png";
        if (!QFile::exists(iconMutePath)) iconMutePath = ":/resources/mic-mute.png";

        // Fallback search in working dir / repo root
        if (!QFile::exists(iconOnPath)) iconOnPath = "resources/mic-on.png";
        if (!QFile::exists(iconMutePath)) iconMutePath = "resources/mic-mute.png";

        m_iconOn = QIcon(iconOnPath);
        m_iconMute = QIcon(iconMutePath);

        // Fallback to standard theme icons if files missing
        if (m_iconOn.isNull()) m_iconOn = QIcon::fromTheme("audio-input-microphone");
        if (m_iconMute.isNull()) m_iconMute = QIcon::fromTheme("audio-input-microphone-muted");

        m_hud = new HudOverlay();

        setupIpcServer();
        setupTray();
        setupPulseAudio();
        setupEvdevHotkey();
    }

    ~MicIndicator() {
        if (m_hotkeyWorker) {
            m_hotkeyWorker->stop();
            delete m_hotkeyWorker;
        }
        if (m_ipcServer) {
            m_ipcServer->close();
        }
        if (m_paContext) {
            pa_context_disconnect(m_paContext);
            pa_context_unref(m_paContext);
        }
        if (m_paMainloop) {
            pa_glib_mainloop_free(m_paMainloop);
        }
    }

    void toggleMute() {
        bool targetMute = !m_isMuted;
        setMicMute(targetMute);
    }

    void setMicMute(bool mute) {
        if (m_paContext && pa_context_get_state(m_paContext) == PA_CONTEXT_READY && !m_defaultSourceName.isEmpty()) {
            pa_operation *op = pa_context_set_source_mute_by_name(
                m_paContext,
                m_defaultSourceName.toUtf8().constData(),
                mute ? 1 : 0,
                nullptr,
                nullptr
            );
            if (op) pa_operation_unref(op);
        } else {
            QProcess::execute("pactl", {"set-source-mute", "@DEFAULT_SOURCE@", mute ? "1" : "0"});
        }
    }

    void updateState(bool muted, bool triggerHud = false) {
        bool changed = (m_isMuted != muted);
        m_isMuted = muted;

        m_trayIcon->setIcon(m_isMuted ? m_iconMute : m_iconOn);
        m_trayIcon->setToolTip(m_isMuted ? tr("Micrófono: Muteado (Ctrl+Alt+Espacio)") : tr("Micrófono: Activo (Ctrl+Alt+Espacio)"));
        m_toggleAction->setText(m_isMuted ? tr("Desmutear Micrófono") : tr("Mutear Micrófono"));

        if ((changed && m_initialized) || triggerHud) {
            m_hud->showStatus(m_isMuted, m_isMuted ? m_iconMute : m_iconOn);
        }

        m_initialized = true;
    }

private:
    void setupEvdevHotkey() {
        m_hotkeyWorker = new EvdevHotkeyWorker();
        connect(m_hotkeyWorker, &EvdevHotkeyWorker::hotkeyPressed, this, &MicIndicator::toggleMute, Qt::QueuedConnection);
        m_hotkeyWorker->start();
    }

    void setupIpcServer() {
        QString socketName = "mic_indicator_" + QString::fromLocal8Bit(qgetenv("USER"));
        QLocalServer::removeServer(socketName);

        m_ipcServer = new QLocalServer(this);
        connect(m_ipcServer, &QLocalServer::newConnection, this, [this]() {
            while (m_ipcServer->hasPendingConnections()) {
                QLocalSocket *client = m_ipcServer->nextPendingConnection();
                connect(client, &QLocalSocket::readyRead, this, [this, client]() {
                    QByteArray cmd = client->readAll().trimmed();
                    if (cmd == "toggle" || cmd.isEmpty()) {
                        toggleMute();
                    }
                    client->disconnectFromServer();
                });
            }
        });
        m_ipcServer->listen(socketName);
    }

    void setupTray() {
        m_trayIcon = new QSystemTrayIcon(m_iconOn, this);

        auto *menu = new QMenu();
        m_toggleAction = menu->addAction(tr("Mutear Micrófono (Ctrl+Alt+Espacio)"), this, [this]() {
            toggleMute();
        });

        menu->addSeparator();

        m_autostartAction = menu->addAction(tr("Iniciar con el sistema"));
        m_autostartAction->setCheckable(true);
        m_autostartAction->setChecked(isAutostartEnabled());
        connect(m_autostartAction, &QAction::toggled, this, &MicIndicator::setAutostart);

        menu->addSeparator();
        menu->addAction(tr("Salir"), m_app, &QApplication::quit);

        m_trayIcon->setContextMenu(menu);

        connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                toggleMute();
            }
        });

        m_trayIcon->show();
    }

    // PulseAudio integration with GLib / Qt event loop (Event-driven, no polling)
    void setupPulseAudio() {
        m_paMainloop = pa_glib_mainloop_new(nullptr);
        pa_mainloop_api *api = pa_glib_mainloop_get_api(m_paMainloop);

        m_paContext = pa_context_new(api, "Microphone Indicator");
        pa_context_set_state_callback(m_paContext, &MicIndicator::contextStateCallback, this);

        if (pa_context_connect(m_paContext, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
            std::cerr << "Failed to connect to PulseAudio context" << std::endl;
        }
    }

    static void contextStateCallback(pa_context *c, void *userdata) {
        auto *self = static_cast<MicIndicator*>(userdata);
        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_READY: {
                // Subscribe to source (mic) change events
                pa_context_set_subscribe_callback(c, &MicIndicator::subscribeCallback, self);
                pa_operation *op = pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SOURCE, nullptr, nullptr);
                if (op) pa_operation_unref(op);

                self->queryDefaultSourceState();
                break;
            }
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                std::cerr << "PulseAudio context state failed/terminated" << std::endl;
                break;
            default:
                break;
        }
    }

    static void subscribeCallback(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
        auto *self = static_cast<MicIndicator*>(userdata);
        if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE) {
            self->queryDefaultSourceState();
        }
    }

    void queryDefaultSourceState() {
        if (!m_paContext || pa_context_get_state(m_paContext) != PA_CONTEXT_READY) return;

        pa_operation *op = pa_context_get_server_info(m_paContext, &MicIndicator::serverInfoCallback, this);
        if (op) pa_operation_unref(op);
    }

    static void serverInfoCallback(pa_context *c, const pa_server_info *i, void *userdata) {
        if (!i || !i->default_source_name) return;
        auto *self = static_cast<MicIndicator*>(userdata);
        self->m_defaultSourceName = QString::fromUtf8(i->default_source_name);

        pa_operation *op = pa_context_get_source_info_by_name(c, i->default_source_name, &MicIndicator::sourceInfoCallback, self);
        if (op) pa_operation_unref(op);
    }

    static void sourceInfoCallback(pa_context *c, const pa_source_info *i, int eol, void *userdata) {
        if (eol > 0 || !i) return;
        auto *self = static_cast<MicIndicator*>(userdata);
        bool muted = (i->mute != 0);

        QMetaObject::invokeMethod(self, [self, muted]() {
            self->updateState(muted);
        }, Qt::QueuedConnection);
    }

    // Autostart management via ~/.config/autostart/
    QString autostartFilePath() const {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        return configDir + "/autostart/microphone-indicator.desktop";
    }

    bool isAutostartEnabled() const {
        return QFile::exists(autostartFilePath());
    }

    void setAutostart(bool enable) {
        QString path = autostartFilePath();
        if (enable) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "[Desktop Entry]\n";
                out << "Type=Application\n";
                out << "Name=Microphone Indicator\n";
                out << "Exec=/opt/microphone-indicator/microphone-indicator\n";
                out << "Icon=audio-input-microphone\n";
                out << "Comment=Muestra el estado del micrófono y permite mutearlo\n";
                out << "Terminal=false\n";
                out << "Categories=Utility;Audio;\n";
            }
        } else {
            QFile::remove(path);
        }
    }

    QApplication *m_app;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QAction *m_toggleAction = nullptr;
    QAction *m_autostartAction = nullptr;
    HudOverlay *m_hud = nullptr;
    QLocalServer *m_ipcServer = nullptr;
    EvdevHotkeyWorker *m_hotkeyWorker = nullptr;

    QIcon m_iconOn;
    QIcon m_iconMute;

    bool m_isMuted = false;
    bool m_initialized = false;

    QString m_defaultSourceName;
    pa_glib_mainloop *m_paMainloop = nullptr;
    pa_context *m_paContext = nullptr;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("Microphone Indicator");
    app.setApplicationDisplayName("Microphone Indicator");

    QString socketName = "mic_indicator_" + QString::fromLocal8Bit(qgetenv("USER"));

    // Check if arguments like --toggle or an existing instance exists
    bool toggleOnly = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--toggle" || QString(argv[i]) == "-t") {
            toggleOnly = true;
            break;
        }
    }

    // Try sending command to existing instance
    QLocalSocket socket;
    socket.connectToServer(socketName);
    if (socket.waitForConnected(300)) {
        socket.write("toggle\n");
        socket.flush();
        socket.waitForBytesWritten(300);
        return 0; // Command sent to existing daemon!
    }

    // If --toggle was passed but no daemon running, toggle via pactl directly
    if (toggleOnly) {
        QProcess::execute("pactl", {"set-source-mute", "@DEFAULT_SOURCE@", "toggle"});
        return 0;
    }

    // Start daemon instance
    MicIndicator indicator(&app);

    return app.exec();
}
