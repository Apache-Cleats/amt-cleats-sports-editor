/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Sports Analytics Dashboard
  Comprehensive sports data analytics and visualization dashboard with Apache Superset integration
***/

#ifndef SPORTSANALYTICSDASHBOARD_H
#define SPORTSANALYTICSDASHBOARD_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTableWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTimer>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QScatterSeries>
#include <QPieSeries>
#include <QAreaSeries>
#include <QValueAxis>
#include <QCategoryAxis>
#include <QDateTimeAxis>
#include <QLegend>
#include <QWebEngineView>
#include <QWebChannel>
#include <QJSEngine>

#include "triangle_defense_sync.h"
#include "video_timeline_sync.h"
#include "superset_panel_integration.h"

namespace olive {

// Forward declarations
class SportsIntegrationCoordinator;
class FormationOverlay;

/**
 * @brief Analytics dashboard view modes
 */
enum class AnalyticsDashboardMode {
  RealTimeAnalysis,     // Live game analysis
  PostGameAnalysis,     // Post-game review and statistics
  SeasonAnalysis,       // Season-long trends and statistics
  PlayerComparison,     // Individual player comparison
  TeamPerformance,      // Team performance metrics
  FormationAnalysis,    // Formation effectiveness analysis
  TriangleDefenseMetrics, // Triangle Defense system analytics
  MELAnalytics,         // M.E.L. scoring system analytics
  PredictiveAnalytics,  // AI-powered predictions and insights
  CustomDashboard       // User-defined custom analytics
};

/**
 * @brief Analytics data source types
 */
enum class AnalyticsDataSource {
  LiveGame,             // Real-time game data
  VideoAnalysis,        // Video-based analysis data
  HistoricalData,       // Historical game/season data
  PlayerStats,          // Individual player statistics
  FormationData,        // Formation detection data
  TriangleDefenseData,  // Triangle Defense metrics
  MELScores,           // M.E.L. scoring data
  ExternalAPIs,        // Third-party sports APIs
  SupersetDatasets,    // Apache Superset datasets
  CustomData           // User-uploaded custom data
};

/**
 * @brief Chart types for visualization
 */
enum class ChartType {
  LineChart,           // Time series and trend analysis
  BarChart,           // Categorical comparisons
  ScatterPlot,        // Correlation analysis
  PieChart,           // Proportional data
  AreaChart,          // Cumulative data
  HeatMap,            // Density and correlation matrices
  RadarChart,         // Multi-dimensional comparison
  BoxPlot,            // Statistical distribution
  Histogram,          // Frequency distribution
  TreeMap,            // Hierarchical data
  Sankey,             // Flow diagrams
  NetworkGraph,       // Relationship mapping
  GeoMap,             // Spatial/field position data
  CustomVisualization // Plugin-based custom charts
};

/**
 * @brief Real-time analytics metrics
 */
struct RealTimeMetrics {
  QString metric_id;
  QString metric_name;
  QString metric_category;
  double current_value;
  double previous_value;
  double change_percentage;
  QString trend_direction; // "up", "down", "stable"
  QDateTime last_updated;
  QString unit;
  double min_value;
  double max_value;
  bool is_critical;
  
  RealTimeMetrics() : current_value(0.0), previous_value(0.0), change_percentage(0.0),
                      trend_direction("stable"), min_value(0.0), max_value(100.0),
                      is_critical(false) {}
};

/**
 * @brief Analytics dashboard widget configuration
 */
struct DashboardWidget {
  QString widget_id;
  QString widget_title;
  QString widget_type; // "chart", "metric", "table", "text", "custom"
  ChartType chart_type;
  AnalyticsDataSource data_source;
  QJsonObject data_config;
  QJsonObject visual_config;
  QRectF widget_geometry;
  bool is_real_time;
  int refresh_interval_ms;
  bool is_visible;
  int z_order;
  QDateTime created_at;
  QDateTime last_updated;
  
  DashboardWidget() : chart_type(ChartType::LineChart),
                      data_source(AnalyticsDataSource::LiveGame),
                      is_real_time(false), refresh_interval_ms(5000),
                      is_visible(true), z_order(0) {}
};

/**
 * @brief Dashboard layout configuration
 */
struct DashboardLayout {
  QString layout_id;
  QString layout_name;
  QString layout_description;
  AnalyticsDashboardMode dashboard_mode;
  QList<DashboardWidget> widgets;
  QSizeF dashboard_size;
  QColor background_color;
  QString theme_name;
  bool auto_refresh;
  int global_refresh_interval_ms;
  QJsonObject layout_metadata;
  QDateTime created_at;
  QDateTime modified_at;
  
  DashboardLayout() : dashboard_mode(AnalyticsDashboardMode::RealTimeAnalysis),
                      background_color(QColor(240, 240, 240)),
                      theme_name("default"), auto_refresh(true),
                      global_refresh_interval_ms(10000) {}
};

/**
 * @brief Superset integration configuration
 */
struct SupersetIntegration {
  QString superset_url;
  QString api_token;
  QString database_id;
  QString dataset_id;
  QStringList available_charts;
  QStringList available_dashboards;
  QJsonObject connection_config;
  bool is_connected;
  QDateTime last_sync;
  QString sync_status;
  
  SupersetIntegration() : is_connected(false), sync_status("disconnected") {}
};

/**
 * @brief Analytics data query configuration
 */
struct AnalyticsQuery {
  QString query_id;
  QString query_name;
  AnalyticsDataSource data_source;
  QString sql_query;
  QJsonObject query_parameters;
  QStringList required_fields;
  QString aggregation_type; // "sum", "avg", "count", "min", "max"
  QString time_range;
  QString group_by_field;
  QJsonObject filters;
  bool is_cached;
  int cache_duration_minutes;
  QDateTime created_at;
  
  AnalyticsQuery() : data_source(AnalyticsDataSource::LiveGame),
                     aggregation_type("avg"), time_range("1h"),
                     is_cached(true), cache_duration_minutes(15) {}
};

/**
 * @brief Sports analytics dashboard main class
 */
class SportsAnalyticsDashboard : public QWidget
{
  Q_OBJECT

public:
  explicit SportsAnalyticsDashboard(QWidget* parent = nullptr);
  virtual ~SportsAnalyticsDashboard();

  /**
   * @brief Initialize dashboard system
   */
  bool Initialize(SportsIntegrationCoordinator* coordinator = nullptr,
                  SupersetPanelIntegration* superset_integration = nullptr);

  /**
   * @brief Shutdown dashboard system
   */
  void Shutdown();

  /**
   * @brief Set dashboard mode
   */
  void SetDashboardMode(AnalyticsDashboardMode mode);

  /**
   * @brief Get current dashboard mode
   */
  AnalyticsDashboardMode GetDashboardMode() const { return current_mode_; }

  /**
   * @brief Load dashboard layout
   */
  bool LoadDashboardLayout(const QString& layout_id);

  /**
   * @brief Save current dashboard layout
   */
  bool SaveDashboardLayout(const QString& layout_name, const QString& description = "");

  /**
   * @brief Create new dashboard widget
   */
  QString CreateWidget(const QString& title, ChartType chart_type,
                      AnalyticsDataSource data_source, const QJsonObject& config = QJsonObject());

  /**
   * @brief Update widget configuration
   */
  bool UpdateWidget(const QString& widget_id, const QJsonObject& config);

  /**
   * @brief Remove widget from dashboard
   */
  void RemoveWidget(const QString& widget_id);

  /**
   * @brief Add real-time metric
   */
  void AddRealTimeMetric(const RealTimeMetrics& metric);

  /**
   * @brief Update real-time metric value
   */
  void UpdateRealTimeMetric(const QString& metric_id, double value);

  /**
   * @brief Configure Superset integration
   */
  bool ConfigureSuperset(const SupersetIntegration& config);

  /**
   * @brief Sync data with Superset
   */
  void SyncWithSuperset();

  /**
   * @brief Execute analytics query
   */
  QJsonArray ExecuteAnalyticsQuery(const AnalyticsQuery& query);

  /**
   * @brief Export dashboard data
   */
  QJsonObject ExportDashboardData(const QString& format = "json") const;

  /**
   * @brief Import dashboard configuration
   */
  bool ImportDashboardConfig(const QJsonObject& config);

  /**
   * @brief Set auto-refresh enabled
   */
  void SetAutoRefreshEnabled(bool enabled);

  /**
   * @brief Set global refresh interval
   */
  void SetGlobalRefreshInterval(int interval_ms);

  /**
   * @brief Add custom data source
   */
  void AddCustomDataSource(const QString& source_name, const QJsonObject& config);

  /**
   * @brief Get available chart types
   */
  QStringList GetAvailableChartTypes() const;

  /**
   * @brief Get dashboard statistics
   */
  QJsonObject GetDashboardStatistics() const;

public slots:
  /**
   * @brief Handle formation data updates
   */
  void OnFormationDetected(const FormationData& formation);
  void OnFormationUpdated(const FormationData& formation);

  /**
   * @brief Handle Triangle Defense updates
   */
  void OnTriangleCallRecommended(TriangleCall call, const FormationData& formation);
  void OnDefensiveMetricsUpdated(const QJsonObject& metrics);

  /**
   * @brief Handle M.E.L. score updates
   */
  void OnMELResultsUpdated(const QString& formation_id, const MELResult& results);

  /**
   * @brief Handle video timeline updates
   */
  void OnVideoPositionChanged(qint64 position);
  void OnVideoMetadataUpdated(const QJsonObject& metadata);

  /**
   * @brief Handle real-time data updates
   */
  void OnRealTimeDataReceived(const QJsonObject& data);

  /**
   * @brief Handle Superset events
   */
  void OnSupersetDataUpdated(const QString& dataset_id, const QJsonArray& data);
  void OnSupersetConnectionStatusChanged(bool connected);

  /**
   * @brief Handle user interactions
   */
  void OnWidgetClicked(const QString& widget_id);
  void OnDashboardModeChanged(AnalyticsDashboardMode mode);
  void OnLayoutChanged();

signals:
  /**
   * @brief Dashboard event signals
   */
  void DashboardModeChanged(AnalyticsDashboardMode mode);
  void WidgetAdded(const QString& widget_id);
  void WidgetRemoved(const QString& widget_id);
  void WidgetUpdated(const QString& widget_id);
  void DataSourceConnected(AnalyticsDataSource source);
  void DataSourceDisconnected(AnalyticsDataSource source);

  /**
   * @brief Analytics event signals
   */
  void MetricThresholdExceeded(const QString& metric_id, double value, double threshold);
  void AnalyticsQueryCompleted(const QString& query_id, const QJsonArray& results);
  void AnalyticsQueryFailed(const QString& query_id, const QString& error);

  /**
   * @brief Superset integration signals
   */
  void SupersetSyncCompleted();
  void SupersetSyncFailed(const QString& error);
  void SupersetChartLoaded(const QString& chart_id);

protected:
  void resizeEvent(QResizeEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private slots:
  void OnRefreshTimer();
  void OnDataUpdateTimer();
  void OnWidgetRefreshTimer();
  void RefreshAllWidgets();
  void RefreshWidget(const QString& widget_id);
  void OnSupersetReplyFinished();
  void UpdateRealTimeMetrics();

private:
  void SetupUI();
  void SetupLayouts();
  void SetupCharts();
  void SetupRealTimeMetrics();
  void SetupSupersetIntegration();
  void SetupDataSources();
  
  void CreateDefaultLayout();
  void LoadDefaultWidgets();
  void ConfigureWidgetLayout();
  void UpdateWidgetPositions();
  
  QWidget* CreateChartWidget(const DashboardWidget& widget);
  QWidget* CreateMetricWidget(const RealTimeMetrics& metric);
  QWidget* CreateTableWidget(const DashboardWidget& widget);
  QWidget* CreateTextWidget(const DashboardWidget& widget);
  QWidget* CreateCustomWidget(const DashboardWidget& widget);
  
  QChart* CreateLineChart(const DashboardWidget& widget, const QJsonArray& data);
  QChart* CreateBarChart(const DashboardWidget& widget, const QJsonArray& data);
  QChart* CreateScatterChart(const DashboardWidget& widget, const QJsonArray& data);
  QChart* CreatePieChart(const DashboardWidget& widget, const QJsonArray& data);
  QChart* CreateAreaChart(const DashboardWidget& widget, const QJsonArray& data);
  
  void UpdateChartData(QChart* chart, const QJsonArray& data);
  void ApplyChartTheme(QChart* chart, const QString& theme_name);
  void ConfigureChartAxes(QChart* chart, const QJsonObject& config);
  void SetupChartInteractivity(QChartView* chart_view);
  
  QJsonArray QueryFormationData(const AnalyticsQuery& query);
  QJsonArray QueryTriangleDefenseData(const AnalyticsQuery& query);
  QJsonArray QueryMELData(const AnalyticsQuery& query);
  QJsonArray QueryVideoData(const AnalyticsQuery& query);
  QJsonArray QueryHistoricalData(const AnalyticsQuery& query);
  QJsonArray QuerySupersetData(const AnalyticsQuery& query);
  
  void ProcessRealTimeData(const QJsonObject& data);
  void UpdateDashboardMetrics();
  void CalculateAdvancedMetrics();
  void GenerateInsights();
  
  bool ValidateWidgetConfig(const DashboardWidget& widget);
  QString GenerateWidgetId();
  QColor GetMetricColor(const RealTimeMetrics& metric);
  QString FormatMetricValue(double value, const QString& unit);
  
  void SaveLayoutToFile(const DashboardLayout& layout, const QString& filename);
  DashboardLayout LoadLayoutFromFile(const QString& filename);
  void ExportWidgetData(const QString& widget_id, const QString& format);
  void ImportWidgetData(const QString& widget_id, const QJsonObject& data);
  
  void ConnectToSuperset();
  void DisconnectFromSuperset();
  void SendSupersetRequest(const QString& endpoint, const QJsonObject& data = QJsonObject());
  void ProcessSupersetResponse(const QJsonObject& response);
  void SyncDashboardToSuperset();
  void LoadSupersetDashboard(const QString& dashboard_id);
  
  QJsonObject CalculateFormationEffectiveness() const;
  QJsonObject CalculateTriangleDefenseEfficiency() const;
  QJsonObject CalculateMELTrends() const;
  QJsonObject CalculatePlayerPerformanceMetrics() const;
  QJsonObject CalculateTeamPerformanceMetrics() const;
  QJsonObject GeneratePredictiveInsights() const;
  
  void SetupRealTimeFormationAnalysis();
  void SetupPostGameAnalysis();
  void SetupSeasonAnalysis();
  void SetupPlayerComparison();
  void SetupTeamPerformance();
  void SetupFormationAnalysis();
  void SetupTriangleDefenseMetrics();
  void SetupMELAnalytics();
  void SetupPredictiveAnalytics();
  void SetupCustomDashboard();

  // Core components
  SportsIntegrationCoordinator* integration_coordinator_;
  SupersetPanelIntegration* superset_integration_;
  TriangleDefenseSync* triangle_defense_sync_;
  VideoTimelineSync* timeline_sync_;
  
  // Dashboard state
  AnalyticsDashboardMode current_mode_;
  DashboardLayout current_layout_;
  bool is_initialized_;
  bool auto_refresh_enabled_;
  int global_refresh_interval_ms_;
  
  // UI components
  QVBoxLayout* main_layout_;
  QHBoxLayout* toolbar_layout_;
  QGridLayout* dashboard_layout_;
  QTabWidget* main_tabs_;
  QSplitter* main_splitter_;
  
  // Toolbar widgets
  QComboBox* mode_selector_;
  QComboBox* layout_selector_;
  QPushButton* refresh_button_;
  QPushButton* export_button_;
  QPushButton* settings_button_;
  QCheckBox* auto_refresh_checkbox_;
  QSpinBox* refresh_interval_spinbox_;
  
  // Dashboard panels
  QWidget* analytics_panel_;
  QWidget* real_time_panel_;
  QWidget* configuration_panel_;
  QWidget* superset_panel_;
  
  // Chart components
  QMap<QString, QChartView*> chart_views_;
  QMap<QString, QChart*> charts_;
  QMap<QString, QWidget*> widgets_;
  
  // Real-time metrics
  mutable QMutex metrics_mutex_;
  QMap<QString, RealTimeMetrics> real_time_metrics_;
  QMap<QString, QLabel*> metric_labels_;
  QMap<QString, QProgressBar*> metric_progress_bars_;
  
  // Data management
  mutable QMutex data_mutex_;
  QMap<QString, AnalyticsQuery> analytics_queries_;
  QMap<QString, QJsonArray> cached_data_;
  QMap<AnalyticsDataSource, bool> data_source_status_;
  
  // Superset integration
  SupersetIntegration superset_config_;
  QNetworkAccessManager* network_manager_;
  QWebEngineView* superset_web_view_;
  QWebChannel* web_channel_;
  
  // Timers
  QTimer* refresh_timer_;
  QTimer* data_update_timer_;
  QTimer* metrics_update_timer_;
  QMap<QString, QTimer*> widget_timers_;
  
  // Configuration
  QString dashboard_theme_;
  QJsonObject dashboard_settings_;
  QString config_directory_;
  QString data_cache_directory_;
  
  // Widget interaction
  QString selected_widget_id_;
  QString hovered_widget_id_;
  QPointF last_mouse_position_;
  bool widget_drag_mode_;
  
  // Analytics engine
  QJSEngine* analytics_engine_;
  QJsonObject custom_metrics_;
  QJsonObject predictive_models_;
  
  // Performance monitoring
  QMap<QString, qint64> widget_render_times_;
  QMap<QString, qint64> query_execution_times_;
  qint64 total_dashboard_render_time_;
  int dashboard_refresh_count_;
  
  // Data sources
  QMap<QString, QJsonObject> custom_data_sources_;
  QStringList available_data_sources_;
  QMap<QString, bool> data_source_connections_;
};

/**
 * @brief Dashboard widget base class for custom widgets
 */
class DashboardWidgetBase : public QWidget
{
  Q_OBJECT

public:
  explicit DashboardWidgetBase(const DashboardWidget& config, QWidget* parent = nullptr);
  virtual ~DashboardWidgetBase() = default;

  virtual void UpdateData(const QJsonArray& data) = 0;
  virtual void RefreshWidget() = 0;
  virtual QJsonObject GetWidgetConfig() const = 0;
  virtual void SetWidgetConfig(const QJsonObject& config) = 0;

  QString GetWidgetId() const { return widget_config_.widget_id; }
  QString GetWidgetTitle() const { return widget_config_.widget_title; }
  
signals:
  void WidgetClicked(const QString& widget_id);
  void WidgetDataChanged(const QString& widget_id);
  void WidgetConfigChanged(const QString& widget_id);

protected:
  DashboardWidget widget_config_;
  QJsonArray current_data_;
  bool is_updating_;
};

/**
 * @brief Real-time formation metrics widget
 */
class FormationMetricsWidget : public DashboardWidgetBase
{
  Q_OBJECT

public:
  explicit FormationMetricsWidget(const DashboardWidget& config, QWidget* parent = nullptr);

  void UpdateData(const QJsonArray& data) override;
  void RefreshWidget() override;
  QJsonObject GetWidgetConfig() const override;
  void SetWidgetConfig(const QJsonObject& config) override;

private:
  void SetupUI();
  void UpdateFormationMetrics();
  void CalculateFormationEfficiency();

  QVBoxLayout* layout_;
  QLabel* formation_count_label_;
  QLabel* efficiency_label_;
  QProgressBar* confidence_progress_;
  QChart* formation_trend_chart_;
  QChartView* chart_view_;
};

/**
 * @brief Triangle Defense analytics widget
 */
class TriangleDefenseWidget : public DashboardWidgetBase
{
  Q_OBJECT

public:
  explicit TriangleDefenseWidget(const DashboardWidget& config, QWidget* parent = nullptr);

  void UpdateData(const QJsonArray& data) override;
  void RefreshWidget() override;
  QJsonObject GetWidgetConfig() const override;
  void SetWidgetConfig(const QJsonObject& config) override;

private:
  void SetupUI();
  void UpdateTriangleMetrics();
  void CalculateDefenseEffectiveness();

  QVBoxLayout* layout_;
  QLabel* call_accuracy_label_;
  QLabel* response_time_label_;
  QChart* defense_effectiveness_chart_;
  QChartView* chart_view_;
  QTableWidget* call_history_table_;
};

/**
 * @brief M.E.L. scoring analytics widget
 */
class MELAnalyticsWidget : public DashboardWidgetBase
{
  Q_OBJECT

public:
  explicit MELAnalyticsWidget(const DashboardWidget& config, QWidget* parent = nullptr);

  void UpdateData(const QJsonArray& data) override;
  void RefreshWidget() override;
  QJsonObject GetWidgetConfig() const override;
  void SetWidgetConfig(const QJsonObject& config) override;

private:
  void SetupUI();
  void UpdateMELScores();
  void CalculateMELTrends();

  QVBoxLayout* layout_;
  QLabel* combined_score_label_;
  QProgressBar* making_progress_;
  QProgressBar* efficiency_progress_;
  QProgressBar* logical_progress_;
  QChart* mel_trend_chart_;
  QChartView* chart_view_;
};

/**
 * @brief Utility functions for sports analytics
 */
namespace SportsAnalyticsUtils {

/**
 * @brief Calculate formation transition frequency
 */
QJsonObject CalculateFormationTransitions(const QJsonArray& formation_data);

/**
 * @brief Calculate player heat map data
 */
QJsonArray GeneratePlayerHeatMap(const QJsonArray& position_data);

/**
 * @brief Calculate team performance metrics
 */
QJsonObject CalculateTeamPerformance(const QJsonArray& game_data);

/**
 * @brief Generate predictive insights
 */
QJsonObject GeneratePredictiveInsights(const QJsonArray& historical_data);

/**
 * @brief Calculate formation effectiveness score
 */
double CalculateFormationEffectiveness(const FormationData& formation, const QJsonObject& context);

/**
 * @brief Generate time-based analytics
 */
QJsonArray GenerateTimeBasedAnalytics(const QJsonArray& data, const QString& time_field);

/**
 * @brief Calculate correlation matrix
 */
QJsonObject CalculateCorrelationMatrix(const QJsonArray& data, const QStringList& fields);

/**
 * @brief Generate performance percentiles
 */
QJsonObject CalculatePerformancePercentiles(const QJsonArray& data, const QString& metric_field);

/**
 * @brief Export analytics data to various formats
 */
bool ExportAnalyticsData(const QJsonArray& data, const QString& format, const QString& filename);

/**
 * @brief Validate analytics query syntax
 */
bool ValidateAnalyticsQuery(const AnalyticsQuery& query);

} // namespace SportsAnalyticsUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::AnalyticsDashboardMode)
Q_DECLARE_METATYPE(olive::AnalyticsDataSource)
Q_DECLARE_METATYPE(olive::ChartType)
Q_DECLARE_METATYPE(olive::RealTimeMetrics)
Q_DECLARE_METATYPE(olive::DashboardWidget)
Q_DECLARE_METATYPE(olive::DashboardLayout)
Q_DECLARE_METATYPE(olive::SupersetIntegration)
Q_DECLARE_METATYPE(olive::AnalyticsQuery)

#endif // SPORTSANALYTICSDASHBOARD_H
