/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Superset Dashboard Integration Panel
  Embeds Apache Superset dashboards with video synchronization
***/

#ifndef SUPERSETPANEL_H
#define SUPERSETPANEL_H

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>

#include "panel/panelwidget.h"

namespace olive {

class SupersetBridge;
class KafkaPublisher;
class TriangleDefenseSync;

/**
 * @brief Panel for embedding Apache Superset dashboards with video synchronization
 */
class SupersetPanel : public PanelWidget
{
  Q_OBJECT

public:
  explicit SupersetPanel(const QString& object_name = QStringLiteral("SupersetPanel"),
                        QWidget* parent = nullptr);

  virtual ~SupersetPanel() override;

  /**
   * @brief Load specific Superset dashboard by ID
   */
  void LoadDashboard(const QString& dashboard_id);

  /**
   * @brief Set authentication token for Superset access
   */
  void SetAuthToken(const QString& token);

  /**
   * @brief Synchronize with video timestamp
   */
  void SyncWithVideoTimestamp(qint64 timestamp_ms);

  /**
   * @brief Set Triangle Defense formation data for overlay
   */
  void UpdateFormationData(const QJsonObject& formation_data);

  /**
   * @brief Get current dashboard URL
   */
  QString GetCurrentDashboardUrl() const;

public slots:
  /**
   * @brief Handle video playback events
   */
  void OnVideoPlaybackChanged(bool playing);
  void OnVideoSeekChanged(qint64 position);
  void OnVideoRateChanged(double rate);

  /**
   * @brief Handle Triangle Defense events
   */
  void OnTriangleDefenseCall(const QString& call_type, const QJsonObject& data);
  void OnFormationClassified(const QString& formation_type, double confidence);

  /**
   * @brief Handle dashboard interactions
   */
  void OnDashboardLoaded();
  void OnDashboardError(const QString& error);

protected:
  virtual void showEvent(QShowEvent* event) override;
  virtual void hideEvent(QHideEvent* event) override;
  virtual void resizeEvent(QResizeEvent* event) override;

private slots:
  void RefreshDashboard();
  void OnWebPageLoadFinished(bool success);
  void OnWebPageLoadProgress(int progress);
  void OnJavaScriptMessage(const QJsonObject& message);
  void OnSyncTimerTimeout();

private:
  void SetupUI();
  void SetupWebEngine();
  void SetupConnections();
  void InjectJavaScript();
  void SendMessageToSuperset(const QJsonObject& message);
  void UpdateSyncStatus(const QString& status);

  // UI Components
  QVBoxLayout* main_layout_;
  QHBoxLayout* toolbar_layout_;
  QSplitter* main_splitter_;
  
  QComboBox* dashboard_selector_;
  QPushButton* refresh_button_;
  QPushButton* sync_button_;
  QLabel* sync_status_label_;
  QLineEdit* timestamp_display_;
  
  // Web Engine
  QWebEngineView* web_view_;
  QWebEnginePage* web_page_;
  QWebChannel* web_channel_;
  SupersetBridge* superset_bridge_;

  // Integration Services
  KafkaPublisher* kafka_publisher_;
  TriangleDefenseSync* triangle_sync_;
  
  // Synchronization
  QTimer* sync_timer_;
  qint64 current_video_timestamp_;
  bool is_video_playing_;
  double video_playback_rate_;
  
  // Configuration
  QString superset_base_url_;
  QString auth_token_;
  QString current_dashboard_id_;
  QJsonObject current_formation_data_;
  
  // State Management
  bool dashboard_loaded_;
  bool sync_enabled_;
  int sync_interval_ms_;
};

/**
 * @brief Bridge class for JavaScript communication with Superset
 */
class SupersetBridge : public QObject
{
  Q_OBJECT

public:
  explicit SupersetBridge(QObject* parent = nullptr);

public slots:
  /**
   * @brief Receive messages from Superset JavaScript
   */
  void receiveMessage(const QString& message);

  /**
   * @brief Send commands to Superset
   */
  void updateTimestamp(qint64 timestamp);
  void updateFormationData(const QString& data);
  void highlightTriangleCall(const QString& call_type);

signals:
  /**
   * @brief Emit messages back to C++
   */
  void messageReceived(const QJsonObject& message);
  void dashboardInteraction(const QString& event_type, const QJsonObject& data);
};

} // namespace olive

#endif // SUPERSETPANEL_H
