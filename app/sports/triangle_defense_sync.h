/***
  Apache-Cleats Sports Editor
  Copyright (C) 2024 AnalyzeMyTeam

  Triangle Defense Synchronization Service
  Real-time M.E.L. pipeline integration with video timeline
***/

#ifndef TRIANGLEDEFENSESYNC_H
#define TRIANGLEDEFENSESYNC_H

#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QQueue>
#include <QMutex>
#include <QThread>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace olive {

/**
 * @brief Formation classification types
 */
enum class FormationType {
  Larry,          // Male tight formation
  Linda,          // Female tight formation  
  Rita,           // Female loose formation
  Ricky,          // Male loose formation
  Randy,          // Male very loose formation
  Pat,            // Neutral/unknown formation
  Unknown
};

/**
 * @brief Triangle Defense call types
 */
enum class TriangleCall {
  StrongSide,     // Defense shifts to strong side
  WeakSide,       // Defense shifts to weak side
  MiddleHash,     // Defense focuses on middle hash
  LeftHash,       // Defense shifts to left hash
  RightHash,      // Defense shifts to right hash
  RedZone,        // Red zone defensive alignment
  GoalLine,       // Goal line stand formation
  NoCall          // No specific call needed
};

/**
 * @brief M.E.L. stage results
 */
struct MELResult {
  double making_score;
  double efficiency_score;
  double logical_score;
  double combined_score;
  QString stage_status;
  qint64 processing_timestamp;
  QJsonObject detailed_metrics;
  
  MELResult() : making_score(0.0), efficiency_score(0.0), logical_score(0.0),
                combined_score(0.0), processing_timestamp(0) {}
};

/**
 * @brief Formation analysis data
 */
struct FormationData {
  QString formation_id;
  FormationType type;
  TriangleCall recommended_call;
  double confidence;
  qint64 video_timestamp;
  qint64 detection_timestamp;
  MELResult mel_results;
  QJsonObject player_positions;
  QJsonObject field_context;
  QString hash_position; // L, M, R
  QString field_zone;    // Red Zone, Midfield, etc.
  
  FormationData() : type(FormationType::Unknown), recommended_call(TriangleCall::NoCall),
                    confidence(0.0), video_timestamp(0), detection_timestamp(0) {}
};

/**
 * @brief Real-time coaching alert
 */
struct CoachingAlert {
  QString alert_id;
  QString alert_type;
  QString message;
  QString target_staff;
  int priority_level; // 1-5, 5 being critical
  qint64 alert_timestamp;
  qint64 video_timestamp;
  FormationData related_formation;
  QJsonObject context_data;
  bool acknowledged;
  
  CoachingAlert() : priority_level(1), alert_timestamp(0), video_timestamp(0),
                    acknowledged(false) {}
};

/**
 * @brief Triangle Defense synchronization service
 */
class TriangleDefenseSync : public QObject
{
  Q_OBJECT

public:
  explicit TriangleDefenseSync(QObject* parent = nullptr);
  virtual ~TriangleDefenseSync();

  /**
   * @brief Initialize connection to M.E.L. pipeline
   */
  bool Initialize(const QString& supabase_url, const QString& api_key,
                  const QString& websocket_url = QString());

  /**
   * @brief Shutdown service gracefully
   */
  void Shutdown();

  /**
   * @brief Get formation data for specific video timestamp
   */
  FormationData GetFormationAt(qint64 video_timestamp) const;

  /**
   * @brief Get formations within time range
   */
  QList<FormationData> GetFormationsInRange(qint64 start_timestamp, qint64 end_timestamp) const;

  /**
   * @brief Get active coaching alerts
   */
  QList<CoachingAlert> GetActiveAlerts() const;

  /**
   * @brief Acknowledge coaching alert
   */
  bool AcknowledgeAlert(const QString& alert_id);

  /**
   * @brief Get real-time M.E.L. pipeline status
   */
  QJsonObject GetPipelineStatus() const;

  /**
   * @brief Force refresh of formation data
   */
  void RefreshFormationData();

  /**
   * @brief Set video timeline synchronization
   */
  void SetVideoTimestamp(qint64 timestamp);

  /**
   * @brief Enable/disable real-time updates
   */
  void SetRealTimeMode(bool enabled);

  /**
   * @brief Get synchronization statistics
   */
  QJsonObject GetSyncStatistics() const;

public slots:
  /**
   * @brief Handle video playback events
   */
  void OnVideoPlaybackChanged(bool playing, qint64 position);
  void OnVideoSeekPerformed(qint64 new_position);
  void OnVideoRateChanged(double rate);

  /**
   * @brief Handle manual formation annotations
   */
  void OnManualFormationMarked(qint64 timestamp, FormationType type, double confidence);
  void OnTriangleCallOverride(qint64 timestamp, TriangleCall call, const QString& reason);

signals:
  /**
   * @brief Formation detection signals
   */
  void FormationDetected(const FormationData& formation);
  void FormationUpdated(const FormationData& formation);
  void FormationClassified(FormationType type, double confidence, qint64 timestamp);

  /**
   * @brief Triangle Defense signals
   */
  void TriangleCallRecommended(TriangleCall call, const FormationData& formation);
  void DefensiveShiftSuggested(const QString& shift_type, const QJsonObject& context);

  /**
   * @brief Coaching alert signals
   */
  void CoachingAlertReceived(const CoachingAlert& alert);
  void AlertPriorityChanged(const QString& alert_id, int new_priority);
  void CriticalAlertReceived(const CoachingAlert& alert);

  /**
   * @brief M.E.L. pipeline signals
   */
  void MELResultsUpdated(const QString& formation_id, const MELResult& results);
  void PipelineStatusChanged(const QString& stage, const QString& status);
  void PipelineError(const QString& error_message);

  /**
   * @brief Synchronization signals
   */
  void SyncStatusChanged(bool connected);
  void DataRefreshed(int formation_count, int alert_count);

private slots:
  void OnSupabaseDataReceived();
  void OnWebSocketConnected();
  void OnWebSocketDisconnected();
  void OnWebSocketMessageReceived(const QString& message);
  void OnNetworkReplyFinished();
  void OnSyncTimer();
  void OnCacheCleanupTimer();
  void OnHeartbeatTimer();

private:
  void SetupDatabase();
  void SetupNetworking();
  void SetupWebSocket();
  void SetupTimers();
  
  void LoadFormationData();
  void LoadCoachingAlerts();
  void UpdateFormationCache(const FormationData& formation);
  void UpdateAlertCache(const CoachingAlert& alert);
  
  bool FetchFormationsFromSupabase(qint64 start_time, qint64 end_time);
  bool FetchAlertsFromSupabase();
  bool UpdateSupabaseFormation(const FormationData& formation);
  
  void ProcessMELPipelineUpdate(const QJsonObject& update);
  void ProcessFormationDetection(const QJsonObject& detection);
  void ProcessCoachingAlert(const QJsonObject& alert);
  
  FormationType ParseFormationType(const QString& type_string) const;
  TriangleCall ParseTriangleCall(const QString& call_string) const;
  QString FormationTypeToString(FormationType type) const;
  QString TriangleCallToString(TriangleCall call) const;
  
  void CleanupOldData();
  void SendHeartbeat();
  void HandleConnectionError(const QString& error);
  void AttemptReconnection();
  
  double CalculateFormationConfidence(const QJsonObject& mel_data) const;
  TriangleCall DetermineTriangleCall(const FormationData& formation) const;
  int CalculateAlertPriority(const QString& alert_type, const FormationData& formation) const;

  // Configuration
  QString supabase_url_;
  QString api_key_;
  QString websocket_url_;
  bool real_time_enabled_;
  int sync_interval_ms_;
  int cache_retention_hours_;
  int max_cached_formations_;
  
  // Network components
  QNetworkAccessManager* network_manager_;
  QWebSocket* websocket_;
  QNetworkReply* current_reply_;
  
  // Database
  QSqlDatabase cache_db_;
  QString db_connection_name_;
  
  // Timers
  QTimer* sync_timer_;
  QTimer* cache_cleanup_timer_;
  QTimer* heartbeat_timer_;
  
  // Data caches
  mutable QMutex cache_mutex_;
  QMap<qint64, FormationData> formation_cache_; // timestamp -> formation
  QMap<QString, CoachingAlert> alert_cache_;    // alert_id -> alert
  MELResult latest_mel_results_;
  QJsonObject pipeline_status_;
  
  // Synchronization state
  bool is_connected_;
  bool is_initialized_;
  qint64 current_video_timestamp_;
  qint64 last_sync_timestamp_;
  qint64 last_data_fetch_;
  bool video_playing_;
  double video_rate_;
  
  // Statistics
  struct SyncStats {
    qint64 formations_processed;
    qint64 alerts_processed;
    qint64 sync_operations;
    qint64 network_requests;
    qint64 cache_hits;
    qint64 cache_misses;
    QDateTime start_time;
    
    SyncStats() : formations_processed(0), alerts_processed(0), sync_operations(0),
                  network_requests(0), cache_hits(0), cache_misses(0),
                  start_time(QDateTime::currentDateTime()) {}
  } stats_;
  
  // Connection resilience
  int reconnection_attempts_;
  QTimer* reconnect_timer_;
  qint64 last_heartbeat_;
  bool heartbeat_received_;
};

/**
 * @brief Helper class for M.E.L. pipeline data processing
 */
class MELPipelineProcessor : public QObject
{
  Q_OBJECT

public:
  explicit MELPipelineProcessor(QObject* parent = nullptr);

  /**
   * @brief Process M.E.L. stage results
   */
  MELResult ProcessMELData(const QJsonObject& making_data,
                          const QJsonObject& efficiency_data,
                          const QJsonObject& logical_data);

  /**
   * @brief Calculate combined M.E.L. score
   */
  double CalculateCombinedScore(const MELResult& result) const;

  /**
   * @brief Determine formation type from M.E.L. results
   */
  FormationType DetermineFormationType(const MELResult& result,
                                      const QJsonObject& formation_context) const;

  /**
   * @brief Generate coaching recommendations
   */
  QJsonObject GenerateCoachingRecommendations(const FormationData& formation) const;

private:
  double ParseMELScore(const QJsonObject& stage_data, const QString& score_field) const;
  QString DetermineMELStageStatus(const QJsonObject& stage_data) const;
  QJsonObject ExtractDetailedMetrics(const QJsonObject& stage_data) const;
};

/**
 * @brief Utility functions for Triangle Defense processing
 */
namespace TriangleDefenseUtils {

/**
 * @brief Calculate time-based formation similarity
 */
double CalculateFormationSimilarity(const FormationData& f1, const FormationData& f2);

/**
 * @brief Interpolate formation data between timestamps
 */
FormationData InterpolateFormation(const FormationData& before, const FormationData& after,
                                  qint64 target_timestamp);

/**
 * @brief Validate formation data consistency
 */
bool ValidateFormationData(const FormationData& formation);

/**
 * @brief Convert video timestamp to game clock
 */
QString VideoTimestampToGameClock(qint64 video_timestamp, qint64 game_start_timestamp);

/**
 * @brief Extract hash position from field coordinates
 */
QString DetermineHashPosition(const QJsonObject& field_context);

/**
 * @brief Calculate defensive urgency score
 */
double CalculateDefensiveUrgency(const FormationData& formation);

} // namespace TriangleDefenseUtils

} // namespace olive

Q_DECLARE_METATYPE(olive::FormationType)
Q_DECLARE_METATYPE(olive::TriangleCall)
Q_DECLARE_METATYPE(olive::FormationData)
Q_DECLARE_METATYPE(olive::CoachingAlert)
Q_DECLARE_METATYPE(olive::MELResult)

#endif // TRIANGLEDEFENSESYNC_H
