/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Superset Dashboard Integration Panel Implementation
  Embeds Apache Superset dashboards with video synchronization
***/

#include "superset_panel.h"

#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QMessageBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QSettings>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

#include "sports/kafka_publisher.h"
#include "sports/triangle_defense_sync.h"
#include "config/config.h"

namespace olive {

SupersetPanel::SupersetPanel(const QString& object_name, QWidget* parent)
  : PanelWidget(object_name, parent)
  , main_layout_(nullptr)
  , toolbar_layout_(nullptr)
  , main_splitter_(nullptr)
  , dashboard_selector_(nullptr)
  , refresh_button_(nullptr)
  , sync_button_(nullptr)
  , sync_status_label_(nullptr)
  , timestamp_display_(nullptr)
  , web_view_(nullptr)
  , web_page_(nullptr)
  , web_channel_(nullptr)
  , superset_bridge_(nullptr)
  , kafka_publisher_(nullptr)
  , triangle_sync_(nullptr)
  , sync_timer_(nullptr)
  , current_video_timestamp_(0)
  , is_video_playing_(false)
  , video_playback_rate_(1.0)
  , dashboard_loaded_(false)
  , sync_enabled_(true)
  , sync_interval_ms_(100)
{
  SetupUI();
  SetupWebEngine();
  SetupConnections();
  
  // Initialize integration services
  kafka_publisher_ = new KafkaPublisher(this);
  triangle_sync_ = new TriangleDefenseSync(this);
  
  // Load configuration
  QSettings settings;
  superset_base_url_ = settings.value("superset/base_url", "http://localhost:8088").toString();
  sync_interval_ms_ = settings.value("superset/sync_interval_ms", 100).toInt();
  
  // Setup sync timer
  sync_timer_ = new QTimer(this);
  sync_timer_->setInterval(sync_interval_ms_);
  connect(sync_timer_, &QTimer::timeout, this, &SupersetPanel::OnSyncTimerTimeout);
}

SupersetPanel::~SupersetPanel()
{
  if (sync_timer_ && sync_timer_->isActive()) {
    sync_timer_->stop();
  }
}

void SupersetPanel::SetupUI()
{
  // Main layout
  main_layout_ = new QVBoxLayout(this);
  main_layout_->setContentsMargins(2, 2, 2, 2);
  main_layout_->setSpacing(2);

  // Toolbar layout
  toolbar_layout_ = new QHBoxLayout();
  
  // Dashboard selector
  dashboard_selector_ = new QComboBox();
  dashboard_selector_->addItem("Triangle Defense Overview", "triangle-defense-overview");
  dashboard_selector_->addItem("Formation Analysis", "formation-analysis");
  dashboard_selector_->addItem("M.E.L. Pipeline", "mel-pipeline");
  dashboard_selector_->addItem("Coaching Alerts", "coaching-alerts");
  dashboard_selector_->addItem("Real-time Metrics", "realtime-metrics");
  dashboard_selector_->setCurrentIndex(0);
  
  // Control buttons
  refresh_button_ = new QPushButton("Refresh");
  refresh_button_->setToolTip("Refresh dashboard");
  
  sync_button_ = new QPushButton("Sync: ON");
  sync_button_->setToolTip("Toggle video synchronization");
  sync_button_->setCheckable(true);
  sync_button_->setChecked(true);
  
  // Status display
  sync_status_label_ = new QLabel("Ready");
  sync_status_label_->setStyleSheet("color: green; font-weight: bold;");
  
  timestamp_display_ = new QLineEdit("00:00:00.000");
  timestamp_display_->setReadOnly(true);
  timestamp_display_->setMaximumWidth(100);
  timestamp_display_->setToolTip("Current video timestamp");
  
  // Add to toolbar
  toolbar_layout_->addWidget(new QLabel("Dashboard:"));
  toolbar_layout_->addWidget(dashboard_selector_);
  toolbar_layout_->addWidget(refresh_button_);
  toolbar_layout_->addWidget(sync_button_);
  toolbar_layout_->addStretch();
  toolbar_layout_->addWidget(new QLabel("Status:"));
  toolbar_layout_->addWidget(sync_status_label_);
  toolbar_layout_->addWidget(new QLabel("Time:"));
  toolbar_layout_->addWidget(timestamp_display_);

  main_layout_->addLayout(toolbar_layout_);
  
  // Main splitter for future expansion
  main_splitter_ = new QSplitter(Qt::Vertical);
  main_layout_->addWidget(main_splitter_);
}

void SupersetPanel::SetupWebEngine()
{
  // Create web engine view
  web_view_ = new QWebEngineView();
  web_page_ = new QWebEnginePage();
  web_view_->setPage(web_page_);
  
  // Configure web engine settings
  QWebEngineSettings* settings = web_page_->settings();
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
  
  // Setup web channel for JavaScript communication
  web_channel_ = new QWebChannel(this);
  superset_bridge_ = new SupersetBridge(this);
  web_channel_->registerObject("supersetBridge", superset_bridge_);
  web_page_->setWebChannel(web_channel_);
  
  main_splitter_->addWidget(web_view_);
}

void SupersetPanel::SetupConnections()
{
  // UI connections
  connect(dashboard_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            QString dashboard_id = dashboard_selector_->itemData(index).toString();
            LoadDashboard(dashboard_id);
          });
          
  connect(refresh_button_, &QPushButton::clicked, this, &SupersetPanel::RefreshDashboard);
  
  connect(sync_button_, &QPushButton::toggled, [this](bool checked) {
    sync_enabled_ = checked;
    sync_button_->setText(checked ? "Sync: ON" : "Sync: OFF");
    if (checked && is_video_playing_) {
      sync_timer_->start();
    } else {
      sync_timer_->stop();
    }
  });
  
  // Web engine connections
  connect(web_page_, &QWebEnginePage::loadFinished, this, &SupersetPanel::OnWebPageLoadFinished);
  connect(web_page_, &QWebEnginePage::loadProgress, this, &SupersetPanel::OnWebPageLoadProgress);
  
  // Superset bridge connections
  connect(superset_bridge_, &SupersetBridge::messageReceived, this, &SupersetPanel::OnJavaScriptMessage);
  connect(superset_bridge_, &SupersetBridge::dashboardInteraction, 
          [this](const QString& event_type, const QJsonObject& data) {
            // Publish dashboard interaction events to Kafka
            if (kafka_publisher_) {
              QJsonObject event;
              event["type"] = "dashboard_interaction";
              event["event_type"] = event_type;
              event["data"] = data;
              event["timestamp"] = current_video_timestamp_;
              kafka_publisher_->PublishEvent("dashboard-events", event);
            }
          });
}

void SupersetPanel::LoadDashboard(const QString& dashboard_id)
{
  if (dashboard_id.isEmpty()) {
    return;
  }
  
  current_dashboard_id_ = dashboard_id;
  dashboard_loaded_ = false;
  
  UpdateSyncStatus("Loading...");
  
  // Construct Superset URL with embedded parameters
  QString url = QString("%1/superset/dashboard/%2/?standalone=true&show_filters=true")
                .arg(superset_base_url_, dashboard_id);
                
  // Add authentication if token is available
  if (!auth_token_.isEmpty()) {
    url += QString("&access_token=%1").arg(auth_token_);
  }
  
  // Load the dashboard
  web_view_->load(QUrl(url));
  
  // Publish dashboard load event
  if (kafka_publisher_) {
    QJsonObject event;
    event["type"] = "dashboard_load";
    event["dashboard_id"] = dashboard_id;
    event["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    kafka_publisher_->PublishEvent("dashboard-events", event);
  }
}

void SupersetPanel::SetAuthToken(const QString& token)
{
  auth_token_ = token;
  
  // If dashboard is already loaded, reload with new auth
  if (dashboard_loaded_ && !current_dashboard_id_.isEmpty()) {
    LoadDashboard(current_dashboard_id_);
  }
}

void SupersetPanel::SyncWithVideoTimestamp(qint64 timestamp_ms)
{
  current_video_timestamp_ = timestamp_ms;
  
  // Update timestamp display
  QTime time = QTime::fromMSecsSinceStartOfDay(timestamp_ms % (24 * 60 * 60 * 1000));
  timestamp_display_->setText(time.toString("hh:mm:ss.zzz"));
  
  // Send timestamp to Superset if dashboard is loaded
  if (dashboard_loaded_ && superset_bridge_) {
    superset_bridge_->updateTimestamp(timestamp_ms);
  }
}

void SupersetPanel::UpdateFormationData(const QJsonObject& formation_data)
{
  current_formation_data_ = formation_data;
  
  // Send formation data to Superset
  if (dashboard_loaded_ && superset_bridge_) {
    QJsonDocument doc(formation_data);
    superset_bridge_->updateFormationData(doc.toJson(QJsonDocument::Compact));
  }
  
  // Publish formation update event
  if (kafka_publisher_) {
    QJsonObject event;
    event["type"] = "formation_update";
    event["formation_data"] = formation_data;
    event["video_timestamp"] = current_video_timestamp_;
    kafka_publisher_->PublishEvent("formation-events", event);
  }
}

QString SupersetPanel::GetCurrentDashboardUrl() const
{
  if (web_view_) {
    return web_view_->url().toString();
  }
  return QString();
}

void SupersetPanel::OnVideoPlaybackChanged(bool playing)
{
  is_video_playing_ = playing;
  
  if (sync_enabled_) {
    if (playing) {
      sync_timer_->start();
      UpdateSyncStatus("Syncing");
    } else {
      sync_timer_->stop();
      UpdateSyncStatus("Paused");
    }
  }
  
  // Notify Superset of playback state
  if (dashboard_loaded_ && superset_bridge_) {
    QJsonObject message;
    message["action"] = "playback_state";
    message["playing"] = playing;
    message["timestamp"] = current_video_timestamp_;
    
    QJsonDocument doc(message);
    superset_bridge_->receiveMessage(doc.toJson(QJsonDocument::Compact));
  }
}

void SupersetPanel::OnVideoSeekChanged(qint64 position)
{
  SyncWithVideoTimestamp(position);
  
  // Publish seek event
  if (kafka_publisher_) {
    QJsonObject event;
    event["type"] = "video_seek";
    event["position"] = position;
    event["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    kafka_publisher_->PublishEvent("video-events", event);
  }
}

void SupersetPanel::OnVideoRateChanged(double rate)
{
  video_playback_rate_ = rate;
  
  // Adjust sync timer interval based on playback rate
  if (sync_timer_) {
    int adjusted_interval = static_cast<int>(sync_interval_ms_ / qMax(0.1, rate));
    sync_timer_->setInterval(adjusted_interval);
  }
}

void SupersetPanel::OnTriangleDefenseCall(const QString& call_type, const QJsonObject& data)
{
  // Highlight the call in Superset
  if (dashboard_loaded_ && superset_bridge_) {
    superset_bridge_->highlightTriangleCall(call_type);
  }
  
  // Publish Triangle Defense event
  if (kafka_publisher_) {
    QJsonObject event;
    event["type"] = "triangle_defense_call";
    event["call_type"] = call_type;
    event["call_data"] = data;
    event["video_timestamp"] = current_video_timestamp_;
    kafka_publisher_->PublishEvent("triangle-defense-events", event);
  }
  
  UpdateSyncStatus(QString("Triangle: %1").arg(call_type));
}

void SupersetPanel::OnFormationClassified(const QString& formation_type, double confidence)
{
  // Update formation data
  QJsonObject formation_data;
  formation_data["type"] = formation_type;
  formation_data["confidence"] = confidence;
  formation_data["timestamp"] = current_video_timestamp_;
  
  UpdateFormationData(formation_data);
}

void SupersetPanel::OnDashboardLoaded()
{
  dashboard_loaded_ = true;
  UpdateSyncStatus("Ready");
  
  // Inject our custom JavaScript
  InjectJavaScript();
  
  // Send initial state
  if (superset_bridge_) {
    superset_bridge_->updateTimestamp(current_video_timestamp_);
    
    if (!current_formation_data_.isEmpty()) {
      QJsonDocument doc(current_formation_data_);
      superset_bridge_->updateFormationData(doc.toJson(QJsonDocument::Compact));
    }
  }
}

void SupersetPanel::OnDashboardError(const QString& error)
{
  UpdateSyncStatus(QString("Error: %1").arg(error));
  dashboard_loaded_ = false;
}

void SupersetPanel::RefreshDashboard()
{
  if (!current_dashboard_id_.isEmpty()) {
    LoadDashboard(current_dashboard_id_);
  } else {
    web_view_->reload();
  }
}

void SupersetPanel::OnWebPageLoadFinished(bool success)
{
  if (success) {
    OnDashboardLoaded();
  } else {
    OnDashboardError("Failed to load dashboard");
  }
}

void SupersetPanel::OnWebPageLoadProgress(int progress)
{
  UpdateSyncStatus(QString("Loading %1%").arg(progress));
}

void SupersetPanel::OnJavaScriptMessage(const QJsonObject& message)
{
  QString action = message["action"].toString();
  
  if (action == "dashboard_ready") {
    OnDashboardLoaded();
  } else if (action == "filter_changed") {
    // Handle dashboard filter changes
    QJsonObject data = message["data"].toObject();
    
    if (kafka_publisher_) {
      QJsonObject event;
      event["type"] = "dashboard_filter_change";
      event["filter_data"] = data;
      event["video_timestamp"] = current_video_timestamp_;
      kafka_publisher_->PublishEvent("dashboard-events", event);
    }
  } else if (action == "chart_clicked") {
    // Handle chart interactions
    QJsonObject data = message["data"].toObject();
    
    if (kafka_publisher_) {
      QJsonObject event;
      event["type"] = "chart_interaction";
      event["chart_data"] = data;
      event["video_timestamp"] = current_video_timestamp_;
      kafka_publisher_->PublishEvent("dashboard-events", event);
    }
  }
}

void SupersetPanel::OnSyncTimerTimeout()
{
  if (dashboard_loaded_ && sync_enabled_ && is_video_playing_) {
    SyncWithVideoTimestamp(current_video_timestamp_);
  }
}

void SupersetPanel::InjectJavaScript()
{
  QString js = R"(
    // Apache-Cleats Superset Integration Script
    (function() {
      // Setup communication channel
      if (typeof qt !== 'undefined' && qt.webChannelTransport) {
        new QWebChannel(qt.webChannelTransport, function(channel) {
          window.supersetBridge = channel.objects.supersetBridge;
          
          // Notify that dashboard is ready
          window.supersetBridge.receiveMessage(JSON.stringify({
            action: 'dashboard_ready',
            timestamp: Date.now()
          }));
        });
      }
      
      // Custom event handlers for Superset interactions
      document.addEventListener('click', function(e) {
        if (e.target.closest('.superset-chart')) {
          if (window.supersetBridge) {
            window.supersetBridge.receiveMessage(JSON.stringify({
              action: 'chart_clicked',
              data: {
                chart_id: e.target.closest('.superset-chart').id,
                position: { x: e.clientX, y: e.clientY }
              }
            }));
          }
        }
      });
      
      // Listen for filter changes
      if (window.Superset) {
        const originalFilterChanged = window.Superset.filterChanged || function() {};
        window.Superset.filterChanged = function(filters) {
          if (window.supersetBridge) {
            window.supersetBridge.receiveMessage(JSON.stringify({
              action: 'filter_changed',
              data: { filters: filters }
            }));
          }
          return originalFilterChanged.apply(this, arguments);
        };
      }
    })();
  )";
  
  web_page_->runJavaScript(js);
}

void SupersetPanel::SendMessageToSuperset(const QJsonObject& message)
{
  QJsonDocument doc(message);
  QString js = QString("if (window.receiveCleatMessage) { window.receiveCleatMessage(%1); }")
               .arg(doc.toJson(QJsonDocument::Compact));
  web_page_->runJavaScript(js);
}

void SupersetPanel::UpdateSyncStatus(const QString& status)
{
  if (sync_status_label_) {
    sync_status_label_->setText(status);
    
    // Color coding for status
    if (status.contains("Error", Qt::CaseInsensitive)) {
      sync_status_label_->setStyleSheet("color: red; font-weight: bold;");
    } else if (status.contains("Loading")) {
      sync_status_label_->setStyleSheet("color: orange; font-weight: bold;");
    } else if (status.contains("Syncing") || status.contains("Ready")) {
      sync_status_label_->setStyleSheet("color: green; font-weight: bold;");
    } else {
      sync_status_label_->setStyleSheet("color: blue; font-weight: bold;");
    }
  }
}

void SupersetPanel::showEvent(QShowEvent* event)
{
  PanelWidget::showEvent(event);
  
  // Load default dashboard if none loaded
  if (!dashboard_loaded_ && !current_dashboard_id_.isEmpty()) {
    LoadDashboard(current_dashboard_id_);
  }
}

void SupersetPanel::hideEvent(QHideEvent* event)
{
  // Stop sync timer when hidden
  if (sync_timer_ && sync_timer_->isActive()) {
    sync_timer_->stop();
  }
  
  PanelWidget::hideEvent(event);
}

void SupersetPanel::resizeEvent(QResizeEvent* event)
{
  PanelWidget::resizeEvent(event);
  
  // Notify Superset of resize for responsive adjustments
  if (dashboard_loaded_ && superset_bridge_) {
    QJsonObject message;
    message["action"] = "resize";
    message["width"] = event->size().width();
    message["height"] = event->size().height();
    
    QJsonDocument doc(message);
    superset_bridge_->receiveMessage(doc.toJson(QJsonDocument::Compact));
  }
}

// SupersetBridge Implementation
SupersetBridge::SupersetBridge(QObject* parent)
  : QObject(parent)
{
}

void SupersetBridge::receiveMessage(const QString& message)
{
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
  
  if (error.error == QJsonParseError::NoError) {
    emit messageReceived(doc.object());
  }
}

void SupersetBridge::updateTimestamp(qint64 timestamp)
{
  QJsonObject message;
  message["action"] = "update_timestamp";
  message["timestamp"] = timestamp;
  
  QJsonDocument doc(message);
  receiveMessage(doc.toJson(QJsonDocument::Compact));
}

void SupersetBridge::updateFormationData(const QString& data)
{
  QJsonObject message;
  message["action"] = "update_formation_data";
  message["data"] = data;
  
  QJsonDocument doc(message);
  receiveMessage(doc.toJson(QJsonDocument::Compact));
}

void SupersetBridge::highlightTriangleCall(const QString& call_type)
{
  QJsonObject message;
  message["action"] = "highlight_triangle_call";
  message["call_type"] = call_type;
  
  QJsonDocument doc(message);
  receiveMessage(doc.toJson(QJsonDocument::Compact));
}

} // namespace olive
