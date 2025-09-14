/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Sports Analytics Dashboard Implementation
  Comprehensive sports data analytics and visualization dashboard with Apache Superset integration
***/

#include "sports_analytics_dashboard.h"

#include <QDebug>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QJsonParseError>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QtMath>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>

#include "sports_integration_coordinator.h"
#include "formation_overlay.h"

namespace olive {

SportsAnalyticsDashboard::SportsAnalyticsDashboard(QWidget* parent)
  : QWidget(parent)
  , integration_coordinator_(nullptr)
  , superset_integration_(nullptr)
  , triangle_defense_sync_(nullptr)
  , timeline_sync_(nullptr)
  , current_mode_(AnalyticsDashboardMode::RealTimeAnalysis)
  , is_initialized_(false)
  , auto_refresh_enabled_(true)
  , global_refresh_interval_ms_(10000)
  , main_layout_(nullptr)
  , toolbar_layout_(nullptr)
  , dashboard_layout_(nullptr)
  , main_tabs_(nullptr)
  , main_splitter_(nullptr)
  , mode_selector_(nullptr)
  , layout_selector_(nullptr)
  , refresh_button_(nullptr)
  , export_button_(nullptr)
  , settings_button_(nullptr)
  , auto_refresh_checkbox_(nullptr)
  , refresh_interval_spinbox_(nullptr)
  , analytics_panel_(nullptr)
  , real_time_panel_(nullptr)
  , configuration_panel_(nullptr)
  , superset_panel_(nullptr)
  , network_manager_(nullptr)
  , superset_web_view_(nullptr)
  , web_channel_(nullptr)
  , refresh_timer_(nullptr)
  , data_update_timer_(nullptr)
  , metrics_update_timer_(nullptr)
  , dashboard_theme_("default")
  , widget_drag_mode_(false)
  , analytics_engine_(nullptr)
  , total_dashboard_render_time_(0)
  , dashboard_refresh_count_(0)
{
  qInfo() << "Initializing Sports Analytics Dashboard";
  
  // Set up configuration directories
  config_directory_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/analytics";
  data_cache_directory_ = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/analytics";
  
  QDir().mkpath(config_directory_);
  QDir().mkpath(data_cache_directory_);
  
  // Initialize analytics engine
  analytics_engine_ = new QJSEngine(this);
  
  // Initialize network manager
  network_manager_ = new QNetworkAccessManager(this);
  connect(network_manager_, &QNetworkAccessManager::finished,
          this, &SportsAnalyticsDashboard::OnSupersetReplyFinished);
  
  // Setup timers
  refresh_timer_ = new QTimer(this);
  refresh_timer_->setInterval(global_refresh_interval_ms_);
  connect(refresh_timer_, &QTimer::timeout, this, &SportsAnalyticsDashboard::OnRefreshTimer);
  
  data_update_timer_ = new QTimer(this);
  data_update_timer_->setInterval(1000); // 1 second for real-time updates
  connect(data_update_timer_, &QTimer::timeout, this, &SportsAnalyticsDashboard::OnDataUpdateTimer);
  
  metrics_update_timer_ = new QTimer(this);
  metrics_update_timer_->setInterval(500); // 500ms for metric updates
  connect(metrics_update_timer_, &QTimer::timeout, this, &SportsAnalyticsDashboard::UpdateRealTimeMetrics);
  
  // Initialize default configuration
  dashboard_settings_["theme"] = "default";
  dashboard_settings_["auto_refresh"] = true;
  dashboard_settings_["refresh_interval"] = global_refresh_interval_ms_;
  dashboard_settings_["enable_animations"] = true;
  dashboard_settings_["cache_enabled"] = true;
  dashboard_settings_["cache_duration"] = 300; // 5 minutes
  
  // Initialize data source status
  data_source_status_[AnalyticsDataSource::LiveGame] = false;
  data_source_status_[AnalyticsDataSource::VideoAnalysis] = false;
  data_source_status_[AnalyticsDataSource::HistoricalData] = false;
  data_source_status_[AnalyticsDataSource::PlayerStats] = false;
  data_source_status_[AnalyticsDataSource::FormationData] = false;
  data_source_status_[AnalyticsDataSource::TriangleDefenseData] = false;
  data_source_status_[AnalyticsDataSource::MELScores] = false;
  data_source_status_[AnalyticsDataSource::SupersetDatasets] = false;
  
  SetupUI();
  CreateDefaultLayout();
  
  qInfo() << "Sports Analytics Dashboard initialized";
}

SportsAnalyticsDashboard::~SportsAnalyticsDashboard()
{
  Shutdown();
}

bool SportsAnalyticsDashboard::Initialize(SportsIntegrationCoordinator* coordinator,
                                        SupersetPanelIntegration* superset_integration)
{
  if (is_initialized_) {
    qWarning() << "Sports Analytics Dashboard already initialized";
    return true;
  }

  integration_coordinator_ = coordinator;
  superset_integration_ = superset_integration;

  if (integration_coordinator_) {
    // Connect to integration coordinator events
    connect(integration_coordinator_, &SportsIntegrationCoordinator::FormationDetected,
            this, &SportsAnalyticsDashboard::OnFormationDetected);
    connect(integration_coordinator_, &SportsIntegrationCoordinator::FormationUpdated,
            this, &SportsAnalyticsDashboard::OnFormationUpdated);
    connect(integration_coordinator_, &SportsIntegrationCoordinator::RealTimeDataReceived,
            this, &SportsAnalyticsDashboard::OnRealTimeDataReceived);

    // Get Triangle Defense sync from coordinator
    triangle_defense_sync_ = integration_coordinator_->GetTriangleDefenseSync();
    if (triangle_defense_sync_) {
      connect(triangle_defense_sync_, &TriangleDefenseSync::TriangleCallRecommended,
              this, &SportsAnalyticsDashboard::OnTriangleCallRecommended);
      connect(triangle_defense_sync_, &TriangleDefenseSync::MELResultsUpdated,
              this, &SportsAnalyticsDashboard::OnMELResultsUpdated);
    }

    // Get timeline sync from coordinator
    timeline_sync_ = integration_coordinator_->GetVideoTimelineSync();
    if (timeline_sync_) {
      connect(timeline_sync_, &VideoTimelineSync::PositionChanged,
              this, &SportsAnalyticsDashboard::OnVideoPositionChanged);
      connect(timeline_sync_, &VideoTimelineSync::MetadataUpdated,
              this, &SportsAnalyticsDashboard::OnVideoMetadataUpdated);
    }

    qInfo() << "Sports Integration Coordinator connected to Analytics Dashboard";
  }

  if (superset_integration_) {
    // Connect to Superset integration events
    connect(superset_integration_, &SupersetPanelIntegration::DataUpdated,
            this, &SportsAnalyticsDashboard::OnSupersetDataUpdated);
    connect(superset_integration_, &SupersetPanelIntegration::ConnectionStatusChanged,
            this, &SportsAnalyticsDashboard::OnSupersetConnectionStatusChanged);

    qInfo() << "Superset Panel Integration connected to Analytics Dashboard";
  }

  // Setup data sources
  SetupDataSources();
  SetupRealTimeMetrics();
  SetupSupersetIntegration();

  // Load default widgets
  LoadDefaultWidgets();

  // Start timers
  if (auto_refresh_enabled_) {
    refresh_timer_->start();
    data_update_timer_->start();
    metrics_update_timer_->start();
  }

  is_initialized_ = true;
  qInfo() << "Sports Analytics Dashboard initialization completed";

  return true;
}

void SportsAnalyticsDashboard::Shutdown()
{
  if (!is_initialized_) {
    return;
  }

  qInfo() << "Shutting down Sports Analytics Dashboard";

  // Stop timers
  if (refresh_timer_ && refresh_timer_->isActive()) {
    refresh_timer_->stop();
  }
  if (data_update_timer_ && data_update_timer_->isActive()) {
    data_update_timer_->stop();
  }
  if (metrics_update_timer_ && metrics_update_timer_->isActive()) {
    metrics_update_timer_->stop();
  }

  // Stop widget timers
  for (auto timer : widget_timers_) {
    if (timer && timer->isActive()) {
      timer->stop();
    }
  }

  // Save current layout
  if (!current_layout_.layout_id.isEmpty()) {
    SaveDashboardLayout(current_layout_.layout_name, "Auto-saved on shutdown");
  }

  // Disconnect from Superset
  DisconnectFromSuperset();

  // Clear widget data
  {
    QMutexLocker locker(&data_mutex_);
    cached_data_.clear();
    analytics_queries_.clear();
  }

  // Clear metrics
  {
    QMutexLocker locker(&metrics_mutex_);
    real_time_metrics_.clear();
  }

  is_initialized_ = false;
  qInfo() << "Sports Analytics Dashboard shutdown complete";
}

void SportsAnalyticsDashboard::SetDashboardMode(AnalyticsDashboardMode mode)
{
  if (current_mode_ != mode) {
    AnalyticsDashboardMode previous_mode = current_mode_;
    current_mode_ = mode;

    // Update UI based on mode
    switch (mode) {
      case AnalyticsDashboardMode::RealTimeAnalysis:
        SetupRealTimeFormationAnalysis();
        break;
      case AnalyticsDashboardMode::PostGameAnalysis:
        SetupPostGameAnalysis();
        break;
      case AnalyticsDashboardMode::SeasonAnalysis:
        SetupSeasonAnalysis();
        break;
      case AnalyticsDashboardMode::PlayerComparison:
        SetupPlayerComparison();
        break;
      case AnalyticsDashboardMode::TeamPerformance:
        SetupTeamPerformance();
        break;
      case AnalyticsDashboardMode::FormationAnalysis:
        SetupFormationAnalysis();
        break;
      case AnalyticsDashboardMode::TriangleDefenseMetrics:
        SetupTriangleDefenseMetrics();
        break;
      case AnalyticsDashboardMode::MELAnalytics:
        SetupMELAnalytics();
        break;
      case AnalyticsDashboardMode::PredictiveAnalytics:
        SetupPredictiveAnalytics();
        break;
      case AnalyticsDashboardMode::CustomDashboard:
        SetupCustomDashboard();
        break;
    }

    // Update mode selector
    if (mode_selector_) {
      mode_selector_->setCurrentIndex(static_cast<int>(mode));
    }

    emit DashboardModeChanged(mode);
    
    qInfo() << "Dashboard mode changed from" << static_cast<int>(previous_mode) 
            << "to" << static_cast<int>(mode);
  }
}

bool SportsAnalyticsDashboard::LoadDashboardLayout(const QString& layout_id)
{
  QString layout_file = config_directory_ + "/layouts/" + layout_id + ".json";
  
  if (!QFile::exists(layout_file)) {
    qWarning() << "Dashboard layout file not found:" << layout_file;
    return false;
  }

  DashboardLayout layout = LoadLayoutFromFile(layout_file);
  if (layout.layout_id.isEmpty()) {
    qWarning() << "Failed to load dashboard layout from:" << layout_file;
    return false;
  }

  current_layout_ = layout;
  
  // Clear existing widgets
  for (auto widget : widgets_) {
    if (widget) {
      widget->deleteLater();
    }
  }
  widgets_.clear();
  charts_.clear();
  chart_views_.clear();

  // Create widgets from layout
  for (const DashboardWidget& widget_config : layout.widgets) {
    QWidget* widget = nullptr;
    
    switch (widget_config.chart_type) {
      case ChartType::LineChart:
      case ChartType::BarChart:
      case ChartType::ScatterPlot:
      case ChartType::PieChart:
      case ChartType::AreaChart:
        widget = CreateChartWidget(widget_config);
        break;
      default:
        if (widget_config.widget_type == "metric") {
          // Create metric widget if we have corresponding metric data
          if (real_time_metrics_.contains(widget_config.widget_id)) {
            widget = CreateMetricWidget(real_time_metrics_[widget_config.widget_id]);
          }
        } else if (widget_config.widget_type == "table") {
          widget = CreateTableWidget(widget_config);
        } else if (widget_config.widget_type == "text") {
          widget = CreateTextWidget(widget_config);
        } else {
          widget = CreateCustomWidget(widget_config);
        }
        break;
    }
    
    if (widget) {
      widgets_[widget_config.widget_id] = widget;
      
      // Setup widget timer if needed
      if (widget_config.is_real_time && widget_config.refresh_interval_ms > 0) {
        QTimer* widget_timer = new QTimer(this);
        widget_timer->setInterval(widget_config.refresh_interval_ms);
        connect(widget_timer, &QTimer::timeout, [this, widget_config]() {
          RefreshWidget(widget_config.widget_id);
        });
        widget_timers_[widget_config.widget_id] = widget_timer;
        widget_timer->start();
      }
    }
  }

  ConfigureWidgetLayout();
  
  qInfo() << "Dashboard layout loaded:" << layout_id;
  return true;
}

bool SportsAnalyticsDashboard::SaveDashboardLayout(const QString& layout_name, const QString& description)
{
  DashboardLayout layout;
  layout.layout_id = QUuid::createUuid().toString();
  layout.layout_name = layout_name;
  layout.layout_description = description;
  layout.dashboard_mode = current_mode_;
  layout.dashboard_size = size();
  layout.background_color = palette().color(QPalette::Window);
  layout.theme_name = dashboard_theme_;
  layout.auto_refresh = auto_refresh_enabled_;
  layout.global_refresh_interval_ms = global_refresh_interval_ms_;
  layout.created_at = QDateTime::currentDateTime();
  layout.modified_at = QDateTime::currentDateTime();

  // Collect widget configurations
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    const QString& widget_id = it.key();
    QWidget* widget = it.value();
    
    if (widget) {
      DashboardWidget widget_config;
      widget_config.widget_id = widget_id;
      widget_config.widget_title = widget->windowTitle();
      widget_config.widget_geometry = QRectF(widget->geometry());
      widget_config.is_visible = widget->isVisible();
      widget_config.created_at = QDateTime::currentDateTime();
      widget_config.last_updated = QDateTime::currentDateTime();
      
      // Get widget-specific configuration
      if (auto custom_widget = qobject_cast<DashboardWidgetBase*>(widget)) {
        QJsonObject config = custom_widget->GetWidgetConfig();
        widget_config.data_config = config["data_config"].toObject();
        widget_config.visual_config = config["visual_config"].toObject();
      }
      
      layout.widgets.append(widget_config);
    }
  }

  // Save layout to file
  QString layouts_dir = config_directory_ + "/layouts";
  QDir().mkpath(layouts_dir);
  
  QString layout_file = layouts_dir + "/" + layout.layout_id + ".json";
  SaveLayoutToFile(layout, layout_file);

  current_layout_ = layout;
  
  qInfo() << "Dashboard layout saved:" << layout_name << "ID:" << layout.layout_id;
  return true;
}

QString SportsAnalyticsDashboard::CreateWidget(const QString& title, ChartType chart_type,
                                              AnalyticsDataSource data_source, const QJsonObject& config)
{
  DashboardWidget widget_config;
  widget_config.widget_id = GenerateWidgetId();
  widget_config.widget_title = title;
  widget_config.widget_type = "chart";
  widget_config.chart_type = chart_type;
  widget_config.data_source = data_source;
  widget_config.data_config = config;
  widget_config.widget_geometry = QRectF(10, 10, 400, 300);
  widget_config.is_real_time = (data_source == AnalyticsDataSource::LiveGame);
  widget_config.refresh_interval_ms = widget_config.is_real_time ? 5000 : 30000;
  widget_config.is_visible = true;
  widget_config.z_order = widgets_.size();
  widget_config.created_at = QDateTime::currentDateTime();
  widget_config.last_updated = QDateTime::currentDateTime();

  QWidget* widget = CreateChartWidget(widget_config);
  if (widget) {
    widgets_[widget_config.widget_id] = widget;
    
    // Setup widget timer if needed
    if (widget_config.is_real_time) {
      QTimer* widget_timer = new QTimer(this);
      widget_timer->setInterval(widget_config.refresh_interval_ms);
      connect(widget_timer, &QTimer::timeout, [this, widget_config]() {
        RefreshWidget(widget_config.widget_id);
      });
      widget_timers_[widget_config.widget_id] = widget_timer;
      widget_timer->start();
    }
    
    emit WidgetAdded(widget_config.widget_id);
    
    qDebug() << "Widget created:" << widget_config.widget_id << "title:" << title;
    return widget_config.widget_id;
  }

  qWarning() << "Failed to create widget:" << title;
  return QString();
}

bool SportsAnalyticsDashboard::UpdateWidget(const QString& widget_id, const QJsonObject& config)
{
  if (!widgets_.contains(widget_id)) {
    qWarning() << "Widget not found for update:" << widget_id;
    return false;
  }

  QWidget* widget = widgets_[widget_id];
  if (auto custom_widget = qobject_cast<DashboardWidgetBase*>(widget)) {
    custom_widget->SetWidgetConfig(config);
    emit WidgetUpdated(widget_id);
    qDebug() << "Widget updated:" << widget_id;
    return true;
  }

  qWarning() << "Widget does not support configuration updates:" << widget_id;
  return false;
}

void SportsAnalyticsDashboard::RemoveWidget(const QString& widget_id)
{
  if (widgets_.contains(widget_id)) {
    QWidget* widget = widgets_[widget_id];
    widgets_.remove(widget_id);
    
    // Stop widget timer
    if (widget_timers_.contains(widget_id)) {
      QTimer* timer = widget_timers_[widget_id];
      timer->stop();
      widget_timers_.remove(widget_id);
      delete timer;
    }
    
    // Remove from charts
    if (charts_.contains(widget_id)) {
      charts_.remove(widget_id);
    }
    if (chart_views_.contains(widget_id)) {
      chart_views_.remove(widget_id);
    }
    
    if (widget) {
      widget->deleteLater();
    }
    
    emit WidgetRemoved(widget_id);
    qDebug() << "Widget removed:" << widget_id;
  }
}

void SportsAnalyticsDashboard::AddRealTimeMetric(const RealTimeMetrics& metric)
{
  QMutexLocker locker(&metrics_mutex_);
  
  real_time_metrics_[metric.metric_id] = metric;
  
  // Create metric widget if not exists
  if (!widgets_.contains(metric.metric_id)) {
    QWidget* metric_widget = CreateMetricWidget(metric);
    if (metric_widget) {
      widgets_[metric.metric_id] = metric_widget;
    }
  }
  
  qDebug() << "Real-time metric added:" << metric.metric_id << "value:" << metric.current_value;
}

void SportsAnalyticsDashboard::UpdateRealTimeMetric(const QString& metric_id, double value)
{
  QMutexLocker locker(&metrics_mutex_);
  
  if (real_time_metrics_.contains(metric_id)) {
    RealTimeMetrics& metric = real_time_metrics_[metric_id];
    
    // Calculate change
    metric.previous_value = metric.current_value;
    metric.current_value = value;
    
    if (metric.previous_value != 0.0) {
      metric.change_percentage = ((value - metric.previous_value) / metric.previous_value) * 100.0;
    }
    
    // Determine trend direction
    if (value > metric.previous_value) {
      metric.trend_direction = "up";
    } else if (value < metric.previous_value) {
      metric.trend_direction = "down";
    } else {
      metric.trend_direction = "stable";
    }
    
    metric.last_updated = QDateTime::currentDateTime();
    
    // Update UI elements
    if (metric_labels_.contains(metric_id)) {
      QLabel* label = metric_labels_[metric_id];
      label->setText(FormatMetricValue(value, metric.unit));
      label->setStyleSheet(QString("color: %1").arg(GetMetricColor(metric).name()));
    }
    
    if (metric_progress_bars_.contains(metric_id)) {
      QProgressBar* progress = metric_progress_bars_[metric_id];
      double normalized_value = (value - metric.min_value) / (metric.max_value - metric.min_value);
      progress->setValue(static_cast<int>(normalized_value * 100));
    }
    
    qDebug() << "Real-time metric updated:" << metric_id << "value:" << value << "change:" << metric.change_percentage << "%";
  }
}

bool SportsAnalyticsDashboard::ConfigureSuperset(const SupersetIntegration& config)
{
  superset_config_ = config;
  
  if (!superset_config_.superset_url.isEmpty()) {
    ConnectToSuperset();
    return true;
  }
  
  return false;
}

void SportsAnalyticsDashboard::SyncWithSuperset()
{
  if (!superset_config_.is_connected) {
    qWarning() << "Cannot sync with Superset: not connected";
    return;
  }
  
  // Sync dashboard configuration to Superset
  SyncDashboardToSuperset();
  
  // Load available datasets and charts
  SendSupersetRequest("/api/v1/dataset/");
  SendSupersetRequest("/api/v1/chart/");
  SendSupersetRequest("/api/v1/dashboard/");
  
  qInfo() << "Syncing with Superset initiated";
}

QJsonArray SportsAnalyticsDashboard::ExecuteAnalyticsQuery(const AnalyticsQuery& query)
{
  QElapsedTimer timer;
  timer.start();
  
  QJsonArray results;
  
  // Check cache first
  QString cache_key = QString("%1_%2_%3").arg(query.query_id, query.sql_query, query.time_range);
  
  if (query.is_cached && cached_data_.contains(cache_key)) {
    QDateTime cache_time = cached_data_[cache_key + "_timestamp"].toArray()[0].toString();
    if (cache_time.addSecs(query.cache_duration_minutes * 60) > QDateTime::currentDateTime()) {
      results = cached_data_[cache_key];
      qDebug() << "Analytics query served from cache:" << query.query_id;
      return results;
    }
  }
  
  // Execute query based on data source
  switch (query.data_source) {
    case AnalyticsDataSource::FormationData:
      results = QueryFormationData(query);
      break;
    case AnalyticsDataSource::TriangleDefenseData:
      results = QueryTriangleDefenseData(query);
      break;
    case AnalyticsDataSource::MELScores:
      results = QueryMELData(query);
      break;
    case AnalyticsDataSource::VideoAnalysis:
      results = QueryVideoData(query);
      break;
    case AnalyticsDataSource::HistoricalData:
      results = QueryHistoricalData(query);
      break;
    case AnalyticsDataSource::SupersetDatasets:
      results = QuerySupersetData(query);
      break;
    default:
      qWarning() << "Unsupported data source for query:" << static_cast<int>(query.data_source);
      break;
  }
  
  // Cache results if enabled
  if (query.is_cached && !results.isEmpty()) {
    QMutexLocker locker(&data_mutex_);
    cached_data_[cache_key] = results;
    QJsonArray timestamp_array;
    timestamp_array.append(QDateTime::currentDateTime().toString());
    cached_data_[cache_key + "_timestamp"] = timestamp_array;
  }
  
  qint64 execution_time = timer.elapsed();
  query_execution_times_[query.query_id] = execution_time;
  
  emit AnalyticsQueryCompleted(query.query_id, results);
  
  qDebug() << "Analytics query executed:" << query.query_id 
           << "results:" << results.size() 
           << "time:" << execution_time << "ms";
  
  return results;
}

QJsonObject SportsAnalyticsDashboard::ExportDashboardData(const QString& format) const
{
  QJsonObject export_data;
  export_data["format"] = format;
  export_data["exported_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  export_data["dashboard_mode"] = static_cast<int>(current_mode_);
  export_data["layout"] = current_layout_.layout_id;
  
  // Export widget data
  QJsonArray widgets_data;
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    QJsonObject widget_data;
    widget_data["widget_id"] = it.key();
    widget_data["widget_title"] = it.value()->windowTitle();
    widget_data["widget_type"] = "chart"; // Default type
    
    // Export chart data if available
    if (charts_.contains(it.key())) {
      // Chart data export would go here
      widget_data["has_chart_data"] = true;
    }
    
    widgets_data.append(widget_data);
  }
  export_data["widgets"] = widgets_data;
  
  // Export metrics data
  QJsonArray metrics_data;
  QMutexLocker locker(&metrics_mutex_);
  for (auto it = real_time_metrics_.constBegin(); it != real_time_metrics_.constEnd(); ++it) {
    QJsonObject metric_data;
    metric_data["metric_id"] = it.key();
    metric_data["metric_name"] = it.value().metric_name;
    metric_data["current_value"] = it.value().current_value;
    metric_data["change_percentage"] = it.value().change_percentage;
    metric_data["trend_direction"] = it.value().trend_direction;
    metric_data["last_updated"] = it.value().last_updated.toString(Qt::ISODate);
    metrics_data.append(metric_data);
  }
  export_data["metrics"] = metrics_data;
  
  // Export statistics
  export_data["statistics"] = GetDashboardStatistics();
  
  return export_data;
}

bool SportsAnalyticsDashboard::ImportDashboardConfig(const QJsonObject& config)
{
  try {
    if (config.contains("dashboard_mode")) {
      SetDashboardMode(static_cast<AnalyticsDashboardMode>(config["dashboard_mode"].toInt()));
    }
    
    if (config.contains("auto_refresh")) {
      SetAutoRefreshEnabled(config["auto_refresh"].toBool());
    }
    
    if (config.contains("refresh_interval")) {
      SetGlobalRefreshInterval(config["refresh_interval"].toInt());
    }
    
    if (config.contains("theme")) {
      dashboard_theme_ = config["theme"].toString();
    }
    
    // Import widget configurations
    if (config.contains("widgets")) {
      QJsonArray widgets_array = config["widgets"].toArray();
      for (const QJsonValue& widget_value : widgets_array) {
        QJsonObject widget_config = widget_value.toObject();
        
        QString title = widget_config["title"].toString();
        ChartType chart_type = static_cast<ChartType>(widget_config["chart_type"].toInt());
        AnalyticsDataSource data_source = static_cast<AnalyticsDataSource>(widget_config["data_source"].toInt());
        QJsonObject data_config = widget_config["data_config"].toObject();
        
        CreateWidget(title, chart_type, data_source, data_config);
      }
    }
    
    // Import Superset configuration
    if (config.contains("superset")) {
      QJsonObject superset_config = config["superset"].toObject();
      SupersetIntegration integration;
      integration.superset_url = superset_config["url"].toString();
      integration.api_token = superset_config["api_token"].toString();
      integration.database_id = superset_config["database_id"].toString();
      integration.dataset_id = superset_config["dataset_id"].toString();
      ConfigureSuperset(integration);
    }
    
    qInfo() << "Dashboard configuration imported successfully";
    return true;
    
  } catch (const std::exception& e) {
    qCritical() << "Error importing dashboard configuration:" << e.what();
    return false;
  }
}

void SportsAnalyticsDashboard::SetAutoRefreshEnabled(bool enabled)
{
  auto_refresh_enabled_ = enabled;
  
  if (auto_refresh_checkbox_) {
    auto_refresh_checkbox_->setChecked(enabled);
  }
  
  if (enabled) {
    if (refresh_timer_) refresh_timer_->start();
    if (data_update_timer_) data_update_timer_->start();
    if (metrics_update_timer_) metrics_update_timer_->start();
  } else {
    if (refresh_timer_) refresh_timer_->stop();
    if (data_update_timer_) data_update_timer_->stop();
    if (metrics_update_timer_) metrics_update_timer_->stop();
  }
  
  qInfo() << "Auto-refresh" << (enabled ? "enabled" : "disabled");
}

void SportsAnalyticsDashboard::SetGlobalRefreshInterval(int interval_ms)
{
  global_refresh_interval_ms_ = qMax(1000, interval_ms); // Minimum 1 second
  
  if (refresh_timer_) {
    refresh_timer_->setInterval(global_refresh_interval_ms_);
  }
  
  if (refresh_interval_spinbox_) {
    refresh_interval_spinbox_->setValue(global_refresh_interval_ms_ / 1000);
  }
  
  qInfo() << "Global refresh interval set to:" << global_refresh_interval_ms_ << "ms";
}

void SportsAnalyticsDashboard::AddCustomDataSource(const QString& source_name, const QJsonObject& config)
{
  custom_data_sources_[source_name] = config;
  available_data_sources_.append(source_name);
  data_source_connections_[source_name] = false;
  
  qInfo() << "Custom data source added:" << source_name;
}

QStringList SportsAnalyticsDashboard::GetAvailableChartTypes() const
{
  return QStringList() << "Line Chart" << "Bar Chart" << "Scatter Plot" << "Pie Chart" 
                       << "Area Chart" << "Heat Map" << "Radar Chart" << "Box Plot"
                       << "Histogram" << "Tree Map" << "Sankey" << "Network Graph"
                       << "Geo Map" << "Custom Visualization";
}

QJsonObject SportsAnalyticsDashboard::GetDashboardStatistics() const
{
  QJsonObject stats;
  
  stats["total_widgets"] = widgets_.size();
  stats["active_charts"] = charts_.size();
  stats["real_time_metrics"] = real_time_metrics_.size();
  stats["dashboard_mode"] = static_cast<int>(current_mode_);
  stats["auto_refresh_enabled"] = auto_refresh_enabled_;
  stats["refresh_interval_ms"] = global_refresh_interval_ms_;
  stats["total_refresh_count"] = dashboard_refresh_count_;
  stats["average_render_time_ms"] = dashboard_refresh_count_ > 0 ? 
    (static_cast<double>(total_dashboard_render_time_) / dashboard_refresh_count_) : 0.0;
  
  // Data source statistics
  QJsonObject data_sources;
  for (auto it = data_source_status_.constBegin(); it != data_source_status_.constEnd(); ++it) {
    data_sources[QString::number(static_cast<int>(it.key()))] = it.value();
  }
  stats["data_sources"] = data_sources;
  
  // Cache statistics
  QJsonObject cache_stats;
  cache_stats["cached_queries"] = cached_data_.size() / 2; // Divide by 2 because we store timestamps too
  cache_stats["cache_directory"] = data_cache_directory_;
  stats["cache"] = cache_stats;
  
  // Superset integration statistics
  QJsonObject superset_stats;
  superset_stats["connected"] = superset_config_.is_connected;
  superset_stats["last_sync"] = superset_config_.last_sync.toString(Qt::ISODate);
  superset_stats["sync_status"] = superset_config_.sync_status;
  stats["superset"] = superset_stats;
  
  return stats;
}

// Slot implementations
void SportsAnalyticsDashboard::OnFormationDetected(const FormationData& formation)
{
  // Update formation-related metrics
  UpdateRealTimeMetric("formation_count", real_time_metrics_.value("formation_count").current_value + 1);
  UpdateRealTimeMetric("formation_confidence", formation.confidence * 100.0);
  
  // Update formation analysis widgets
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    if (auto formation_widget = qobject_cast<FormationMetricsWidget*>(it.value())) {
      QJsonArray formation_data;
      QJsonObject formation_obj;
      formation_obj["formation_id"] = formation.formation_id;
      formation_obj["type"] = static_cast<int>(formation.type);
      formation_obj["confidence"] = formation.confidence;
      formation_obj["timestamp"] = formation.video_timestamp;
      formation_data.append(formation_obj);
      formation_widget->UpdateData(formation_data);
    }
  }
  
  qDebug() << "Formation detected in dashboard:" << formation.formation_id 
           << "type:" << static_cast<int>(formation.type);
}

void SportsAnalyticsDashboard::OnFormationUpdated(const FormationData& formation)
{
  // Update formation confidence metric
  UpdateRealTimeMetric("formation_confidence", formation.confidence * 100.0);
  
  qDebug() << "Formation updated in dashboard:" << formation.formation_id;
}

void SportsAnalyticsDashboard::OnTriangleCallRecommended(TriangleCall call, const FormationData& formation)
{
  // Update Triangle Defense metrics
  UpdateRealTimeMetric("triangle_calls_total", real_time_metrics_.value("triangle_calls_total").current_value + 1);
  
  QString call_type_metric = QString("triangle_call_%1").arg(static_cast<int>(call));
  UpdateRealTimeMetric(call_type_metric, real_time_metrics_.value(call_type_metric).current_value + 1);
  
  // Update Triangle Defense widgets
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    if (auto triangle_widget = qobject_cast<TriangleDefenseWidget*>(it.value())) {
      QJsonArray call_data;
      QJsonObject call_obj;
      call_obj["call_type"] = static_cast<int>(call);
      call_obj["formation_id"] = formation.formation_id;
      call_obj["confidence"] = formation.confidence;
      call_obj["timestamp"] = QDateTime::currentMSecsSinceEpoch();
      call_data.append(call_obj);
      triangle_widget->UpdateData(call_data);
    }
  }
  
  qDebug() << "Triangle call recommended in dashboard:" << static_cast<int>(call)
           << "for formation:" << formation.formation_id;
}

void SportsAnalyticsDashboard::OnMELResultsUpdated(const QString& formation_id, const MELResult& results)
{
  // Update M.E.L. metrics
  UpdateRealTimeMetric("mel_combined_score", results.combined_score);
  UpdateRealTimeMetric("mel_making_score", results.making_score);
  UpdateRealTimeMetric("mel_efficiency_score", results.efficiency_score);
  UpdateRealTimeMetric("mel_logical_score", results.logical_score);
  
  // Update M.E.L. widgets
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    if (auto mel_widget = qobject_cast<MELAnalyticsWidget*>(it.value())) {
      QJsonArray mel_data;
      QJsonObject mel_obj;
      mel_obj["formation_id"] = formation_id;
      mel_obj["combined_score"] = results.combined_score;
      mel_obj["making_score"] = results.making_score;
      mel_obj["efficiency_score"] = results.efficiency_score;
      mel_obj["logical_score"] = results.logical_score;
      mel_obj["timestamp"] = results.processing_timestamp;
      mel_data.append(mel_obj);
      mel_widget->UpdateData(mel_data);
    }
  }
  
  qDebug() << "M.E.L. results updated in dashboard for formation:" << formation_id
           << "combined score:" << results.combined_score;
}

void SportsAnalyticsDashboard::OnVideoPositionChanged(qint64 position)
{
  // Update video position metric
  UpdateRealTimeMetric("video_position", static_cast<double>(position) / 1000.0); // Convert to seconds
  
  // Trigger refresh of time-based widgets
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    // Check if widget is time-based and needs refresh
    QString widget_id = it.key();
    if (widget_id.contains("timeline") || widget_id.contains("temporal")) {
      RefreshWidget(widget_id);
    }
  }
}

void SportsAnalyticsDashboard::OnVideoMetadataUpdated(const QJsonObject& metadata)
{
  // Update video metadata metrics
  if (metadata.contains("duration")) {
    UpdateRealTimeMetric("video_duration", metadata["duration"].toDouble() / 1000.0);
  }
  if (metadata.contains("fps")) {
    UpdateRealTimeMetric("video_fps", metadata["fps"].toDouble());
  }
}

void SportsAnalyticsDashboard::OnRealTimeDataReceived(const QJsonObject& data)
{
  ProcessRealTimeData(data);
}

void SportsAnalyticsDashboard::OnSupersetDataUpdated(const QString& dataset_id, const QJsonArray& data)
{
  QString cache_key = QString("superset_%1").arg(dataset_id);
  
  {
    QMutexLocker locker(&data_mutex_);
    cached_data_[cache_key] = data;
  }
  
  // Update widgets that use this dataset
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    QString widget_id = it.key();
    if (widget_id.contains(dataset_id)) {
      RefreshWidget(widget_id);
    }
  }
  
  qDebug() << "Superset data updated for dataset:" << dataset_id << "records:" << data.size();
}

void SportsAnalyticsDashboard::OnSupersetConnectionStatusChanged(bool connected)
{
  superset_config_.is_connected = connected;
  superset_config_.sync_status = connected ? "connected" : "disconnected";
  
  if (connected) {
    emit DataSourceConnected(AnalyticsDataSource::SupersetDatasets);
    data_source_status_[AnalyticsDataSource::SupersetDatasets] = true;
  } else {
    emit DataSourceDisconnected(AnalyticsDataSource::SupersetDatasets);
    data_source_status_[AnalyticsDataSource::SupersetDatasets] = false;
  }
  
  qInfo() << "Superset connection status changed:" << (connected ? "connected" : "disconnected");
}

void SportsAnalyticsDashboard::OnWidgetClicked(const QString& widget_id)
{
  selected_widget_id_ = widget_id;
  
  // Highlight selected widget
  if (widgets_.contains(widget_id)) {
    QWidget* widget = widgets_[widget_id];
    widget->setStyleSheet("border: 2px solid #007acc;");
    
    // Remove highlight from other widgets
    for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
      if (it.key() != widget_id && it.value()) {
        it.value()->setStyleSheet("");
      }
    }
  }
  
  emit WidgetClicked(widget_id);
}

void SportsAnalyticsDashboard::OnDashboardModeChanged(AnalyticsDashboardMode mode)
{
  SetDashboardMode(mode);
}

void SportsAnalyticsDashboard::OnLayoutChanged()
{
  // Save current layout changes
  if (!current_layout_.layout_id.isEmpty()) {
    current_layout_.modified_at = QDateTime::currentDateTime();
    // Auto-save layout changes
    SaveDashboardLayout(current_layout_.layout_name, "Auto-saved layout changes");
  }
}

// Timer slot implementations
void SportsAnalyticsDashboard::OnRefreshTimer()
{
  QElapsedTimer timer;
  timer.start();
  
  RefreshAllWidgets();
  
  qint64 render_time = timer.elapsed();
  total_dashboard_render_time_ += render_time;
  dashboard_refresh_count_++;
  
  qDebug() << "Dashboard refreshed in" << render_time << "ms";
}

void SportsAnalyticsDashboard::OnDataUpdateTimer()
{
  UpdateDashboardMetrics();
  CalculateAdvancedMetrics();
}

void SportsAnalyticsDashboard::OnWidgetRefreshTimer()
{
  // Individual widget refresh is handled by widget-specific timers
}

void SportsAnalyticsDashboard::RefreshAllWidgets()
{
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    RefreshWidget(it.key());
  }
}

void SportsAnalyticsDashboard::RefreshWidget(const QString& widget_id)
{
  if (!widgets_.contains(widget_id)) {
    return;
  }
  
  QWidget* widget = widgets_[widget_id];
  if (auto custom_widget = qobject_cast<DashboardWidgetBase*>(widget)) {
    custom_widget->RefreshWidget();
  } else if (chart_views_.contains(widget_id)) {
    // Refresh chart widget
    QChartView* chart_view = chart_views_[widget_id];
    if (chart_view && charts_.contains(widget_id)) {
      QChart* chart = charts_[widget_id];
      
      // Get fresh data for chart
      AnalyticsQuery query;
      query.query_id = widget_id;
      query.data_source = AnalyticsDataSource::FormationData; // Default
      query.time_range = "1h";
      query.is_cached = true;
      query.cache_duration_minutes = 5;
      
      QJsonArray data = ExecuteAnalyticsQuery(query);
      UpdateChartData(chart, data);
    }
  }
  
  qDebug() << "Widget refreshed:" << widget_id;
}

void SportsAnalyticsDashboard::OnSupersetReplyFinished()
{
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  if (!reply) {
    return;
  }
  
  if (reply->error() == QNetworkReply::NoError) {
    QByteArray response_data = reply->readAll();
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(response_data, &parse_error);
    
    if (parse_error.error == QJsonParseError::NoError) {
      ProcessSupersetResponse(doc.object());
    } else {
      qWarning() << "Failed to parse Superset response:" << parse_error.errorString();
    }
  } else {
    qWarning() << "Superset request failed:" << reply->errorString();
    emit SupersetSyncFailed(reply->errorString());
  }
  
  reply->deleteLater();
}

void SportsAnalyticsDashboard::UpdateRealTimeMetrics()
{
  // This method is called by timer to update real-time metric displays
  QMutexLocker locker(&metrics_mutex_);
  
  for (auto it = real_time_metrics_.constBegin(); it != real_time_metrics_.constEnd(); ++it) {
    const QString& metric_id = it.key();
    const RealTimeMetrics& metric = it.value();
    
    // Check for threshold violations
    if (metric.is_critical) {
      if (metric.current_value > metric.max_value || metric.current_value < metric.min_value) {
        emit MetricThresholdExceeded(metric_id, metric.current_value, 
                                   metric.current_value > metric.max_value ? metric.max_value : metric.min_value);
      }
    }
  }
}

// Private method implementations
void SportsAnalyticsDashboard::SetupUI()
{
  setWindowTitle("Sports Analytics Dashboard");
  setMinimumSize(1200, 800);
  
  // Main layout
  main_layout_ = new QVBoxLayout(this);
  main_layout_->setContentsMargins(5, 5, 5, 5);
  main_layout_->setSpacing(5);
  
  // Setup toolbar
  SetupLayouts();
  
  // Setup main content area
  main_tabs_ = new QTabWidget(this);
  main_layout_->addWidget(main_tabs_);
  
  // Setup panels
  analytics_panel_ = new QWidget();
  real_time_panel_ = new QWidget();
  configuration_panel_ = new QWidget();
  superset_panel_ = new QWidget();
  
  main_tabs_->addTab(analytics_panel_, "Analytics");
  main_tabs_->addTab(real_time_panel_, "Real-Time");
  main_tabs_->addTab(configuration_panel_, "Configuration");
  main_tabs_->addTab(superset_panel_, "Superset");
  
  // Setup chart area
  SetupCharts();
}

void SportsAnalyticsDashboard::SetupLayouts()
{
  // Toolbar layout
  toolbar_layout_ = new QHBoxLayout();
  toolbar_layout_->setContentsMargins(0, 0, 0, 0);
  
  // Mode selector
  mode_selector_ = new QComboBox();
  mode_selector_->addItems(QStringList() << "Real-Time Analysis" << "Post-Game Analysis" 
                          << "Season Analysis" << "Player Comparison" << "Team Performance"
                          << "Formation Analysis" << "Triangle Defense" << "M.E.L. Analytics"
                          << "Predictive Analytics" << "Custom Dashboard");
  connect(mode_selector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) { OnDashboardModeChanged(static_cast<AnalyticsDashboardMode>(index)); });
  
  // Layout selector
  layout_selector_ = new QComboBox();
  layout_selector_->addItem("Default Layout");
  
  // Control buttons
  refresh_button_ = new QPushButton("Refresh");
  connect(refresh_button_, &QPushButton::clicked, this, &SportsAnalyticsDashboard::RefreshAllWidgets);
  
  export_button_ = new QPushButton("Export");
  connect(export_button_, &QPushButton::clicked, [this]() {
    QString filename = QFileDialog::getSaveFileName(this, "Export Dashboard Data", "", "JSON Files (*.json)");
    if (!filename.isEmpty()) {
      QJsonObject data = ExportDashboardData("json");
      QJsonDocument doc(data);
      QFile file(filename);
      if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qInfo() << "Dashboard data exported to:" << filename;
      }
    }
  });
  
  settings_button_ = new QPushButton("Settings");
  
  // Auto-refresh controls
  auto_refresh_checkbox_ = new QCheckBox("Auto Refresh");
  auto_refresh_checkbox_->setChecked(auto_refresh_enabled_);
  connect(auto_refresh_checkbox_, &QCheckBox::toggled, this, &SportsAnalyticsDashboard::SetAutoRefreshEnabled);
  
  refresh_interval_spinbox_ = new QSpinBox();
  refresh_interval_spinbox_->setRange(1, 300);
  refresh_interval_spinbox_->setValue(global_refresh_interval_ms_ / 1000);
  refresh_interval_spinbox_->setSuffix(" sec");
  connect(refresh_interval_spinbox_, QOverload<int>::of(&QSpinBox::valueChanged),
          [this](int value) { SetGlobalRefreshInterval(value * 1000); });
  
  // Add widgets to toolbar
  toolbar_layout_->addWidget(new QLabel("Mode:"));
  toolbar_layout_->addWidget(mode_selector_);
  toolbar_layout_->addWidget(new QLabel("Layout:"));
  toolbar_layout_->addWidget(layout_selector_);
  toolbar_layout_->addStretch();
  toolbar_layout_->addWidget(auto_refresh_checkbox_);
  toolbar_layout_->addWidget(refresh_interval_spinbox_);
  toolbar_layout_->addWidget(refresh_button_);
  toolbar_layout_->addWidget(export_button_);
  toolbar_layout_->addWidget(settings_button_);
  
  main_layout_->addLayout(toolbar_layout_);
}

void SportsAnalyticsDashboard::SetupCharts()
{
  // Dashboard layout for charts
  dashboard_layout_ = new QGridLayout();
  dashboard_layout_->setSpacing(10);
  analytics_panel_->setLayout(dashboard_layout_);
  
  // Real-time panel layout
  QVBoxLayout* real_time_layout = new QVBoxLayout(real_time_panel_);
  
  // Configuration panel layout
  QVBoxLayout* config_layout = new QVBoxLayout(configuration_panel_);
  
  // Superset panel setup
  superset_web_view_ = new QWebEngineView(superset_panel_);
  QVBoxLayout* superset_layout = new QVBoxLayout(superset_panel_);
  superset_layout->addWidget(superset_web_view_);
}

void SportsAnalyticsDashboard::SetupRealTimeMetrics()
{
  // Initialize default real-time metrics
  RealTimeMetrics formation_count;
  formation_count.metric_id = "formation_count";
  formation_count.metric_name = "Formations Detected";
  formation_count.metric_category = "Formation Analysis";
  formation_count.unit = "count";
  formation_count.min_value = 0;
  formation_count.max_value = 1000;
  AddRealTimeMetric(formation_count);
  
  RealTimeMetrics formation_confidence;
  formation_confidence.metric_id = "formation_confidence";
  formation_confidence.metric_name = "Formation Confidence";
  formation_confidence.metric_category = "Formation Analysis";
  formation_confidence.unit = "%";
  formation_confidence.min_value = 0;
  formation_confidence.max_value = 100;
  AddRealTimeMetric(formation_confidence);
  
  RealTimeMetrics triangle_calls;
  triangle_calls.metric_id = "triangle_calls_total";
  triangle_calls.metric_name = "Triangle Calls Total";
  triangle_calls.metric_category = "Triangle Defense";
  triangle_calls.unit = "count";
  triangle_calls.min_value = 0;
  triangle_calls.max_value = 500;
  AddRealTimeMetric(triangle_calls);
  
  RealTimeMetrics mel_score;
  mel_score.metric_id = "mel_combined_score";
  mel_score.metric_name = "M.E.L. Combined Score";
  mel_score.metric_category = "M.E.L. Analytics";
  mel_score.unit = "score";
  mel_score.min_value = 0;
  mel_score.max_value = 100;
  AddRealTimeMetric(mel_score);
  
  qInfo() << "Real-time metrics initialized";
}

void SportsAnalyticsDashboard::SetupSupersetIntegration()
{
  // Initialize Superset web channel for bidirectional communication
  web_channel_ = new QWebChannel(this);
  superset_web_view_->page()->setWebChannel(web_channel_);
  
  // Register dashboard object for JavaScript access
  web_channel_->registerObject("dashboard", this);
  
  qInfo() << "Superset integration setup completed";
}

void SportsAnalyticsDashboard::SetupDataSources()
{
  // Initialize data source connections
  available_data_sources_ << "Live Game" << "Video Analysis" << "Historical Data"
                         << "Player Stats" << "Formation Data" << "Triangle Defense"
                         << "M.E.L. Scores" << "External APIs" << "Superset Datasets";
  
  for (const QString& source : available_data_sources_) {
    data_source_connections_[source] = false;
  }
  
  qInfo() << "Data sources initialized:" << available_data_sources_.size() << "sources";
}

void SportsAnalyticsDashboard::CreateDefaultLayout()
{
  current_layout_.layout_id = "default";
  current_layout_.layout_name = "Default Dashboard";
  current_layout_.layout_description = "Default sports analytics dashboard layout";
  current_layout_.dashboard_mode = AnalyticsDashboardMode::RealTimeAnalysis;
  current_layout_.created_at = QDateTime::currentDateTime();
  current_layout_.modified_at = QDateTime::currentDateTime();
}

void SportsAnalyticsDashboard::LoadDefaultWidgets()
{
  // Create default formation analysis widget
  QString formation_widget_id = CreateWidget("Formation Analysis", ChartType::LineChart, 
                                            AnalyticsDataSource::FormationData);
  
  // Create default Triangle Defense widget  
  QString triangle_widget_id = CreateWidget("Triangle Defense Metrics", ChartType::BarChart,
                                           AnalyticsDataSource::TriangleDefenseData);
  
  // Create default M.E.L. analytics widget
  QString mel_widget_id = CreateWidget("M.E.L. Score Trends", ChartType::AreaChart,
                                      AnalyticsDataSource::MELScores);
  
  qInfo() << "Default widgets loaded:" << formation_widget_id << triangle_widget_id << mel_widget_id;
}

void SportsAnalyticsDashboard::ConfigureWidgetLayout()
{
  // Arrange widgets in grid layout
  int row = 0, col = 0;
  const int max_cols = 3;
  
  for (auto it = widgets_.constBegin(); it != widgets_.constEnd(); ++it) {
    QWidget* widget = it.value();
    if (widget && dashboard_layout_) {
      dashboard_layout_->addWidget(widget, row, col);
      
      col++;
      if (col >= max_cols) {
        col = 0;
        row++;
      }
    }
  }
}

QWidget* SportsAnalyticsDashboard::CreateChartWidget(const DashboardWidget& widget)
{
  QWidget* container = new QWidget();
  container->setWindowTitle(widget.widget_title);
  container->setMinimumSize(400, 300);
  
  QVBoxLayout* layout = new QVBoxLayout(container);
  
  // Create chart based on type
  QChart* chart = nullptr;
  QJsonArray sample_data; // Would be populated with real data
  
  switch (widget.chart_type) {
    case ChartType::LineChart:
      chart = CreateLineChart(widget, sample_data);
      break;
    case ChartType::BarChart:
      chart = CreateBarChart(widget, sample_data);
      break;
    case ChartType::ScatterPlot:
      chart = CreateScatterChart(widget, sample_data);
      break;
    case ChartType::PieChart:
      chart = CreatePieChart(widget, sample_data);
      break;
    case ChartType::AreaChart:
      chart = CreateAreaChart(widget, sample_data);
      break;
    default:
      chart = CreateLineChart(widget, sample_data); // Default fallback
      break;
  }
  
  if (chart) {
    QChartView* chart_view = new QChartView(chart);
    chart_view->setRenderHint(QPainter::Antialiasing);
    SetupChartInteractivity(chart_view);
    
    layout->addWidget(chart_view);
    
    charts_[widget.widget_id] = chart;
    chart_views_[widget.widget_id] = chart_view;
    
    ApplyChartTheme(chart, dashboard_theme_);
  }
  
  return container;
}

QWidget* SportsAnalyticsDashboard::CreateMetricWidget(const RealTimeMetrics& metric)
{
  QWidget* container = new QWidget();
  container->setWindowTitle(metric.metric_name);
  container->setMinimumSize(200, 100);
  
  QVBoxLayout* layout = new QVBoxLayout(container);
  
  // Metric value label
  QLabel* value_label = new QLabel(FormatMetricValue(metric.current_value, metric.unit));
  value_label->setStyleSheet("font-size: 24px; font-weight: bold;");
  value_label->setAlignment(Qt::AlignCenter);
  metric_labels_[metric.metric_id] = value_label;
  
  // Metric name label
  QLabel* name_label = new QLabel(metric.metric_name);
  name_label->setAlignment(Qt::AlignCenter);
  
  // Progress bar for normalized value
  QProgressBar* progress = new QProgressBar();
  progress->setRange(0, 100);
  double normalized_value = (metric.current_value - metric.min_value) / (metric.max_value - metric.min_value);
  progress->setValue(static_cast<int>(normalized_value * 100));
  metric_progress_bars_[metric.metric_id] = progress;
  
  layout->addWidget(name_label);
  layout->addWidget(value_label);
  layout->addWidget(progress);
  
  return container;
}

QChart* SportsAnalyticsDashboard::CreateLineChart(const DashboardWidget& widget, const QJsonArray& data)
{
  QChart* chart = new QChart();
  chart->setTitle(widget.widget_title);
  
  QLineSeries* series = new QLineSeries();
  series->setName("Data Series");
  
  // Populate with sample or real data
  for (int i = 0; i < 10; ++i) {
    series->append(i, QRandomGenerator::global()->bounded(100));
  }
  
  chart->addSeries(series);
  chart->createDefaultAxes();
  
  return chart;
}

QChart* SportsAnalyticsDashboard::CreateBarChart(const DashboardWidget& widget, const QJsonArray& data)
{
  QChart* chart = new QChart();
  chart->setTitle(widget.widget_title);
  
  QBarSeries* series = new QBarSeries();
  QBarSet* set = new QBarSet("Data");
  
  // Populate with sample data
  for (int i = 0; i < 5; ++i) {
    *set << QRandomGenerator::global()->bounded(100);
  }
  
  series->append(set);
  chart->addSeries(series);
  chart->createDefaultAxes();
  
  return chart;
}

QString SportsAnalyticsDashboard::GenerateWidgetId()
{
  return QString("widget_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(widgets_.size());
}

QColor SportsAnalyticsDashboard::GetMetricColor(const RealTimeMetrics& metric)
{
  if (metric.trend_direction == "up") {
    return QColor(34, 139, 34); // Forest Green
  } else if (metric.trend_direction == "down") {
    return QColor(220, 20, 60); // Crimson
  } else {
    return QColor(105, 105, 105); // Dim Gray
  }
}

QString SportsAnalyticsDashboard::FormatMetricValue(double value, const QString& unit)
{
  if (unit == "%") {
    return QString("%1%").arg(value, 0, 'f', 1);
  } else if (unit == "count") {
    return QString::number(static_cast<int>(value));
  } else {
    return QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);
  }
}

// Additional placeholder implementations for completeness
QWidget* SportsAnalyticsDashboard::CreateTableWidget(const DashboardWidget& widget) 
{
  QTableWidget* table = new QTableWidget();
  table->setWindowTitle(widget.widget_title);
  return table;
}

QWidget* SportsAnalyticsDashboard::CreateTextWidget(const DashboardWidget& widget)
{
  QTextEdit* text = new QTextEdit();
  text->setWindowTitle(widget.widget_title);
  text->setText("Text widget content");
  return text;
}

QWidget* SportsAnalyticsDashboard::CreateCustomWidget(const DashboardWidget& widget)
{
  QLabel* label = new QLabel("Custom Widget");
  label->setWindowTitle(widget.widget_title);
  return label;
}

// Placeholder implementations for various analysis modes
void SportsAnalyticsDashboard::SetupRealTimeFormationAnalysis() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupPostGameAnalysis() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupSeasonAnalysis() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupPlayerComparison() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupTeamPerformance() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupFormationAnalysis() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupTriangleDefenseMetrics() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupMELAnalytics() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupPredictiveAnalytics() { /* Implementation */ }
void SportsAnalyticsDashboard::SetupCustomDashboard() { /* Implementation */ }

// Placeholder data query implementations
QJsonArray SportsAnalyticsDashboard::QueryFormationData(const AnalyticsQuery& query) { return QJsonArray(); }
QJsonArray SportsAnalyticsDashboard::QueryTriangleDefenseData(const AnalyticsQuery& query) { return QJsonArray(); }
QJsonArray SportsAnalyticsDashboard::QueryMELData(const AnalyticsQuery& query) { return QJsonArray(); }
QJsonArray SportsAnalyticsDashboard::QueryVideoData(const AnalyticsQuery& query) { return QJsonArray(); }
QJsonArray SportsAnalyticsDashboard::QueryHistoricalData(const AnalyticsQuery& query) { return QJsonArray(); }
QJsonArray SportsAnalyticsDashboard::QuerySupersetData(const AnalyticsQuery& query) { return QJsonArray(); }

// Additional method implementations would continue here...
// [Additional placeholder implementations for remaining methods]

} // namespace olive
